# Detecção de erros em telemetria com ESP32

Enzo Aurélio · IFMT - Octayde · TCD · Professor Ruy Oliveira.

**Série atual: CORPO64, coletada em 19/10/2026.** Sete rodadas físicas válidas, 7.000 quadros e 56.000 amostras do injetor. Foram confirmados dois erros não detectados do SUM8, nas sequências 415 e 553 do modo 2. O repositório é público.

## Materiais atuais

- [Relatório PDF](Relatorio_Final_ESP32.pdf) e [texto completo](Relatorio_Final_ESP32.md).
- [Dados físicos CORPO64](revisoes/corpo64/dados/) e [resultados consolidados](resultados_consolidados.json).
- [Validação física](revisoes/corpo64/validacao/VALIDACAO_FISICA.json), [revisão oficial das coletas](revisoes/corpo64/validacao/REVISAO_OFICIAL_COLETAS.json) e [procedência](revisoes/corpo64/PROVENIENCIA_COLETAS.json).
- [Controles TX e recuperação](evidencias/corpo64/ORIGEM.md).
- [Guia para repetir os ensaios](GUIA_DEMONSTRACAO.md), [protocolo atual](ESPECIFICACAO_PROTOCOLO.md), [roteiro de 20 minutos](APRESENTACAO_20_MIN.md) e [pendências de entrega](PROXIMOS_PASSOS.md).
- [Firmware atual e instruções](revisoes/corpo64/LEIA_PRIMEIRO.md), [montagem](montagem/GUIA_MONTAGEM_FISICA_ESP32.md) e [esquema elétrico](montagem/esquema_eletrico.svg).

## Resultados medidos

| Técnica | Modo | OK | Corrompido | Perdido | Não detectado |
| --- | --- | --- | --- | --- | --- |
| SUM8 | 0 | 1000 | 0 | 0 | 0 |
| SUM8 | 1 | 563 | 392 | 45 | 0 |
| SUM8 | 2 | 289 | 619 | 90 | 2 |
| SUM8 | 3 | 100 | 809 | 91 | 0 |
| CRC8 | 1 | 540 | 418 | 42 | 0 |
| CRC8 | 2 | 305 | 593 | 102 | 0 |
| CRC8 | 3 | 116 | 792 | 92 | 0 |

Taxa ND = NAO_DETECTADO/(CORROMPIDO+NAO_DETECTADO). SUM8/2: 2/621 = **0,3221%**. Os 90 perdidos dessa condição não entram no denominador. Nas sete séries, todos os perdidos foram LEN inválido, sem checagem concluída.

![Contagens por condição](revisoes/corpo64/graficos/contagens.svg)

## O que o sistema faz

TX envia dados sintéticos por UART 115200 8N1. RUÍDO aciona BC547 para forçar bits a zero em LEN, sequência, payload ou CHK. RX calcula SUM8 ou CRC-8/SMBUS e registra a decisão antes de consultar a cópia de depuração. Referência não corrige a decisão ou o enlace. Marcador e cópia ficam preservados. Não há ACK nem retransmissão.

Tempos médios RX: SUM8 **1,006 µs** em 3.774 checagens; CRC8 **3,003 µs** em 2.764. Controles TX: 1 µs e 3 µs. Intervalo TX: 101 ms. Recuperação após reativar UART: 38,691 ms, após pausa efetiva de 350,443 ms.

## Versões e histórico

Use os programas de **revisoes/corpo64/firmware/** e o coletor v3 dessa revisão. Os programas em firmware/ na raiz e as coletas em dados/ na raiz pertencem à série anterior, com ruído somente no payload. Não combinar as versões.

O [relatório e os documentos anteriores](historico/payload_2026-10-17/README_HISTORICO.md), os PDFs por etapa e as evidências antigas foram preservados. A especificação prévia CORPO64 e o estado original de testes de software também permanecem como registros datados.

## Reproduzir a análise

Requer Python 3 e reportlab para o PDF. Na raiz: `python scripts/consolidar.py`, `python scripts/atualizar_documentacao.py`, `python scripts/gerar_pdf.py`, `python scripts/manifestos.py`. Os dois primeiros passos usam os CSVs versionados, sem precisar de portas seriais. O coletor no Windows funciona sem Python.

Zero ND em CRC8 nas rodadas não garante detecção universal. Hashes garantem integridade dos arquivos, sem atestar o binário gravado. A defesa e a predição ao vivo ainda precisam ser realizadas. Os programas e documentos tiveram assistência de IA, conforme registrado no relatório.
