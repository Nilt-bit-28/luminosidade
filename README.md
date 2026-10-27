Alunos Afonseca - Garcia - 1º TEN MS PQD Rivero 
# Monitoramento de luminosidade — STM32MP1 e KY-018

Projeto de Programação Aplicada: leitura de luminosidade para identificar entrada de luz no invólucro de uma carga. O programa C++ roda no Linux do kit STM32MP1, lê o ADC pela interface IIO e mostra contagens brutas no terminal em formato CSV.

## Estado da entrega em 26/10/2026

- Classe de leitura implementada, com comentários Doxygen e sem variáveis globais.
- Compilação cruzada realizada com a toolchain do professor.
- Executável transferido pela Ethernet e executado com sucesso no kit real.
- O grupo confirmou a visualização das leituras pelo PuTTY.
- Os valores são contagens do ADC, não lux. A estabilidade, a resposta à luz e a calibração ainda precisam de avaliação.
- Alarme de abertura, envio UDP e interface gráfica são etapas futuras.

O enunciado refere-se ao kit DK1. A imagem Linux do kit desta sessão contém `stm32mp157c-dk2.dtb`, utilizado no teste. Esse nome não deve ser aplicado automaticamente a outro kit ou imagem.

## Circuito e alimentação

Montar com a placa desligada e sem cabos USB. O enunciado pede enviar a foto da montagem desligada antes dos testes energizados.

| KY-018 | Kit |
| --- | --- |
| S, saída analógica | A4, CN17, pino 5 |
| Positivo, pino central no módulo utilizado | 3V3, CN16, pino 4 |
| Negativo, marcado com `-` | GND, CN16, pino 7 |

Conferir as marcações reais do módulo. Alimentar o sensor com **3,3 V**, nunca com 5 V. O módulo já contém o divisor resistivo.

Alimentar o kit em **PWR_IN** com fonte USB-C adequada de **5 V / 3 A** e cabo compatível. A alimentação inicial pelo notebook foi detectada como 500 mA e gerou aviso de corrente insuficiente. Manter o cartão SD e as chaves na configuração recebida.

## Arquivos e classes

| Arquivo | Função |
| --- | --- |
| `luminosidade.cpp` | Classes, comentários Doxygen e ponto de entrada |
| `compilar.sh` | Compila para Linux ARM |
| `ambiente.sh` | Configura o caminho do SDK e suas bibliotecas |
| `cxx.sh` | Permite ao editor consultar o compilador no ambiente correto |
| `testar_no_ubuntu.sh` | Teste local com arquivo simulado |
| `.vscode/` | Tarefa de compilação e configuração C++ do editor |
| `Doxyfile` | Configuração opcional de geração da documentação |
| `docs/teste_na_placa_2026-10-26.txt` | Registro do teste real |
| `build/` | Arquivos gerados, ignorados pelo Git |

**SensorLuminosidade:** guarda privadamente o caminho do arquivo ADC. Seu construtor recebe o caminho; `long ler() const` abre o arquivo, lê e retorna a contagem. Falha de leitura ou valor negativo gera `std::runtime_error`. O arquivo é reaberto a cada chamada e fechado automaticamente.

**Aplicacao:** o método estático `executar(argc, argv)` verifica argumentos, cria o sensor, solicita leituras e imprime resultados. Entre as leituras há uma pausa de 500 ms; após a última amostra limitada não há pausa. Exceções são capturadas e mostradas. Retorno 0 indica sucesso; retorno 1 indica falha.

**main():** apenas delega a execução à classe Aplicacao.

## Dependências

- Linux no computador de desenvolvimento: utilizado Ubuntu 24.04 no WSL 2.
- Bash e ferramentas básicas do Linux.
- SDK `arm-buildroot-linux-gnueabihf_sdk-DK2.tar.gz` fornecido pelo professor, com compilador, bibliotecas e sysroot.
- Linux do kit com canais ADC habilitados no IIO.
- Opcional: VS Code com extensão WSL; g++ local para simulação; Doxygen para gerar HTML; OpenSSH/SCP para transferir pela rede.

O SDK fica fora da pasta do projeto e não deve ser enviado ao Git.

## Abrir no VS Code pelo Ubuntu

No terminal Ubuntu deste computador:

```sh
cd /home/felipe/stm32/luminosidade
code .
```

Conferir `WSL: Ubuntu-24.04` no canto inferior esquerdo. Os arquivos nessa pasta são os utilizados para desenvolver. O ZIP entregue é uma cópia de backup.

## Preparar o SDK em outro computador

Extrair o pacote original pelo Linux, preservando permissões e links. Após posicionar o SDK no local definitivo, executar `relocate-sdk.sh` na raiz dele, conforme o manual do Buildroot.

Na instalação deste computador, a extração pelo Windows omitiu links simbólicos e o pacote apresentava diretórios `usr` repetidos. Na cópia Linux utilizada foram restaurados os links do pacote original e executada a relocação. Se esse problema ocorrer novamente, corrigir a extração antes de compilar; não trocar a versão da toolchain sem verificar a compatibilidade com o kit.

## Compilar para a placa

O SDK é procurado por padrão em `$HOME/stm32/sdk`.

```sh
cd /home/felipe/stm32/luminosidade
bash compilar.sh
```

Para outro local de instalação:

```sh
STM32_SDK=/caminho/completo/do/sdk bash compilar.sh
```

O script verifica o compilador e o sysroot, compila em um arquivo temporário e só substitui `build/luminosidade` após sucesso. Se a compilação falhar, corrigir o erro antes de transferir; não enviar um executável antigo por engano.

No VS Code, **Ctrl+Shift+B** chama essa mesma tarefa. Apenas compila: não envia nem executa na placa.

O resultado é um executável ELF Linux ARM de 32 bits, que não executa diretamente no Windows nem no Ubuntu x86 do notebook.

## Acesso ao kit e transferência

O Micro-USB ST-LINK fornece o terminal serial, separado da alimentação PWR_IN. No PuTTY: selecionar a porta COM reconhecida pelo Windows, com 115200 baud, 8 bits, sem paridade, 1 stop bit e sem controle de fluxo. Usar as credenciais fornecidas pelo professor.

Na Ethernet direta testada:

| Equipamento | IPv4 | Máscara |
| --- | --- | --- |
| Notebook, adaptador Ethernet | 192.168.42.10 | 255.255.255.0 |
| STM32, eth0 | 192.168.42.2 | 255.255.255.0 |

Não foi necessário gateway. O ajuste de endereço aplicado no Windows foi temporário e pode precisar ser repetido após reiniciar. Em outra rede, verificar os endereços reais.

No terminal Ubuntu do notebook, dentro do projeto:

```sh
scp -O build/luminosidade root@192.168.42.2:/root/luminosidade
```

`-O` usa o protocolo SCP tradicional, compatível com o serviço Dropbear do kit quando não há SFTP. Na sessão testada, foi utilizado PSCP no Windows, pelo mesmo protocolo SCP.

No **terminal da placa**, não no Ubuntu do notebook:

```sh
chmod +x /root/luminosidade
/root/luminosidade /sys/bus/iio/devices/iio:device0/in_voltage13_raw
```

Ctrl+C encerra. Para obter somente seis leituras:

```sh
/root/luminosidade /sys/bus/iio/devices/iio:device0/in_voltage13_raw 6
```

## Canal ADC e saída

Canal utilizado: `/sys/bus/iio/devices/iio:device0/in_voltage13_raw`.

O canal foi selecionado comparando leituras cobertas e iluminadas. Na avaliação inicial houve variações e valores próximos de zero. A execução do código no kit foi confirmada, mas isso não comprova exatidão ou calibração do sensor.

Em outro kit ou imagem, listar e comparar os canais:

```sh
for f in /sys/bus/iio/devices/iio:device*/in_voltage*_raw; do
    [ -f "$f" ] || continue
    echo "$f"
    cat "$f"
done
```

Não presumir que A4 significa `in_voltage4_raw`. A saída CSV contém sensor, valor e unidade. Amostra real registrada:

```text
sensor,valor,unidade
KY018,40,contagem_ADC
KY018,0,contagem_ADC
KY018,0,contagem_ADC
KY018,0,contagem_ADC
KY018,0,contagem_ADC
KY018,182,contagem_ADC
```

Essa é uma saída local de terminal, ainda sem transmissão UDP.

## Teste no Ubuntu sem a placa

Requer g++ local, já instalado neste computador:

```sh
bash testar_no_ubuntu.sh
```

O mesmo código é compilado para o notebook. Um arquivo com 1234 simula a origem da leitura e são solicitadas três amostras. O programa local é `build/luminosidade_ubuntu`, separado do executável ARM.

```sh
echo 2800 > build/adc_simulado.txt
./build/luminosidade_ubuntu build/adc_simulado.txt 3
```

Também foram verificados arquivos ausentes, vazios, com texto e com número negativo: o programa sinalizou falha. Esse teste não mede luz.

## Doxygen

Os comentários no código descrevem classes, métodos, parâmetros, retornos e exceções. Com Doxygen instalado:

```sh
doxygen Doxyfile
```

A documentação pronta está incluída no repositório em `docs/doxygen/html/`, junto do código comentado e do Doxyfile.

Para visualizar, abrir `docs/doxygen/html/index.html` no navegador. A página inicial apresenta o projeto; o menu de classes permite consultar SensorLuminosidade, Aplicacao, seus métodos, parâmetros, retornos e exceções.

No GitHub, os arquivos HTML não são exibidos automaticamente como um site. Usar Code → Download ZIP, extrair a pasta completa e abrir o arquivo index.html no navegador. Manter os demais arquivos da pasta HTML juntos, pois contêm estilos e navegação.

Após mudar comentários ou o README, executar novamente `doxygen Doxyfile` e incluir os arquivos atualizados no próximo commit.

## Inicialização manual utilizada no teste

O boot automático não iniciou o Linux e parou em `STM32MP>`. O cartão permitiu listar a partição 4 e ler o núcleo e o arquivo de hardware. A seguinte sequência funcionou nesse kit:

```text
load mmc 0:4 ${kernel_addr_r} /boot/zImage
load mmc 0:4 ${fdt_addr_r} /boot/stm32mp157c-dk2.dtb
part uuid mmc 0:4 rootuuid
setenv bootargs console=ttySTM0,115200 root=PARTUUID=${rootuuid} rootwait rw
bootz ${kernel_addr_r} - ${fdt_addr_r}
```

Executar somente no U-Boot, não no terminal Linux. Os parâmetros foram temporários: não foi usado `saveenv` nem aplicada correção permanente do boot. Pode ser necessário repetir após ligar. Não foi comprovado defeito no cartão SD.

## Desligamento

Interromper as leituras com Ctrl+C. No terminal da placa:

```sh
poweroff
```

Aguardar a mensagem final de desligamento antes de retirar os cabos.

## Entrega pelo Git

Versionar código, scripts, README, Doxyfile, configurações do editor e evidências. Não enviar a pasta build nem o SDK.

O repositório local foi preparado na pasta do projeto. Configurar o nome e e-mail reais do autor, criar o commit e publicar no GitHub. Se o repositório for privado, garantir acesso ao professor e aos integrantes.

Conforme o enunciado, informar no Classroom o link do **commit final**. Conferir os arquivos publicados e acrescentar uma captura do terminal real e a foto da montagem do grupo quando disponíveis.

## Referências

- Enunciado de Projeto de Programação Aplicada fornecido pelo professor.
- [ST: interface IIO](https://wiki.st.com/stm32mpu/wiki/How_to_use_the_IIO_user_space_interface).
- [Buildroot: SDK e relocação](https://buildroot.org/downloads/manual/manual.html).
- [VS Code com WSL](https://code.visualstudio.com/docs/remote/wsl).
- [GitHub: publicar código local](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github).
