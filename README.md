# Detecção de erro em telemetria com ESP32

Enzo Aurélio · Luís Fernando · Vinicius Campos
IFMT - Octayde · Transmissão e Comunicação de Dados · Professor Ruy Oliveira

Três ESP32 formam um enlace UART a 115200 baud. TX envia o quadro, RX verifica SUM8 ou CRC-8 e a terceira placa injeta ruído físico. As sete coletas de 19/10/2026 têm mil quadros por condição.

Enzo Aurélio ficou responsável pelo TX, Vinicius Campos pelo RX e Luís Fernando pelo injetor de ruído. As principais dificuldades da bancada foram ajustar o ruído e organizar os cabos para deixar as ligações visíveis e claras.

## Arquivos da entrega

| Conteúdo | Onde abrir |
| --- | --- |
| Relatório final em PDF | [Relatorio_Final_ESP32.pdf](Relatorio_Final_ESP32.pdf) |
| Formato, algoritmos, sequência e timeouts | [ESPECIFICACAO_PROTOCOLO.md](ESPECIFICACAO_PROTOCOLO.md) |
| TX, RX e injetor, com os headers necessários | [firmware/](firmware/) |
| CSVs, logs, metadados e tabelas das duas técnicas | [dados/](dados/README.md) |
| Tempos de cálculo e recuperação | [evidencias/](evidencias/ORIGEM.md) |
| Esquema elétrico e mapa dos fios | [montagem/](montagem/README.md) |
| Coletor e revisão dos registros no Windows | [coletor/](coletor/LEIA_PRIMEIRO.md) |

## Gravação e execução

Baixe em **Code → Download ZIP** e extraia antes de abrir os programas. Na Arduino IDE use **ESP32 Dev Module / WROOM clássico**, CPU **240 MHz**, Arduino-ESP32 **3.3.12**. Abra cada `.ino` na própria pasta e mantenha os arquivos `.h` ao lado. As ligações estão no guia de montagem.

1. Pare o TX enviando `x`. Use os Monitores Seriais em **115200 baud / New Line**.
2. Feche os monitores de RX e RUÍDO e abra `coletor/Abrir_Coletor.cmd`.
3. Escolha as portas, a técnica, o modo e 10 ou 1000 quadros. Clique **Preparar e gravar**.
4. Espere **PRONTO E GRAVANDO**. No TX, envie `s` para SUM8 ou `c` para CRC8; depois `t` para dez ou `g` para mil.
5. Aguarde salvar e confira `revisao.txt` na pasta criada em `coletor/Coletas/`.

O primeiro controle é SUM8, modo 0, dez quadros: espera-se 10 OK e 80 amostras do injetor. As instruções de revisão sem conectar as placas estão em coletor/LEIA_PRIMEIRO.md.

| Placa | Comandos USB |
| --- | --- |
| TX | `x` parar; `s` SUM8; `c` CRC8; `t` dez; `g` mil |
| RX | `x` cancelar; `s` SUM8; `c` CRC8; `d` preparar dez; `r` mil; `a` dez com pausa |
| RUÍDO | `0` sem pulsos; `1` um alvo; `2` dois alvos; `3` rajada; `4` controle extra SUM8 |

Para usar os três monitores manualmente, feche o coletor e prepare RX antes de iniciar TX. Exemplo SUM8/0/dez: RUÍDO `0`; RX `x`, `s`, `d`; TX `s`, `t`, um comando por vez. Para o controle de recuperação, use `a` no lugar de `d`, com ruído em zero.

## Resultados e demonstração

SUM8/modo 2 registrou **2 erros não detectados entre 621 erros checados (0,3221%)**. São as sequências **415 e 553**, com o cálculo e os bytes no relatório. CRC8 registrou zero ND nas condições ensaiadas. Todas as perdas das sete séries foram por LEN inválido. Os dados completos ficam em dados/ e as contagens em [TABELAS.csv](dados/TABELAS.csv).

Na demonstração de 20 minutos, cada integrante precisa explicar TX, RX e RUÍDO. Antes do cenário escolhido pelo professor, cada um prevê e justifica os contadores dos próximos dez quadros usando as tabelas; depois compara com o RX. Novos sorteios não têm contagens exatas garantidas.

O modo 4 é um controle dirigido: mira os bits de valor 128 em P0 e P1. Se ambos mudarem, a soma diminui 256 e o SUM8 mantém resíduo zero com o payload alterado. Confirme o efeito nos logs. Esse controle é separado das séries aleatórias dos modos 1 a 3.

## Autoria e apoio

Houve assistência de IA na programação e análise.

Repositório: https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria
