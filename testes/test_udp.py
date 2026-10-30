"""Testes do protocolo e da comunicacao real entre processos por UDP."""
import csv
import os
from pathlib import Path
import queue
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import unittest

from servidor_udp import Leitura


class TesteServidor(unittest.TestCase):
    def setUp(self):
        self.pasta = tempfile.TemporaryDirectory(prefix="luminosidade_udp_")
        self.raiz = Path(__file__).resolve().parents[1]
        self.csv = Path(self.pasta.name) / "recebido.csv"
        self.processos = []

    def tearDown(self):
        for processo in self.processos:
            if processo.poll() is None:
                processo.terminate()
            processo.communicate(timeout=5)
        self.pasta.cleanup()

    @staticmethod
    def porta_livre():
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind(("127.0.0.1", 0))
            return s.getsockname()[1]

    def iniciar(self, limite):
        porta = self.porta_livre()
        p = subprocess.Popen([sys.executable, str(self.raiz / "servidor_udp.py"), "--host", "127.0.0.1",
                              "--porta", str(porta), "--csv", str(self.csv), "--max-pacotes", str(limite)],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.processos.append(p)
        fila = queue.Queue()
        threading.Thread(target=lambda: fila.put(p.stdout.readline()), daemon=True).start()
        primeira = fila.get(timeout=10)
        self.assertIn("Servidor UDP escutando", primeira)
        return porta, p

    def test_protocolo_e_rejeicoes(self):
        leitura = Leitura.decodificar(b"v1,teste,KY018,2,321,contagem_ADC,1000\n")
        self.assertEqual((leitura.grupo, leitura.sequencia, leitura.valor, leitura.tempo_ms), ("teste", 2, 321, 1000))
        for dados in [b"", b"v2,teste,KY018,0,1,contagem_ADC,1", b"v1,teste,KY018,0,-1,contagem_ADC,1",
                      b"v1,teste,KY018,0,1,lux,1", b"x" * 513, b"\xff"]:
            with self.subTest(dados=dados), self.assertRaises((ValueError, UnicodeError)):
                Leitura.decodificar(dados)

    def test_servidor_descarta_invalido_e_salva_csv(self):
        porta, p = self.iniciar(1)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.sendto(b"pacote incorreto", ("127.0.0.1", porta))
            s.sendto(b"v1,teste,KY018,0,321,contagem_ADC,0\n", ("127.0.0.1", porta))
        saida, erro = p.communicate(timeout=10)
        self.assertEqual(p.returncode, 0, erro)
        self.assertIn("Pacote descartado", erro)
        self.assertIn("Total: 1", saida)
        with self.csv.open(newline="", encoding="utf-8") as f:
            linhas = list(csv.DictReader(f))
        self.assertEqual(len(linhas), 1)
        self.assertEqual(linhas[0]["valor"], "321")
        self.assertEqual(linhas[0]["grupo"], "teste")
        self.assertTrue(linhas[0]["recebido_em_utc"].endswith("+00:00"))

    def test_csv_existente_nao_e_sobrescrito(self):
        self.csv.write_text("registro anterior\n", encoding="utf-8")
        r = subprocess.run([sys.executable, str(self.raiz / "servidor_udp.py"), "--host", "127.0.0.1",
                            "--porta", str(self.porta_livre()), "--csv", str(self.csv)],
                           capture_output=True, text=True, timeout=10)
        self.assertEqual(r.returncode, 1)
        self.assertIn("ja existe", r.stderr)
        self.assertEqual(self.csv.read_text(), "registro anterior\n")

    def test_aviso_sem_leituras(self):
        porta, p = self.iniciar(1)
        fila = queue.Queue()
        # A segunda linha e o caminho do CSV; a seguinte surge apos 5s sem leituras.
        def aguardar():
            p.stdout.readline()
            fila.put(p.stdout.readline())
        threading.Thread(target=aguardar, daemon=True).start()
        self.assertIn("nenhum pacote valido", fila.get(timeout=10))
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.sendto(b"v1,teste,KY018,0,0,contagem_ADC,0\n", ("127.0.0.1", porta))
        p.communicate(timeout=5)
        self.assertEqual(p.returncode, 0)

    @unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("g++"), "envio C++ requer Linux e g++")
    def test_emissor_cpp_e_receptor_python(self):
        exe = Path(self.pasta.name) / "luminosidade"
        ambiente = os.environ.copy()
        ambiente.pop("LD_LIBRARY_PATH", None)
        subprocess.run(["/usr/bin/g++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pthread",
                        str(self.raiz / "luminosidade.cpp"), "-o", str(exe)],
                       check=True, capture_output=True, text=True, env=ambiente, timeout=30)
        adc = Path(self.pasta.name) / "adc.txt"
        adc.write_text("321\n")
        local = subprocess.run([str(exe), str(adc), "3", "--intervalo-ms", "100"],
                               capture_output=True, text=True, env=ambiente, timeout=10)
        self.assertEqual(local.returncode, 0, local.stderr)
        self.assertEqual(local.stdout.count("KY018,321,contagem_ADC"), 3)
        porta, p = self.iniciar(3)
        envio = subprocess.run([str(exe), str(adc), "3", "--udp", "127.0.0.1", str(porta),
                                "--grupo", "teste_cpp", "--intervalo-ms", "100"],
                               capture_output=True, text=True, env=ambiente, timeout=10)
        self.assertEqual(envio.returncode, 0, envio.stderr)
        _, erro = p.communicate(timeout=10)
        self.assertEqual(p.returncode, 0, erro)
        with self.csv.open(newline="", encoding="utf-8") as f:
            linhas = list(csv.DictReader(f))
        self.assertEqual([r["sequencia"] for r in linhas], ["0", "1", "2"])
        self.assertTrue(all(r["grupo"] == "teste_cpp" and r["valor"] == "321" for r in linhas))
        self.assertLessEqual(int(linhas[0]["tempo_ms"]), int(linhas[-1]["tempo_ms"]))
        for argumentos in [[str(adc), "1", "--udp", "invalido", "5005"],
                           [str(adc), "1", "--grupo", "nome,errado"], [str(adc), "0"],
                           [str(adc), "1", "--udp", "127.0.0.1", "70000"]]:
            r = subprocess.run([str(exe)] + argumentos, capture_output=True, text=True, env=ambiente, timeout=5)
            self.assertEqual(r.returncode, 1)


if __name__ == "__main__":
    unittest.main()
