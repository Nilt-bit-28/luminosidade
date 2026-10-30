/** @file luminosidade.cpp
 * @brief Leitura do KY-018 pelo Linux IIO, exibicao local e envio opcional UDP.
 */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

/** @brief Le o ADC; a contagem bruta ainda nao e uma medida em lux. */
class SensorLuminosidade {
    std::string caminho_; ///< Arquivo in_voltageX_raw do canal confirmado no kit.
public:
    /** @brief Guarda o caminho do ADC. @param caminho Arquivo da leitura bruta. */
    explicit SensorLuminosidade(std::string caminho) : caminho_(std::move(caminho)) {}

    /** @brief Obtem uma nova amostra.
     * @return Contagem bruta nao negativa.
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

/** @brief Valida e encapsula os argumentos da linha de comando. */
class Configuracao {
    std::string caminho_; ///< Caminho do ADC.
    std::string ip_; ///< IPv4 do servidor; vazio desativa o envio UDP.
    std::string grupo_ = "grupo_luminosidade"; ///< Identificador do grupo.
    long amostras_ = 0; ///< Zero significa execucao continua.
    int porta_ = 5005; ///< Porta UDP do receptor.
    int intervalo_ = 500; ///< Pausa entre amostras, em milissegundos.

    /** @brief Converte um inteiro decimal e verifica seus limites. */
    static long numero(const std::string& texto, long minimo, long maximo) {
        if (texto.empty() || texto.find_first_not_of("0123456789") != std::string::npos) {
            throw std::runtime_error("Numero invalido: " + texto);
        }
        const long valor = std::stol(texto);
        if (valor < minimo || valor > maximo) {
            throw std::runtime_error("Numero fora dos limites: " + texto);
        }
        return valor;
    }
public:
    /** @brief Interpreta argumentos sem alterar o modo local antigo.
     * @param argc Quantidade de argumentos.
     * @param argv Nome, caminho ADC, quantidade opcional e opcoes.
     * @throws std::exception Se algum argumento for invalido.
     */
    Configuracao(int argc, char* argv[]) {
        if (argc < 2) throw std::runtime_error("Informe o caminho do ADC. Use --help para ajuda.");
        caminho_ = argv[1];
        bool recebeu_amostras = false;
        for (int i = 2; i < argc; ++i) {
            const std::string argumento = argv[i];
            if (argumento == "--udp") {
                if (i + 2 >= argc || !ip_.empty()) {
                    throw std::runtime_error("Use --udp IPV4 PORTA uma unica vez.");
                }
                ip_ = argv[++i];
                porta_ = static_cast<int>(numero(argv[++i], 1, 65535));
            } else if (argumento == "--grupo") {
                if (++i >= argc) throw std::runtime_error("Informe o identificador apos --grupo.");
                grupo_ = argv[i];
                if (grupo_.empty() || grupo_.size() > 32 ||
                    grupo_.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) {
                    throw std::runtime_error("Grupo: use 1 a 32 letras ASCII, numeros, _ ou -.");
                }
            } else if (argumento == "--intervalo-ms") {
                if (++i >= argc) throw std::runtime_error("Informe o intervalo em ms.");
                intervalo_ = static_cast<int>(numero(argv[i], 100, 60000));
            } else if (!recebeu_amostras && argumento.compare(0, 2, "--") != 0) {
                amostras_ = numero(argumento, 1, 1000000000);
                recebeu_amostras = true;
            } else {
                throw std::runtime_error("Argumento desconhecido: " + argumento);
            }
        }
    }
    /** @return Caminho do ADC. */
    const std::string& caminho() const { return caminho_; }
    /** @return IPv4 do servidor ou texto vazio no modo local. */
    const std::string& ip() const { return ip_; }
    /** @return Identificador do grupo. */
    const std::string& grupo() const { return grupo_; }
    /** @return Quantidade solicitada; zero significa sem limite. */
    long amostras() const { return amostras_; }
    /** @return Porta UDP do servidor. */
    int porta() const { return porta_; }
    /** @return Pausa em milissegundos. */
    int intervalo() const { return intervalo_; }
};

/** @brief Gerencia o socket UDP e serializa as leituras no protocolo v1. */
class EnviadorUDP {
    int socket_ = -1; ///< Descritor do socket, fechado automaticamente.
    sockaddr_in destino_{}; ///< Endereco IPv4 e porta do servidor.
    std::string grupo_; ///< Identificador de quem envia a leitura.
    std::chrono::steady_clock::time_point inicio_; ///< Inicio do processo de envio.
public:
    /** @brief Abre o socket para um IPv4 numerico e porta validos.
     * @param ip IPv4 do notebook servidor.
     * @param porta Porta UDP entre 1 e 65535.
     * @param grupo Identificador ja validado pela Configuracao.
     * @throws std::runtime_error Se o endereco ou o socket forem invalidos.
     */
    EnviadorUDP(const std::string& ip, int porta, std::string grupo)
        : grupo_(std::move(grupo)), inicio_(std::chrono::steady_clock::now()) {
        destino_.sin_family = AF_INET;
        destino_.sin_port = htons(static_cast<unsigned short>(porta));
        if (inet_pton(AF_INET, ip.c_str(), &destino_.sin_addr) != 1) {
            throw std::runtime_error("IPv4 invalido para o servidor: " + ip);
        }
        socket_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) throw std::runtime_error("Falha ao abrir socket UDP: " + std::string(std::strerror(errno)));
    }
    /** @brief Fecha o socket ao destruir o objeto. */
    ~EnviadorUDP() { if (socket_ >= 0) close(socket_); }
    EnviadorUDP(const EnviadorUDP&) = delete; ///< Evita duplicar o descritor.
    EnviadorUDP& operator=(const EnviadorUDP&) = delete; ///< Evita copiar o recurso.

    /** @brief Envia uma leitura em um unico datagrama CSV ASCII.
     * @param valor Contagem bruta do ADC.
     * @param sequencia Numero da amostra, com inicio em zero.
     * @throws std::runtime_error Se o sistema nao aceitar o datagrama.
     * Enviar com sucesso nao confirma recebimento pelo servidor: UDP nao tem ACK.
     */
    void enviar(long valor, std::uint64_t sequencia) const {
        const auto tempo = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - inicio_).count();
        const std::string mensagem = "v1," + grupo_ + ",KY018," +
            std::to_string(sequencia) + "," + std::to_string(valor) +
            ",contagem_ADC," + std::to_string(tempo) + "\n";
        const ssize_t enviados = sendto(socket_, mensagem.data(), mensagem.size(), 0,
            reinterpret_cast<const sockaddr*>(&destino_), sizeof(destino_));
        if (enviados < 0) throw std::runtime_error("Falha UDP: " + std::string(std::strerror(errno)));
        if (static_cast<std::size_t>(enviados) != mensagem.size()) {
            throw std::runtime_error("Falha UDP: datagrama incompleto.");
        }
    }
};

/** @brief Controla a leitura, a exibicao local e o envio opcional. */
class Aplicacao {
public:
    /** @brief Apresenta o modo local e as opcoes de rede. */
    static void ajuda() {
        std::cout << "Uso: ./luminosidade CAMINHO_ADC [AMOSTRAS] [--udp IPV4 PORTA]\n"
                     "       [--grupo IDENTIFICADOR] [--intervalo-ms 100..60000]\n"
                     "Sem AMOSTRAS, executa continuamente. Ctrl+C encerra.\n"
                     "Sem --udp, apenas mostra as leituras no terminal.\n";
    }
    /** @brief Executa a aplicacao; as falhas de envio nao interrompem a leitura.
     * @param argc Quantidade de argumentos.
     * @param argv Argumentos da linha de comando.
     * @return Zero no termino normal; um na falha de configuracao ou leitura.
     */
    static int executar(int argc, char* argv[]) {
        try {
            if (argc == 2 && std::string(argv[1]) == "--help") { ajuda(); return 0; }
            const Configuracao config(argc, argv);
            SensorLuminosidade sensor(config.caminho());
            std::unique_ptr<EnviadorUDP> enviador;
            if (!config.ip().empty()) {
                enviador.reset(new EnviadorUDP(config.ip(), config.porta(), config.grupo()));
                std::cerr << "Destino UDP: " << config.ip() << ':' << config.porta()
                          << " | grupo: " << config.grupo() << '\n';
            }
            std::cout << "sensor,valor,unidade\n";
            for (std::uint64_t i = 0; config.amostras() == 0 || i < static_cast<std::uint64_t>(config.amostras()); ++i) {
                const long valor = sensor.ler();
                std::cout << "KY018," << valor << ",contagem_ADC" << std::endl;
                if (enviador) {
                    try { enviador->enviar(valor, i); }
                    catch (const std::exception& erro) { std::cerr << "Aviso: " << erro.what() << '\n'; }
                }
                if (config.amostras() == 0 || i + 1 < static_cast<std::uint64_t>(config.amostras())) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(config.intervalo()));
                }
            }
            return 0;
        } catch (const std::exception& erro) {
            std::cerr << "Erro: " << erro.what() << '\n';
            return 1;
        }
    }
};

/** @brief Delega a execucao para Aplicacao.
 * @param argc Quantidade de argumentos. @param argv Argumentos.
 * @return Codigo devolvido pela aplicacao.
 */
int main(int argc, char* argv[]) { return Aplicacao::executar(argc, argv); }
