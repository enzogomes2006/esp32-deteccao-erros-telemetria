# Detecção de erros em telemetria sob ruído real

Enzo Aurélio | IFMT - Octayde | TCD - Transmissão e Comunicação de Dados | Professor Ruy Oliveira | Outubro de 2026

Repositório com códigos, dados e documentação: https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria

# RESUMO

Este trabalho compara a detecção de erros por soma modular de oito bits (SUM8) e CRC-8/SMBUS em um enlace UART entre dois ESP32, perturbado por um terceiro microcontrolador. O injetor aciona um transistor NPN para forçar bits do payload a zero e registra o efeito efetivamente observado. Foram realizadas sete rodadas válidas de 1000 quadros: quatro com SUM8, incluindo o controle sem ruído, e três com CRC8. O receptor executa a checagem antes de consultar uma cópia de depuração, utilizada exclusivamente para auditar a integridade do payload. SUM8 apresentou quatro erros não detectados no modo de dois alvos, correspondentes a 0,5083% dos 787 quadros com erro dessa rodada. CRC8 apresentou zero erros não detectados nos modos ensaiados. O tempo de verificação registrado na série foi de 1 µs para SUM8 e 3 µs para CRC8. Em um ensaio complementar de interrupção da recepção, o RX completo retomou a aceitação 153,028 ms após a primeira detecção de perda. Os resultados evidenciam uma limitação da soma modular e a maior cobertura do CRC para as classes de erro utilizadas, sem generalizar a detecção para qualquer padrão de corrupção.

Palavras-chave: ESP32; comunicação UART; checksum; CRC8; detecção de erros.

## 1 INTRODUÇÃO

Sistemas de telemetria dependem da identificação de alterações nos dados transmitidos. Um código de detecção acrescenta redundância ao quadro e permite ao receptor decidir sobre sua aceitação. Entretanto, a aprovação na checagem não garante ausência de erro: determinadas alterações preservam o resíduo esperado.

O objetivo deste estudo foi implementar um protocolo serial, produzir perturbações reais na linha e comparar SUM8 e CRC8 quanto à ocorrência de erros não detectados. Também foram avaliados o custo de cálculo e verificação, o overhead e a recuperação após ausência temporária de recepção. A análise distingue a decisão autônoma do enlace da auditoria posterior dos dados.

# 2 FUNDAMENTAÇÃO TEÓRICA

## 2.1 Soma modular de oito bits

O SUM8 reduz a soma dos bytes protegidos ao resto da divisão por 256. Neste protocolo, o transmissor anexa o complemento modular da soma. Para os sete bytes anteriores ao detector, o valor transmitido é:

CHK = -soma(LEN, seqL, seqH, p0, p1, p2, p3) mod 256.

O receptor soma o corpo completo, incluindo CHK, e aceita apenas se o resto for zero. Essa operação detecta uma alteração isolada de um bit, pois sua contribuição em valor absoluto é uma potência de dois entre 1 e 128. Por outro lado, alterações cuja soma seja múltipla de 256 não modificam o resíduo.

Como o circuito força apenas o nível baixo, apagar o bit mais significativo de dois bytes distintos reduz a soma em 128 + 128 = 256. Esse padrão mantém o resíduo zero e constitui uma colisão do SUM8. A aceitação é indevida quando o payload recebido difere do original.

## 2.2 CRC-8/SMBUS

O CRC opera sobre bits com aritmética polinomial em GF(2), usando operações XOR. Foi adotado o polinômio x^8 + x^2 + x + 1, identificado no SMBus por 1 0000 0111 (SMBUS, s.d.). A implementação utiliza 0x07 sem o termo de grau oito, estado inicial 0x00, sem reflexão e com xorout=0x00.

Os sete bytes anteriores ao CRC representam 56 bits de dados. O catálogo de Koopman e suas notas indicam distância de Hamming 4 para o polinômio completo 0x107 em comprimentos de dados até 119 bits. Assim, para o bloco empregado, erros de um e de dois bits são detectáveis no modelo de substituição de bits (KOOPMAN, 2024a; 2024b).

Qualquer alteração não nula limitada a um único byte também tem extensão de até oito bits e não é divisível por esse gerador de grau oito. Essa propriedade é relevante para o modo de rajada utilizado. A cobertura não equivale à detecção de toda corrupção possível, nem abrange automaticamente perda de bytes ou falhas de enquadramento.

# 3 ESPECIFICAÇÃO DO PROTOCOLO

O enlace de dados é unidirecional, do TX para o RX, em UART2 a 115200 baud, oito bits de dados, sem paridade e um bit de parada (8N1). Não há ACK nem retransmissão. A estrutura permanece igual nas rodadas SUM8 e CRC8; somente a técnica de cálculo de CHK é alterada.

A5 5A C3 3C | 07 seqL seqH p0 p1 p2 p3 CHK

Tabela 1 - Campos do quadro de dados.

| Campo | Tamanho | Descrição |
| --- | --- | --- |
| Marcador | 4 bytes | A5 5A C3 3C; identifica o início e não entra na checagem |
| LEN | 1 byte | Valor 07; conta os sete bytes posteriores a LEN |
| Sequência | 2 bytes | Inteiro sem sinal de 16 bits, byte menos significativo primeiro |
| Payload | 4 bytes | Dados sintéticos de telemetria |
| CHK | 1 byte | SUM8 ou CRC8; uma técnica por rodada |

Fonte: Elaboração própria, a partir da implementação do protocolo (2026).

A checagem protege LEN, sequência e payload. No TX, o SUM8 é calculado como complemento modular da soma dos sete bytes; o CRC8 é calculado sobre esses mesmos bytes, na ordem do corpo e com processamento MSB primeiro dentro de cada byte. No RX, os oito bytes são processados e o resíduo esperado é zero. A ordem de bits do cálculo CRC não altera a transmissão LSB primeiro de cada byte pela UART.

A delimitação por comprimento fixo permite qualquer valor no payload, sem escape de bytes. Ao reconhecer o marcador, o RX coleta oito bytes e exige LEN=7. Um marcador dentro desse corpo é tratado como dado. Concluído ou descartado o corpo, a busca de marcadores recomeça no próximo byte processado.

## 3.1 Referência de depuração

Após o quadro, o TX envia D5 6A B4 4B seguido dos oito bytes originais do corpo. Essa referência contém a sequência para associação e permite validar a própria referência antes de comparar o payload. Sua leitura ocorre depois da decisão ACEITO ou DESCARTADO e não modifica o controle de sequência.

O TX insere pausas de 10 ms após o marcador de dados, 3 ms após cada byte do corpo e 10 ms antes da cópia. Essas pausas mantêm a linha em estado ocioso alto e permitem o processamento do injetor; a velocidade de transmissão dos bytes continua em 115200 baud.

# 3.2 CONTROLE DE RECEPÇÃO E SEQUÊNCIA

A rodada é delimitada por uma janela de 10 ou 1000 posições, com sequência inicial zero nos comandos de operação. O RX calcula o resíduo antes de consultar a referência. Para uma sequência admissível, registra posições anteriores ainda não observadas como perdidas, marca a posição atual como aceita ou descartada e passa a esperar o número seguinte. Um quadro descartado após checagem não é contado novamente como perdido.

As diferenças de sequência são calculadas com aritmética sem sinal de 16 bits. O valor delta = seq_rx - seq_esperada, reduzido a 16 bits, é considerado antigo ou fora de ordem quando delta ≥ 32768. O índice na janela também é calculado em 16 bits; posições fora da janela ou já registradas são ignoradas e diagnosticadas. A referência não é utilizada para corrigir o histórico.

O controle usa a sequência recebida também nos descartes porque o injetor tem como alvos somente os bits do payload. Portanto, a implementação assume que o cabeçalho não é corrompido nesse experimento. Essa premissa é necessária: uma sequência pertencente a um quadro com checagem falha não seria confiável em um canal capaz de alterar o cabeçalho.

Tabela 2 - Prazos de recepção e seus marcos iniciais.

| Situação | Prazo | Tratamento |
| --- | --- | --- |
| Corpo ou cópia parcial | 60 ms desde seu marcador | Abandona o bloco e retorna à busca de marcador |
| Referência pendente | 60 ms desde o marcador de dados | Registra auditoria incompleta e invalida a medição |
| Silêncio inicial do enlace | 250 ms desde o primeiro marcador, reiniciado após cada corpo checado | Declara perdida a próxima posição ainda não observada |
| Silêncio persistente | Avanços nominais de 100 ms por posição | Prossegue pela janela sem duplicar classificações |

Fonte: Elaboração própria, a partir do RX completo v0.2 (2026).

Antes do primeiro marcador de dados, o RX aguarda o início da transmissão e não declara perdas. O timeout de enlace de 250 ms corresponde a aproximadamente 2,48 períodos de 101 ms medidos. O prazo de 60 ms para bloco e auditoria é superior à duração prevista de aproximadamente 45 ms até o fim da referência. Os limites são verificados no loop do firmware.

A passagem de 65535 para zero foi verificada em software; não nas rodadas físicas de 0 a 999.

# 4 MATERIAIS E PROCEDIMENTOS

Foram utilizados três ESP32 clássicos WROOM, um transistor NPN BC547 e dois resistores de 1 kΩ. A saída GPIO17 do TX foi ligada ao GPIO16 do RX por um resistor em série. O coletor do NPN foi conectado ao lado RX da linha e o emissor ao GND comum. A base foi acionada pelo GPIO19 do ESP32 de ruído através do segundo resistor. O injetor observou a linha limpa no GPIO16 e a perturbada no GPIO18.

As placas foram alimentadas individualmente por USB, com os GNDs interligados. O firmware foi compilado para ESP32 Dev Module, CPU de 240 MHz e Arduino-ESP32 3.3.12. A série selecionada empregou o TX preservado, RX completo v0.2, injetor com captura v1.3.1 e coletor CSV v2.

Tabela 3 - Modos de ruído empregados nas sete rodadas.

| Modo | Alvos no payload |
| --- | --- |
| 0 | Sem pulso; controle de recepção |
| 1 | Um bit sorteado entre os 32 bits |
| 2 | Dois bits distintos sorteados entre os 32 bits |
| 3 | Rajada de 3 a 8 bits contíguos dentro de um byte sorteado |

Fonte: Elaboração própria, a partir do injetor e da metodologia experimental (2026).

O transistor força somente 1 para 0. Um alvo que já era zero não provoca alteração. A captura de borda em hardware, por MCPWM0, fornece a referência temporal para os pulsos e amostras, sem delayMicroseconds(). Os registros contêm máscara alvo, byte antes, byte depois, alteração efetiva e diagnósticos de sincronismo e atraso. O marcador, LEN, sequência, CHK e cópia não são alvos.

Foram coletados 1000 quadros para SUM8 nos modos 0, 1, 2 e 3, e outros 1000 para CRC8 nos modos 1, 2 e 3. Os payloads e alvos foram sorteados novamente a cada execução. O TX fixa o bit mais significativo de p0 e p1 em 1 para permitir um padrão adicional de demonstração, condição mantida em todas as rodadas.

O coletor salvou simultaneamente os CSVs e logs brutos do RX e do injetor. A conferência incluiu sequência única de 0 a 999, soma das quatro classes, recálculo do resíduo e comparação dos bytes do injetor com a referência e o corpo recebido. Tentativas inválidas não foram incluídas nas tabelas.

# 5 RESULTADOS EXPERIMENTAIS

A série contém 7000 registros RX e 11561 registros do injetor. O número de amostras do injetor varia com a quantidade de bytes observados por quadro e não corresponde ao número de quadros. Todas as sete rodadas selecionadas registraram validação de rodada positiva, sem falhas de referência ou divergências nas verificações realizadas.

Tabela 4 - Resultados SUM8, com 1000 quadros por modo.

| Modo | OK | Corrompido | Perdido | Não detectado | Taxa ND |
| --- | --- | --- | --- | --- | --- |
| 0 | 1000 | 0 | 0 | 0 | NA |
| 1 | 469 | 531 | 0 | 0 | 0,0000% |
| 2 | 213 | 783 | 0 | 4 | 0,5083% |
| 3 | 44 | 956 | 0 | 0 | 0,0000% |

Fonte: Elaboração própria, a partir dos registros experimentais (2026).

Tabela 5 - Resultados CRC8, com 1000 quadros por modo.

| Modo | OK | Corrompido | Perdido | Não detectado | Taxa ND |
| --- | --- | --- | --- | --- | --- |
| 1 | 443 | 557 | 0 | 0 | 0,0000% |
| 2 | 195 | 805 | 0 | 0 | 0,0000% |
| 3 | 26 | 974 | 0 | 0 | 0,0000% |

Fonte: Elaboração própria, a partir dos registros experimentais (2026).

OK identifica um quadro aceito com payload igual à referência. CORROMPIDO identifica um corpo checado e descartado. PERDIDO indica que a checagem não chegou a ser concluída e a falta foi identificada pelo enlace. NÃO DETECTADO identifica uma aceitação com payload diferente da referência válida.

A taxa de não detecção utiliza apenas os quadros com erro de conteúdo:

Taxa ND = NÃO DETECTADO / (CORROMPIDO + NÃO DETECTADO).

Para SUM8 no modo 2, a taxa foi 4/(783+4) = 4/787 = 0,005082592, ou 0,5082592%. Os quatro casos correspondem a 0,4% de todos os 1000 quadros, fração com denominador diferente. No controle sem erro, a taxa é NA, pois o denominador é zero. Não foram registradas perdas nas sete rodadas regulares.

# 5.1 ANÁLISE DE UM ERRO NÃO DETECTADO

O quadro 232 da rodada SUM8/modo 2 apresentou duas alterações físicas confirmadas pelos registros do injetor. O corpo original e o recebido, incluindo o detector, foram:

Original: 07 E8 00 95 8E B4 99 A1
Recebido: 07 E8 00 15 0E B4 99 A1

Tabela 6 - Comparação byte a byte do quadro 232.

| Campo | Original hex | Original decimal | RX hex | RX decimal | Variação |
| --- | --- | --- | --- | --- | --- |
| LEN | 07 | 7 | 07 | 7 | 0 |
| seqL | E8 | 232 | E8 | 232 | 0 |
| seqH | 00 | 0 | 00 | 0 | 0 |
| p0 | 95 | 149 | 15 | 21 | -128 |
| p1 | 8E | 142 | 0E | 14 | -128 |
| p2 | B4 | 180 | B4 | 180 | 0 |
| p3 | 99 | 153 | 99 | 153 | 0 |
| CHK | A1 | 161 | A1 | 161 | 0 |

Fonte: Elaboração própria, a partir dos registros experimentais (2026).

A sequência 0x00E8 equivale a 232. A soma dos sete bytes originais é 863, e CHK = -863 mod 256 = 161 = A1. A soma original completa é 1024. No corpo recebido, 95 (149) tornou-se 15 (21), e 8E (142) tornou-se 0E (14). A redução total foi de 256, produzindo soma 768. Como 1024 e 768 são múltiplos de 256, ambos têm resíduo zero.

Tabela 7 - Efeito registrado pelo injetor no quadro 232.

| Byte payload | Alvo / alterado | Antes → depois | Timestamp (µs) | Mudou / sincronismo |
| --- | --- | --- | --- | --- |
| 0 | 80 / 80 | 95 → 15 | 888739020 | SIM / SIM |
| 1 | 80 / 80 | 8E → 0E | 888742020 | SIM / SIM |

Fonte: Elaboração própria, a partir dos registros experimentais (2026).

O RX decidiu ACEITO antes de ler a cópia. A auditoria posterior constatou payload diferente e classificou o quadro como NÃO DETECTADO. Os dois registros do injetor concordam com os corpos RX e de referência; os campos de amostras e pulsos tardios foram zero. Os registros integrais estão reproduzidos no Apêndice A.

# 5.2 TEMPOS E OVERHEAD

Os tempos de verificação e os intervalos foram obtidos dos CSVs da série atual. O custo de cálculo no TX foi obtido de um controle anterior sem ruído, com dez quadros por técnica e o mesmo firmware TX. Essas amostras são distintas e não foram agregadas.

Tabela 8 - Tempos de cálculo e verificação, em microssegundos.

| Medição | SUM8 | CRC8 | Número de amostras |
| --- | --- | --- | --- |
| TX: média de cálculo | 1,100 | 3,000 | 10 por técnica |
| TX: mínimo / máximo | 1 / 2 | 3 / 3 | Mesmo controle |
| RX: média de verificação | 1,000 | 3,000 | 1000 por rodada atual |
| RX: mínimo / máximo | 1 / 1 | 3 / 3 | 1000 por rodada atual |

Fonte: CSV das sete rodadas e capturas do controle TX, identificadas nas referências experimentais.

Tabela 9 - Intervalos entre inícios de quadro no RX.

| Coleta | Média (µs) | Mínimo / máximo (µs) |
| --- | --- | --- |
| S0 SUM8/0 | 101001,001 | 100999 / 101919 |
| S1 SUM8/1 | 101000,000 | 100999 / 101001 |
| S2 SUM8/2 | 101000,043 | 100999 / 101022 |
| S3 SUM8/3 | 101000,958 | 100999 / 101919 |
| C1 CRC8/1 | 101001,001 | 100999 / 101919 |
| C2 CRC8/2 | 101001,001 | 100999 / 101919 |
| C3 CRC8/3 | 101000,000 | 101000 / 101000 |

Fonte: Elaboração própria, a partir dos registros experimentais (2026).

Cada média de intervalo utiliza 999 diferenças; o zero inicial é excluído. O período observado ficou próximo de 101 ms, justificando o timeout de enlace de 250 ms. As leituras usam esp_timer_get_time() e resolução em microssegundos, sem representar a latência completa do enlace nem medições submicrossegundo.

Tabela 10 - Composição dos bytes transmitidos por ciclo.

| Componente | Bytes |
| --- | --- |
| Overhead: marcador 4 + LEN 1 + sequência 2 + CHK 1 | 8 |
| Payload | 4 |
| Quadro de dados completo | 12 |
| Depuração: marcador 4 + cópia do corpo 8 | 12 |
| Total de dados e depuração, sem as pausas | 24 |

Fonte: Elaboração própria, a partir do formato do quadro (2026). A cópia é excluída do overhead do protocolo.

# 5.3 PERDA CONTROLADA E RECUPERAÇÃO

Foi executado um ensaio complementar com dez quadros CRC8, RX completo v0.2 e ruído desligado. Após checar e auditar a sequência 1, o RX encerrou sua UART2 por uma janela programada de 350 ms. O TX continuou transmitindo. A ausência de recepção foi induzida por software, separadamente das perturbações elétricas dos experimentos principais.

Tabela 11 - Instantes registrados durante a interrupção da recepção.

| Evento | Instante no RX (µs) |
| --- | --- |
| UART encerrada após auditar seq1 | 707244965 |
| Primeira perda declarada | 707481280 |
| UART reativada | 707595727 |
| Seq5: próxima decisão ACEITO | 707634308 |

Fonte: Capturas do ensaio de recuperação com RX completo v0.2 (17 out. 2026).

Tabela 12 - Duração da pausa e tempos de recuperação.

| Medida | Diferença (µs) | Resultado |
| --- | --- | --- |
| Janela desligada | 707595727 - 707244965 | 350,762 ms |
| Primeira perda até ACEITO | 707634308 - 707481280 | 153,028 ms |
| Reativação até ACEITO | 707634308 - 707595727 | 38,581 ms |

Fonte: Elaboração própria, por diferenças de timestamps da Tabela 11.

O resumo apresentou sete OK e três PERDIDO, sem CORROMPIDO ou NÃO DETECTADO. As sequências 2 e 3 foram declaradas perdidas por timeout; a 4 foi identificada pela lacuna ao receber a sequência 5. O quadro 5 foi aceito na checagem, auditado como correto e seguido por recepção até a sequência 9. A captura complementar confirmou rodada válida.

O cronômetro de recuperação preservou a primeira detecção de perda do episódio, sem reiniciar ao detectar a lacuna seguinte. A medida de 153,028 ms termina na próxima aceitação do protocolo; em geral, uma auditoria posterior ainda poderia revelar aceitação indevida. Neste ensaio, a referência confirmou o payload do quadro 5.

As sete verificações desse controle tiveram média de 3,143 µs, mínimo de 3 µs e máximo de 4 µs. Os tempos foram calculados com leituras do mesmo relógio RX, sem subtrair timestamps de placas distintas. O ensaio está documentado por capturas do Monitor Serial e não integra os CSVs das sete rodadas de 1000 quadros.

# 6 DISCUSSÃO E CONCLUSÃO

## 6.1 Discussão dos resultados

Os quatro erros não detectados em SUM8/modo 2 exibiram o mesmo mecanismo: apagar dois bits de peso 128 em bytes distintos reduziu a soma em 256. Além da sequência 232, os casos ocorreram nas sequências 239, 530 e 609. Nas duas primeiras, a soma completa passou de 1024 para 768; nas duas últimas, de 768 para 512. A informação perdida pela redução modular explica a aceitação de todos esses quadros alterados.

CRC8 não apresentou erros não detectados nas três rodadas, resultado compatível com sua cobertura para um ou dois bits alterados e para alterações limitadas a um byte. A conclusão se refere às classes ensaiadas: um CRC finito também admite colisões fora delas.

A contagem de descartes isoladamente não mede a superioridade de um detector. As rodadas utilizaram novos sorteios de dados e alvos, e o transistor pode selecionar bits que já eram zero. A comparação utiliza a taxa de aceitação indevida entre quadros efetivamente alterados. Também se deve considerar que p0/p1 possuem o bit mais significativo fixado em 1, de modo que o payload não é uniforme em todos os bits.

Os limites do estudo incluem a restrição do ruído ao payload, a quantização temporal em microssegundos e a dependência do injetor em relação ao core e ao hardware testados. A passagem física da sequência 65535 para zero não foi demonstrada nas rodadas finitas. O ensaio de recuperação mostra retorno após uma interrupção de UART, sem representar todas as condições de perda possíveis.

## 6.2 Conclusão

Foi implementado e avaliado um protocolo de telemetria em que a decisão do RX antecede a leitura da referência. As sete rodadas válidas totalizaram 7000 quadros e permitiram observar quatro aceitações indevidas do SUM8, explicadas e confirmadas byte a byte. O CRC8 apresentou maior tempo de cálculo e verificação nos registros utilizados, acompanhando maior cobertura para as classes de erro ensaiadas.

O RX identificou perdas sem duplicar descartes e retomou a aceitação após a pausa controlada. A escolha do detector deve considerar o modelo de erro e a alteração real do dado, pois nem todo pulso produz corrupção.

# REFERÊNCIAS

SMBUS. SMBus FAQ: What is the CRC-8 polynomial for SMBus? [S. l.], s.d. Disponível em: https://smbus.org/faq/faq_main.htm. Acesso em: 17 out. 2026.

KOOPMAN, Philip. 8 Bit CRC Zoo. Pittsburgh: Carnegie Mellon University, 2024a. Disponível em: https://users.ece.cmu.edu/~koopman/crc/crc8.html. Acesso em: 17 out. 2026.

KOOPMAN, Philip. CRC Zoo Notes. Pittsburgh: Carnegie Mellon University, 2024b. Disponível em: https://users.ece.cmu.edu/~koopman/crc/notes.html. Acesso em: 17 out. 2026.

## Registros experimentais e fontes das medições

AURÉLIO, Enzo. Registros experimentais de telemetria com ESP32. 17 out. 2026. Conjunto de sete rodadas, com rx.csv, ruido.csv, logs brutos, metadados e revisão por rodada. Arquivos na pasta dados/ do material experimental; identificação no Apêndice A.

AURÉLIO, Enzo. Capturas dos controles de cálculo no TX. 2026. Dez quadros por técnica, com SUM8 e CRC8. Reproduzidas no documento de evidência Parte 4, páginas 4 e 5. Arquivo: evidencias/Parte_4_Tempos_TX_e_Ensaios_Anteriores.pdf. As medições do antigo RX de ensaio contidas nesse documento não compõem os tempos RX da série atual.

AURÉLIO, Enzo. Capturas do ensaio de recuperação com RX completo v0.2. 2026. Reproduzidas no documento de evidência Parte 6, página 5, incluindo a confirmação complementar da validade. Arquivo: evidencias/Parte_6_Ensaio_RX_Completo.pdf.

Os documentos de evidência preservam as capturas dos instrumentos de registro. As tabelas principais utilizam exclusivamente as sete rodadas identificadas no apêndice. Os controles de cálculo TX e de recuperação mantêm suas amostras e configurações próprias.

# APÊNDICE A - REGISTROS E RASTREABILIDADE

Tabela A1 - Identificação das sete rodadas selecionadas.

| ID | Condição | Pasta original |
| --- | --- | --- |
| S0 | SUM8/0 | 20261017_191422_664_SUM8_modo0_1000 |
| S1 | SUM8/1 | 20261017_184431_084_SUM8_modo1_1000 |
| S2 | SUM8/2 | 20261017_185353_632_SUM8_modo2_1000 |
| S3 | SUM8/3 | 20261017_190634_547_SUM8_modo3_1000 |
| C1 | CRC8/1 | 20261017_185043_967_CRC8_modo1_1000 |
| C2 | CRC8/2 | 20261017_185759_222_CRC8_modo2_1000 |
| C3 | CRC8/3 | 20261017_191057_739_CRC8_modo3_1000 |

Fonte: Elaboração própria, a partir dos registros experimentais (2026).

Os identificadores S0 a S3 correspondem aos modos SUM8; C1 a C3, aos modos CRC8. As pastas do material experimental foram organizadas como dados/TECNICA_modoN_1000. Cada uma contém os seis arquivos originais da coleta, preservados byte a byte. O manifesto SHA256 registra a integridade dessas cópias.

## A.1 Registro RX do quadro 232

seq_enlace=232; seq_rx=232; tecnica=SUM8; residuo=0;
decisao=ACEITO; status=NAO_DETECTADO; payload_confere=NAO;
verificacao_us=1; intervalo_inicio_us=101000;
corpo_rx_hex=07E800150EB499A1;
corpo_tx_hex=07E800958EB499A1;
motivo=CHECADO_ANTES_DA_COPIA.

## A.2 Linhas do injetor para o mesmo quadro

Campos do CSV, na ordem original: sequência, modo, byte_payload, máscara alvo, timestamp_us, antes, depois amostrado, máscara alterada, mudou, sincronismo, atraso máximo, amostras tardias, pulsos tardios e duração do byte.

232,2,0,80,888739020,95,15,80,
SIM,SIM,1,00,00,89

232,2,1,80,888742020,8E,0E,80,
SIM,SIM,1,00,00,89

As quebras de linha acima servem apenas à diagramação. Nos arquivos originais, cada registro ocupa uma linha. A máscara 0x80 corresponde ao bit 7; em ambos os bytes, o registro informa alteração efetiva e sincronismo válido.
