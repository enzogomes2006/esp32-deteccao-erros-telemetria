# Especificação do quadro e da checagem

Formato comum às duas técnicas; somente o cálculo de CHK muda entre rodadas.

Dados: A5 5A C3 3C | 07 seqL seqH p0 p1 p2 p3 CHK

| Campo | Bytes | Definição |
| --- | --- | --- |
| Marcador | 4 | A5 5A C3 3C, na ordem indicada; fora da checagem |
| LEN | 1 | 07: sete bytes após LEN; esta implementação só aceita esse valor |
| Sequência | 2 | uint16, byte menos significativo primeiro; seq = seqL + 256 × seqH |
| Payload | 4 | Telemetria sintética; p0/p1 têm o bit 7 fixado em 1 no TX |
| CHK | 1 | SUM8 ou CRC8; não há dois detectores simultâneos |

O comprimento fixo elimina a necessidade de um marcador de fim e permite dados com qualquer valor. Ao reconhecer o marcador, o RX coleta exatamente oito bytes e valida LEN. Um marcador que apareça dentro desses oito bytes é tratado como dado. Após o corpo, procura novos marcadores a partir do próximo byte, inclusive depois de um descarte.

## SUM8: soma modular

Campos protegidos: LEN, sequência e payload (sete bytes). O TX calcula CHK = -soma(dados) mod 256. O RX soma os oito bytes, incluindo CHK, e aceita somente quando o resultado módulo 256 é zero. Um descarte é registrado e não entrega o payload como confiável.

## CRC-8/SMBUS

Polinômio x^8 + x^2 + x + 1, representado por 0x07 sem o termo x^8 [1]. Estado inicial 0x00, refin=false, refout=false, xorout=0x00. O TX percorre os sete bytes na ordem do corpo, com processamento MSB primeiro em cada byte. O RX percorre os oito bytes; resíduo esperado 0x00. A UART continua transmitindo cada byte em 8N1, LSB primeiro.

## Cópia de depuração: auditoria posterior

Referência: D5 6A B4 4B | cópia dos oito bytes originais do corpo.

O marcador distingue a referência dos dados. O corpo completo é copiado para associar sequência, validar a referência e comparar os quatro bytes do payload. A linha DECISAO_ANTES_DA_COPIA é emitida antes dessa leitura. A referência não modifica a decisão, não corrige a sequência e não reinicia o timeout do enlace.

## Temporização do TX e ausência de retorno

O TX pausa 10 ms após o marcador, 3 ms após cada byte do corpo e 10 ms antes da cópia. As pausas ficam em nível ocioso alto; a taxa de cada byte permanece 115200 baud. Cada rodada começa na sequência 0; t envia dez quadros e g envia mil. O contador é uint16. O enlace implementado é unidirecional, sem ACK ou retransmissão; um descarte não interrompe o quadro seguinte.

# Regras do receptor e timeouts

O estado do enlace depende dos bytes recebidos; a classificação experimental depende também da auditoria.

## Sequência e quadro descartado

A rodada tem uma janela finita de 10 ou 1000 posições, com origem zero nos comandos USB. Após montar um corpo com LEN=7, o RX calcula o resíduo e decide ACEITO ou DESCARTADO. Para um número admissível, marca as posições anteriores ainda não observadas como PERDIDO, registra a decisão da posição atual e passa a esperar seq_rx+1. Um quadro checado e descartado não é contado novamente como perdido.

A diferença é calculada em uint16: delta = seq_rx - seq_esperada. Delta ≥ 32768 significa sequência antiga/fora de ordem. O índice da janela é uint16(seq_rx - primeira_seq); números fora da janela e posições já registradas são ignorados e diagnosticados. Se houver descarte com sequência não confiável, a medição é invalidada. A cópia nunca é usada para corrigir esse histórico.

A aritmética prevê 65535 para 0 em uma janela que cruze esse limite. Esse caso foi exercitado em software; as rodadas físicas de 0 a 999 não demonstram o cruzamento. Antes do primeiro marcador de dados, o RX apenas aguarda: não declara perdas por ausência de início da transmissão.

| Situação | Prazo e origem | Tratamento |
| --- | --- | --- |
| Corpo/cópia parcial | 60 ms desde o respectivo marcador | Abandona o bloco e procura marcador no próximo byte processado |
| Auditoria pendente | 60 ms desde o marcador de dados | Referência ausente invalida a medição; não cria OK fictício |
| Silêncio no enlace | 250 ms desde primeiro marcador; reiniciado após cada corpo checado | Marca uma posição não observada como perdida |
| Silêncio persistente | Avança o prazo em 100 ms por posição perdida | Segue a janela sem duplicar posições classificadas |

O prazo de 250 ms corresponde a aproximadamente 2,48 períodos medidos de 101 ms: tolera variação em torno de um período sem retardar excessivamente a perda. Com as pausas do TX, a referência termina em cerca de 45 ms após o marcador de dados; 60 ms é uma margem de projeto. Os prazos são atendidos pelo loop, sem promessa de disparo no microssegundo exato.

## Quatro classes, uma vez por sequência

| Classe | Critério |
| --- | --- |
| OK | ACEITO, com referência válida e payload igual |
| CORROMPIDO | Corpo checado e DESCARTADO por resíduo diferente de zero |
| PERDIDO | Sem checagem concluída; identificado por lacuna ou timeout |
| NAO_DETECTADO | ACEITO, mas payload diferente da referência válida |

Referência ausente, com LEN/seq divergente ou resíduo inválido torna a rodada inválida. Um aceito sem referência fica não classificado; não vira uma quinta classe da tabela. A série só é usada quando as quatro classes somam o esperado e os diagnósticos de auditoria são consistentes.
