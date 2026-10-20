# Roteiro de apresentação CORPO64 - 20 minutos

Resultados físicos de 19/10/2026. Use o relatório atual e as instruções do README.md. A defesa e a predição precisam ser realizadas ao vivo.

| Tempo | Conteúdo |
|---|---|
| 0-3 min | Objetivo, três ESP32, transistor que força zero e telemetria sintética. |
| 3-6 min | Quadro e corpo de 64 bits; LEN e sequência; SUM8/CRC8; decisão antes da cópia. |
| 6-10 min | Tabelas e gráficos das sete coletas. Estudar seq 415: P0 87→07 e CHK E7→67; soma 768→512; resíduo zero. |
| 10-15 min | Predição individual dos dez quadros do cenário escolhido pelo professor. Preparar RX antes de TX, executar, comparar e conferir validade. |
| 15-18 min | Tempos, overhead e recuperação: pausa 350,443 ms, primeira aceitação 38,691 ms após reativar. |
| 18-20 min | Conclusões, limites, acesso ao GitHub e perguntas. |

## Números e distinções para defender

- SUM8/2: 289 OK, 619 corrompidos, 90 perdidos e 2 ND. Taxa ND=2/621=0,3221%.
- As perdas nas sete rodadas são LEN_INVALIDO; o controle de recepção desligada é separado.
- Casos naturais ND: sequências 415 e 553, ambos com payload e CHK alterados.
- Cálculo TX: 1 µs SUM8 e 3 µs CRC8, dez amostras por técnica.
- Média RX: 1,006 µs SUM8 (3.774 checagens) e 3,003 µs CRC8 (2.764).
- Intervalo TX: 101 ms. Overhead: 8 bytes para 4 úteis; quadro de dados de 12 bytes, sem contar cópia.
- Referência não corrige sequência nem payload entregue. Descarte não é contado de novo como perda.
- Zero ND em CRC8 nos ensaios não é garantia universal. Marcador e start/stop não foram alvos.

## Preparação

Levar as placas, montagem, cabos USB e cópias locais do PDF/repositório. Identificar portas e deixar TX parado antes de preparar. Todos devem explicar o sistema inteiro e informar a assistência de IA conforme as regras da disciplina.

## Registro da predição presencial

Técnica e modo escolhidos pelo professor: ___________________

| Integrante | OK previsto | Corrompido previsto | Perdido previsto | ND previsto | Justificativa |
|---|---|---|---|---|---|
| Enzo Aurélio | | | | | |
| Luís Fernando | | | | | |
| Vinicius Moraes | | | | | |

Resultado e validade observados: __________________________
