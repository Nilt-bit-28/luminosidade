#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

/** @brief Le o valor bruto de um KY-018 pelo ADC exposto pelo Linux IIO.
 * Nao converte para lux: isso requer calibracao do sensor.
 */
class SensorLuminosidade {
    std::string caminho_;
public:
    /** @param caminho Caminho completo do arquivo in_voltageX_raw confirmado no kit. */
    explicit SensorLuminosidade(std::string caminho)
        : caminho_(std::move(caminho)) {}

    /** @brief Abre novamente o arquivo para obter uma nova amostra.
     * @return Contagem bruta do ADC.
     * @throws std::runtime_error Se o arquivo nao puder ser lido.
     */
    long ler() const {
        std::ifstream arquivo(caminho_);
        long valor;
        if (!(arquivo >> valor) || valor < 0) {
            throw std::runtime_error("Falha ao ler ADC: " + caminho_);
        }
        return valor;
    }
};

/** @brief Controla a demonstracao, sem variaveis globais. */
class Aplicacao {
public:
    /** @brief Imprime uma leitura a cada 500 ms; Ctrl+C encerra.
     * @param argc Quantidade de argumentos.
     * @param argv Primeiro argumento: caminho completo do ADC.
     * Segundo argumento opcional: numero positivo de amostras.
     */
    static int executar(int argc, char* argv[]) {
        try {
            if (argc < 2 || argc > 3) {
                std::cerr << "Uso: ./luminosidade CAMINHO_ADC [AMOSTRAS]\n";
                return 1;
            }
            long quantidade = 0;
            if (argc == 3) {
                std::size_t fim = 0;
                quantidade = std::stol(argv[2], &fim);
                if (argv[2][fim] != '\0' || quantidade <= 0) {
                    throw std::runtime_error("AMOSTRAS deve ser um inteiro positivo.");
                }
            }
            SensorLuminosidade sensor(argv[1]);
            std::cout << "sensor,valor,unidade\n";
            for (long i = 0; quantidade == 0 || i < quantidade; ++i) {
                std::cout << "KY018," << sensor.ler() << ",contagem_ADC" << std::endl;
                if (quantidade == 0 || i + 1 < quantidade) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
            return 0;
        } catch (const std::exception& erro) {
            std::cerr << "Erro: " << erro.what() << '\n';
            return 1;
        }
    }
};

/** @brief Ponto de entrada; delega a logica para a classe Aplicacao. */
int main(int argc, char* argv[]) {
    return Aplicacao::executar(argc, argv);
}
