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


# Regras atuais do receptor CORPO64

O parser limita o corpo a oito bytes e exige LEN=7. LEN inválido gera PERDIDO por enquadramento, SEM_CHECAGEM. Em corpos válidos, resíduo não zero gera DESCARTADO/CORROMPIDO e não avança a sequência usando cabeçalho não confiável. Resíduo zero gera ACEITO; o enlace aplica ordenação uint16 de meia faixa e distingue essa aceitação da entrega à aplicação.

Descartes e eventos malformados acumulam eventos anônimos. A próxima sequência aceita permite evidenciar a lacuna, descontando os anônimos e as ausências já diagnosticadas. Isso impede dupla contagem de corrompidos como perdidos. Associações incompatíveis são registradas como ambiguidades.

A auditoria associa cada evento à referência somente depois da decisão. seq_rx mantém a sequência recebida; seq_referencia/seq_experimento são identidade de auditoria. A referência nunca reescreve o enlace. Divergências de cabeçalho são registradas à parte; a classe NAO_DETECTADO da tabela usa divergência do payload. ACEITO não implica automaticamente entregue_aplicacao.

Timeouts: 60 ms para bloco/auditoria e 250 ms para ausência de progresso de dados, com passo de 100 ms. Progresso de dados, não a referência, renova o prazo de enlace. Referência órfã, ausente, inválida ou associação ambígua invalida a medição. Perdas sem evento exigem evidência de ausência e conciliação sem ambiguidade.

Ruído CORPO64: modo 1 sorteia um dos 64 bits; modo 2 dois distintos; modo 3 rajada contígua de 3 a 8 bits dentro de um byte. Modo 0 é controle sem pulsos. Modo 4 é controle extra em P0/P1. Marcador e referência permanecem preservados.

Os detalhes de projeto prévios estão em revisoes/corpo64/ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md, preservada como documento histórico. Implementação atual em revisoes/corpo64/firmware/. A especificação antiga está em historico/payload_2026-10-17/ESPECIFICACAO_PROTOCOLO.md.
