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

Polinômio x^8 + x^2 + x + 1, representado por 0x07 sem o termo x^8. Estado inicial 0x00, refin=false, refout=false, xorout=0x00. O TX percorre os sete bytes na ordem do corpo, com processamento MSB primeiro em cada byte. O RX percorre os oito bytes; resíduo esperado 0x00. A UART continua transmitindo cada byte em 8N1, LSB primeiro.

## Cópia de depuração: auditoria posterior

Referência: D5 6A B4 4B | cópia dos oito bytes originais do corpo.

O marcador distingue a referência dos dados. O corpo completo é copiado para associar sequência, validar a referência e comparar os quatro bytes do payload. A linha DECISAO_ANTES_DA_COPIA é emitida antes dessa leitura. A referência não modifica a decisão, não corrige a sequência e não reinicia o timeout do enlace.

## Temporização do TX e ausência de retorno

O TX pausa 10 ms após o marcador, 3 ms após cada byte do corpo e 10 ms antes da cópia. As pausas ficam em nível ocioso alto; a taxa de cada byte permanece 115200 baud. Cada rodada começa na sequência 0; t envia dez quadros e g envia mil. O contador é uint16. O enlace implementado é unidirecional, sem ACK ou retransmissão; um descarte não interrompe o quadro seguinte.


## Regras do receptor

O parser limita o corpo a oito bytes e exige LEN=7. LEN inválido gera PERDIDO por enquadramento, SEM_CHECAGEM. Em corpos válidos, resíduo não zero gera DESCARTADO/CORROMPIDO e não avança a sequência usando cabeçalho não confiável. Resíduo zero gera ACEITO; o enlace aplica ordenação uint16 de meia faixa e distingue essa aceitação da entrega à aplicação.

Descartes e eventos malformados acumulam eventos anônimos. A próxima sequência aceita permite evidenciar a lacuna, descontando os anônimos e as ausências já diagnosticadas. Isso impede dupla contagem de corrompidos como perdidos. Associações incompatíveis são registradas como ambiguidades.

A auditoria associa cada evento à referência somente depois da decisão. seq_rx mantém a sequência recebida; seq_referencia/seq_experimento são identidade de auditoria. A referência nunca reescreve o enlace. Divergências de cabeçalho são registradas à parte; a classe NAO_DETECTADO da tabela usa divergência do payload. ACEITO não implica automaticamente entregue_aplicacao.

### Sequência, lacunas e repetição

A origem da rodada é zero e a janela tem dez ou mil posições. A sequência é um inteiro de 16 bits sem sinal. Para um corpo aceito, calcula-se `delta = uint16_t(seq_rx - esperada)`. Se `delta >= 32768`, o número é antigo ou repetido: não há nova entrega e a sequência esperada não avança. Essa comparação ordena diferenças menores que meia faixa do contador. A passagem de 65535 para 0 é feita pela própria aritmética uint16; não foi exercitada nas rodadas físicas de 0 a 999.

Cada corpo descartado ou malformado soma um evento anônimo desde a última sequência aceita. Ao aceitar um número adiante, o receptor desconta esses eventos e as ausências já diagnosticadas de `delta`. Somente o restante positivo aumenta a evidência de quadros ausentes. Um valor incompatível gera diagnóstico de ambiguidade, sem inventar uma posição para o descarte. Depois de um aceite em ordem, a próxima sequência esperada é `uint16_t(seq_rx + 1)` e os acumuladores dessa lacuna são zerados.

A referência válida associa o evento pendente à posição experimental depois da decisão. Cada posição só recebe uma classificação. Posições sem evento só são preenchidas como PERDIDO quando a janela pode ser encerrada e a contagem de ausências do enlace coincide com as posições faltantes, sem ambiguidades nem cabeçalho divergente em corpo aceito. Referência órfã, ausente, inválida ou associação ambígua invalida a medição.

### Timeouts e justificativa

| Prazo | Quando começa ou é renovado | Ação |
| --- | --- | --- |
| 60 ms para bloco | Reconhecimento do marcador de dados ou de referência | Abandona bloco incompleto e volta à busca de marcador |
| 60 ms para auditoria | Marcador do evento de dados | Referência não recebida no prazo invalida a medição |
| 250 ms para ausência | Primeiro marcador da rodada; renovado pelo progresso de dados (marcador, decisão de corpo ou timeout do corpo) | Acrescenta evidência de ausência de evento, sem duplicar um descarte |
| 100 ms adicionais | Prazo anterior de ausência | Registra a próxima ausência, se o silêncio continuar |

Antes do primeiro marcador, o RX apenas aguarda; não declara perdas por demora para iniciar o TX. A cópia de depuração não renova o prazo do enlace.

Os logs mediram mediana de 101 ms entre quadros, com máximo de 101,917 ms nas sete séries. Os 250 ms equivalem a aproximadamente 2,48 períodos medidos, permitindo variação de agendamento antes de declarar ausência. O passo de 100 ms acompanha o período nominal do TX. Para quadro e cópia, 24 bytes em 8N1 a 115200 baud mais as pausas programadas de 44 ms dão cerca de 46,08 ms nominais. O limite de 60 ms fornece margem de processamento e impede espera indefinida. Essas contas justificam os valores de projeto; os prazos são atendidos pelo loop e não por um disparo exato no microssegundo.

Ruído CORPO64: modo 1 sorteia um dos 64 bits; modo 2 dois distintos; modo 3 rajada contígua de 3 a 8 bits dentro de um byte. Modo 0 é controle sem pulsos. Modo 4 é controle extra em P0/P1. Marcador e referência permanecem preservados.

A implementação atual está em firmware/. A [proposta prévia desta revisão](https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria/blob/520dca3/revisoes/corpo64/ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md) permanece no histórico Git. Esse registro não comprova a ordem de elaboração da versão original.
