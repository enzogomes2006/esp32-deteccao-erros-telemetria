# Detecção de erro em telemetria sob ruído real — ESP32

Enzo Aurélio · IFMT - Octayde · TCD · Professor Ruy Oliveira.

Entrega da série **CORPO64**, coletada em 19/10/2026. Três ESP32 realizam transmissão UART, recepção com checagem e injeção física de ruído. Uma técnica por rodada: SUM8 ou CRC-8/SMBUS.

## Materiais da entrega

| Material | Arquivo ou pasta |
| --- | --- |
| Relatório explicado, tabelas e análise dos casos | [Relatorio_Final_ESP32.pdf](Relatorio_Final_ESP32.pdf) |
| Protocolo, enquadramento, sequência e timeouts | [ESPECIFICACAO_PROTOCOLO.md](ESPECIFICACAO_PROTOCOLO.md) |
| Três programas e seus headers necessários | [firmware/](firmware/) |
| Sete coletas, CSVs e tabelas | [dados/](dados/) |
| Tempos TX e recuperação após perda | [evidencias/](evidencias/ORIGEM.md) |
| Ligações, resistores e transistor | [montagem/](montagem/README.md) |
| Coletor e conferência dos registros | [coletor/](coletor/LEIA_PRIMEIRO.md) |
| Roteiro de defesa e predição | [APRESENTACAO_20_MIN.md](APRESENTACAO_20_MIN.md) |

## Gravar os programas

Baixe o repositório completo em **Code → Download ZIP** e extraia. Use **ESP32 Dev Module / WROOM clássico**, CPU **240 MHz** e **Arduino-ESP32 3.3.12**. Abra cada `.ino` pela própria pasta, mantendo seus `.h` junto dele:

- TX: [firmware/tx/tx.ino](firmware/tx/tx.ino), GPIO17 na linha de transmissão.
- RX: [firmware/rx/rx.ino](firmware/rx/rx.ino), GPIO16 na linha perturbada.
- RUÍDO: [firmware/noise/noise.ino](firmware/noise/noise.ino), GPIO16 na linha limpa, GPIO18 na perturbada e GPIO19 no resistor de base do BC547.

A [montagem documentada](montagem/README.md) usa GND comum e dois resistores de 1 kΩ. O injetor sorteia alvos nos 64 bits do corpo (LEN, sequência, payload e CHK), preservando marcador e cópia de depuração. Seu pulso só força 1 para 0; um alvo já zero pode não mudar. A base de tempo é a captura de hardware MCPWM.

## Executar uma rodada

1. Deixe TX parado com `x`. Monitores USB: **115200 baud / New Line**.
2. Feche os Monitores Seriais do RX e do RUÍDO e abra **coletor/Abrir_Coletor.cmd** na pasta extraída.
3. Selecione portas, técnica, modo e 10 ou 1000 quadros. Clique **Preparar e gravar**.
4. Aguarde **PRONTO E GRAVANDO**. No Monitor Serial do TX, envie `s` para SUM8 ou `c` para CRC8; depois `t` para dez quadros ou `g` para mil.
5. Aguarde salvar e confira `revisao.txt`. A pasta é criada em coletor/Coletas/.

Para o primeiro controle, selecione SUM8, modo 0, dez quadros. Espera-se 10 OK, zero nas outras classes, 80 amostras do injetor e `rodada_valida=SIM`. As contagens com ruído variam; os valores medidos no relatório não são previsão exata de outra execução.

| Placa | Comandos pelo Monitor Serial |
| --- | --- |
| TX | `x` parar; `s` SUM8; `c` CRC8; `t` dez; `g` mil |
| RX | `x` cancelar; `s` SUM8; `c` CRC8; `d` preparar dez; `r` mil; `a` dez com pausa |
| RUÍDO | `0` desligado; `1` um alvo; `2` dois distintos; `3` rajada; `4` controle extra SUM8 |

No modo manual, feche o coletor e prepare RX antes de iniciar TX. Exemplo SUM8/0/dez: RUÍDO `0`; RX `x`, `s`, `d`; TX `s`, `t`. Envie um comando por vez. Para CRC8, use `c` no RX e no TX.

## Resultados para a defesa

As sete rodadas somam 7.000 quadros. SUM8/modo 2 registrou 289 OK, 619 CORROMPIDOS, 90 PERDIDOS e 2 NÃO DETECTADOS. A taxa ND é **2/(619+2) = 0,3221%**. Os casos reais são as sequências **415 e 553**, analisadas byte a byte no relatório. CRC8 registrou zero ND nas três condições ensaiadas, sem garantia universal de detecção.

Todas as perdas das sete séries tiveram LEN inválido. O controle separado de recepção desligada registrou pausa de 350,443 ms e retomada 38,691 ms após reativar. Para repetir: TX `x`; RUÍDO `0`; RX `x`, `s`, `a`; TX `s`, `t`. Preserve o log inteiro. Esse controle não entra nas sete rodadas aleatórias.

A defesa inclui prever e justificar os contadores dos próximos dez quadros antes de executar o modo escolhido pelo professor. Use o roteiro de 20 minutos. Entrega e avaliação presencial são atividades do estudante.

## Rastreabilidade e autoria

Os programas e os dados foram preservados na reorganização; o relatório foi ajustado aos caminhos atuais. O manifesto registra hashes dos arquivos. Versões anteriores e documentos de preparação permanecem no histórico Git; a versão anterior à limpeza é o commit `2649ac9`.

A [especificação prévia da revisão CORPO64](https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria/blob/520dca3/revisoes/corpo64/ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md) foi registrada antes de seus ajustes. Isso não comprova a ordem de elaboração do projeto original. Houve assistência de IA na programação, análise e documentação, declarada no relatório. As regras de autoria e defesa da disciplina continuam aplicáveis.

Repositório: https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria
