# Protocolo de luminosidade UDP v1

Uma mensagem CSV ASCII por datagrama IPv4 UDP, com no máximo 512 bytes.
Porta padrão: 5005, configurável nos dois programas. O emissor envia uma
amostra aproximadamente a cada 500 ms por padrão. A pausa é configurável.

```text
v1,grupo_luminosidade,KY018,0,1234,contagem_ADC,0
```

| Posição | Campo | Significado |
| --- | --- | --- |
| 1 | versão | Texto fixo `v1` |
| 2 | grupo | 1 a 32 letras ASCII, números, `_` ou `-` |
| 3 | sensor | Texto fixo `KY018` |
| 4 | sequência | Número da amostra, iniciando em 0 a cada execução |
| 5 | valor | Contagem bruta não negativa do ADC; não é lux |
| 6 | unidade | Texto fixo `contagem_ADC` |
| 7 | tempo_ms | Milissegundos desde o início do objeto emissor, não horário civil |

O C++ finaliza a mensagem com uma quebra de linha LF. O receptor também
aceita uma mensagem sem LF. Os números são inteiros decimais não negativos;
o receptor aceita até 19 dígitos e no máximo 9223372036854775807. O emissor
usa o intervalo do tipo `long` da plataforma para a leitura do ADC.

O servidor valida os campos e descarta datagramas inválidos sem encerrar.
Ele acrescenta ao CSV o horário de recebimento UTC e o IP de origem:

```text
recebido_em_utc,ip_origem,grupo,sensor,sequencia,valor,unidade,tempo_ms
```

O arquivo é atualizado a cada leitura. Cada sessão cria um CSV novo; um
nome informado explicitamente com `--csv` não é sobrescrito se já existir.

UDP não garante entrega, ordem ou ausência de duplicatas. O servidor
registra na ordem de chegada e não elimina duplicatas. Sequências permitem
analisar lacunas posteriormente; reiniciar o emissor reinicia a sequência.
Enviar com sucesso no C++ significa apenas que o sistema aceitou o
datagrama, não que o receptor confirmou a leitura. Não há ACK, autenticação
ou criptografia neste protocolo de laboratório em rede local.

Se passar cinco segundos sem leituras válidas, o servidor mostra um aviso e
continua aguardando. Ao receber novamente, retoma a exibição normalmente.
