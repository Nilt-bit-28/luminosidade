# Etapa de servidor — preparação para 02/11/2026

Fluxo: **KY-018 → ADC/Linux da STM32 → programa C++ → Ethernet/UDP →
servidor Python no notebook → terminal e CSV**.

O receptor não precisa de bibliotecas Python externas. Ele funciona no
Windows ou Linux. Nesta etapa, a apresentação é no terminal, sem gráfico.
O protocolo completo está em `PROTOCOLO_UDP.md`.

## O que foi validado

- Compilação cruzada do novo emissor com o SDK do professor.
- Leitura local anterior preservada.
- Troca real de datagramas em localhost entre emissor C++ e servidor Python,
  usando um arquivo simulado no lugar do ADC.
- Servidor testado no Ubuntu e no Python do Windows.
- Validação das mensagens, gravação CSV, preservação de CSV existente e
  aviso de ausência de dados.

O novo envio **STM32 → notebook pela Ethernet foi validado em 29/10/2026**: dez amostras reais foram enviadas e recebidas, com os mesmos valores, e gravadas em CSV. A evidência está em `evidencias_udp_2026-10-29/`. O kit desta sessão foi diferente do teste de 26/10; os canais disponíveis foram conferidos novamente. O teste não constitui calibração do sensor.

O grupo também relatou resposta condizente ao apagar a luz e repetir a medição em 29/10. Trata-se de verificação qualitativa, sem conversão calibrada para lux.

## 1. Preparar a rede

Conectar a STM32 diretamente à Ethernet do notebook. Usar os endereços da
sessão anterior, confirmando que não estão em uso por outros dispositivos:

| Equipamento | IPv4 | Máscara |
| --- | --- | --- |
| Notebook, Ethernet | 192.168.42.10 | 255.255.255.0 |
| STM32, eth0 | 192.168.42.2 | 255.255.255.0 |

Não é necessário gateway nessa conexão direta. A configuração temporária
do Windows usada anteriormente pode ter sido perdida após reiniciar.
No terminal da placa, conferir `ip addr`. No notebook, testar a comunicação.

O servidor deve receber pela interface do **Windows** quando a Ethernet
pertence ao Windows. O modo NAT padrão do WSL 2 não expõe automaticamente
um receptor UDP do Ubuntu à placa externa. O script abaixo usa o Python
Windows ao detectar WSL, sem alterar a configuração do WSL.

## 2. Abrir o servidor no notebook

No terminal Ubuntu, dentro da pasta do projeto:

```sh
cd /home/felipe/stm32/luminosidade
bash iniciar_servidor.sh --host 192.168.42.10 --porta 5005
```

É preciso Python 3 instalado no Windows; ele já existe neste computador.
O servidor anuncia a porta e o caminho do CSV em `logs/`. Cada execução
cria um nome novo com data UTC. Deixar esse terminal aberto.

Alternativa em Linux nativo, fora do WSL:

```sh
python3 servidor_udp.py --host 192.168.42.10 --porta 5005
```

Se o Windows pedir autorização de firewall para o Python, permitir na rede
usada pelo kit. Caso o servidor abra mas não receba, verificar uma regra de
entrada UDP para a porta 5005, limitada à Ethernet e ao IP 192.168.42.2.
Não é necessário desligar o firewall ou liberar outras portas.

Se houver erro de endereço não disponível, conferir o IPv4 do notebook.
Se a porta estiver ocupada, encerrar outro servidor ou escolher outra porta
em ambos os programas. Ctrl+C encerra e mantém o CSV já gravado.

## 3. Compilar e transferir a nova versão

Em outro terminal Ubuntu do projeto:

```sh
bash compilar.sh
scp -O build/luminosidade root@192.168.42.2:/root/luminosidade
```

A nova versão precisa ser copiada: o arquivo antigo na placa não tem UDP.
Caso o WSL não alcance a placa, transferir pelo PSCP do Windows, como no
teste anterior. Não usar o executável local de simulação na STM32.

## 4. Executar na placa pelo PuTTY

Entrar no Linux do kit, repetindo a inicialização manual do README se
necessário. No terminal **da STM32**, executar:

```sh
chmod +x /root/luminosidade
/root/luminosidade /sys/bus/iio/devices/iio:device0/in_voltage13_raw --udp 192.168.42.10 5005 --grupo grupo_luminosidade
```

O destino deve ser o IP do **notebook**, não o IP da STM32. Se o canal ADC
da montagem mudou, substituir o caminho. O programa continua exibindo as
leituras no PuTTY; o servidor deve mostrar os mesmos valores com sequência
e horário de recebimento. Cobrir e iluminar o sensor para comparar.

Para terminar automaticamente após dez amostras:

```sh
/root/luminosidade /sys/bus/iio/devices/iio:device0/in_voltage13_raw 10 --udp 192.168.42.10 5005 --grupo grupo_luminosidade
```

Sem `--udp`, o comando antigo continua válido e somente mostra o ADC.

## 5. Conferir a evidência

- Capturar o terminal da placa executando o emissor.
- Capturar o terminal do notebook recebendo as leituras.
- Abrir o CSV indicado e conferir grupo, sensor, valores e sequência.
- Interromper o emissor e observar o aviso de cinco segundos sem leituras.
- Registrar que os dados são contagens do ADC; não chamar de lux.

Os CSVs ficam ignorados pelo Git para não misturar sessões de testes.
A amostra física validada foi selecionada para `docs/evidencias_udp_2026-10-29/`, com o CSV e o registro textual da execução. Acrescentar capturas próprias dos terminais para a entrega.

## Teste automatizado sem placa

No Ubuntu:

```sh
python3 -m unittest discover -s testes -v
```

O teste de emissor compila o C++ para o notebook e usa sockets UDP reais
em localhost. Os testes de receptor também executam no Windows; o teste
de compilação Linux é pulado automaticamente nessa plataforma.
