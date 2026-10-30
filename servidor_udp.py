"""Receptor UDP do KY-018: terminal e CSV, somente com a biblioteca padrao."""

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
import re
import socket
import sys
import time


@dataclass(frozen=True)
class Leitura:
    """Uma leitura validada do protocolo CSV v1."""

    grupo: str
    sequencia: int
    valor: int
    tempo_ms: int

    @classmethod
    def decodificar(cls, dados):
        """Rejeita versao desconhecida, campos indevidos e numeros invalidos."""
        if len(dados) > 512:
            raise ValueError("pacote maior que 512 bytes")
        texto = dados.decode("ascii").rstrip("\n")
        partes = texto.split(",")
        if len(partes) != 7:
            raise ValueError("esperados sete campos CSV")
        versao, grupo, sensor, sequencia, valor, unidade, tempo_ms = partes
        if versao != "v1" or sensor != "KY018" or unidade != "contagem_ADC":
            raise ValueError("versao, sensor ou unidade desconhecidos")
        if re.fullmatch(r"[A-Za-z0-9_-]{1,32}", grupo) is None:
            raise ValueError("identificador de grupo invalido")
        numeros = []
        for campo in (sequencia, valor, tempo_ms):
            if re.fullmatch(r"[0-9]{1,19}", campo) is None:
                raise ValueError("numero invalido no pacote")
            numero = int(campo)
            if numero > 2**63 - 1:
                raise ValueError("numero fora do limite do protocolo")
            numeros.append(numero)
        return cls(grupo, numeros[0], numeros[1], numeros[2])


class ServidorUDP:
    """Recebe datagramas, valida as leituras e salva cada linha imediatamente."""

    @staticmethod
    def argumentos():
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--host", default="0.0.0.0", help="IPv4 local; padrao: todas as interfaces")
        parser.add_argument("--porta", type=int, default=5005)
        parser.add_argument("--csv", type=Path, help="arquivo novo; por padrao cria um nome com data UTC na pasta logs")
        parser.add_argument("--max-pacotes", type=int, default=0, help="limite de leituras validas; 0 = continuo")
        args = parser.parse_args()
        if args.csv is None:
            nome = datetime.now(timezone.utc).strftime("leituras_%Y%m%d_%H%M%S_%f.csv")
            args.csv = Path("logs") / nome
        if not 1 <= args.porta <= 65535 or args.max_pacotes < 0:
            parser.error("porta deve estar entre 1 e 65535; max-pacotes deve ser nao negativo")
        try:
            socket.inet_pton(socket.AF_INET, args.host)
        except OSError:
            parser.error("--host deve ser um endereco IPv4 numerico")
        return args

    @staticmethod
    def executar():
        args = ServidorUDP.argumentos()
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receptor:
                receptor.bind((args.host, args.porta))
                receptor.settimeout(1.0)
                # Criar um arquivo novo evita misturar cabecalhos e sessoes de testes.
                args.csv.parent.mkdir(parents=True, exist_ok=True)
                with args.csv.open("x", encoding="utf-8", newline="") as arquivo:
                    escritor = csv.writer(arquivo)
                    escritor.writerow(["recebido_em_utc", "ip_origem", "grupo", "sensor", "sequencia", "valor", "unidade", "tempo_ms"])
                    arquivo.flush()
                    print(f"Servidor UDP escutando em {args.host}:{args.porta}", flush=True)
                    print(f"CSV: {args.csv.resolve()} | Ctrl+C encerra", flush=True)
                    ultima_valida = time.monotonic()
                    aviso = False
                    recebidas = 0
                    try:
                        while args.max_pacotes == 0 or recebidas < args.max_pacotes:
                            try:
                                dados, origem = receptor.recvfrom(513)
                            except socket.timeout:
                                if not aviso and time.monotonic() - ultima_valida >= 5:
                                    print("Aguardando leituras: nenhum pacote valido recebido nos ultimos 5 segundos.", flush=True)
                                    aviso = True
                                continue
                            try:
                                leitura = Leitura.decodificar(dados)
                            except (ValueError, UnicodeError) as erro:
                                print(f"Pacote descartado de {origem[0]}: {erro}", file=sys.stderr, flush=True)
                                continue
                            instante = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
                            escritor.writerow([instante, origem[0], leitura.grupo, "KY018", leitura.sequencia,
                                               leitura.valor, "contagem_ADC", leitura.tempo_ms])
                            arquivo.flush()
                            recebidas += 1
                            ultima_valida = time.monotonic()
                            aviso = False
                            print(f"{instante} | {origem[0]} | {leitura.grupo} | amostra {leitura.sequencia} | "
                                  f"KY018 = {leitura.valor} contagem_ADC", flush=True)
                    except KeyboardInterrupt:
                        print("\nRecepcao interrompida pelo usuario.", flush=True)
                    print(f"Total: {recebidas} leituras validas. CSV salvo.", flush=True)
            return 0
        except FileExistsError:
            print(f"Erro: o CSV {args.csv} ja existe. Escolha outro nome com --csv; ele nao sera sobrescrito.", file=sys.stderr)
            return 1
        except OSError as erro:
            print(f"Erro ao abrir a rede ou o CSV: {erro}", file=sys.stderr)
            return 1


if __name__ == "__main__":
    raise SystemExit(ServidorUDP.executar())
