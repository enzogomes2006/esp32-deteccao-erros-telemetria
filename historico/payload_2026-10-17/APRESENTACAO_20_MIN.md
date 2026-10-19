# Roteiro de apresentacao - 20 minutos

Use o GUIA_DEMONSTRACAO.md para os comandos completos. Este roteiro organiza a exposicao; nao comprova que ela ja aconteceu.

| Tempo | Mostrar e explicar |
|---|---|
| 0-3 min | Objetivo; papeis de TX, RX e RUIDO; UART 115200; GND comum; transistor que so forca zero. |
| 3-6 min | Quadro: marcador, LEN, sequencia, quatro bytes de payload e um detector. O RX decide pelo residuo antes de consultar a copia. Explicar timeout, lacunas e ausencia de retransmissao. |
| 6-10 min | Duas tabelas da serie de 1000 quadros. Explicar que alvos em bits ja zero nao causam erro. Mostrar o caso SUM8 seq232: dois MSBs apagados reduzem a soma em 256 e preservam residuo zero. |
| 10-15 min | Teste de predicao escolhido pelo professor: cada integrante anota antes os quatro contadores previstos, somando dez, e justifica. Preparar RX antes de iniciar TX. Comparar previsao, resultado e validade. |
| 15-18 min | Mostrar custos TX/RX e overhead no relatorio. Se solicitado, demonstrar a pausa/recuperacao com o RX completo, sem trocar firmware. |
| 18-20 min | Conclusao e perguntas. Explicar cobertura e limites do CRC8, disponibilizar o repositorio e os dados. |

## Antes da aula

- Levar as tres placas, montagem, cabos USB e computador com Arduino IDE/core adequados.
- Levar o ZIP e o PDF localmente; abrir o repositorio para conferir o link.
- Confirmar portas e alimentar as placas; preservar os programas usados nas coletas.
- Se fizer uma conferencia curta, conferir dez OK sem ruido e validade positiva.
- Deixar o TX parado antes de selecionar tecnica/modo e preparar o RX.
- Fechar o coletor se for usar os mesmos monitores seriais.

## Pontos que todos devem conseguir defender

- Aceitar pelo residuo e auditar pela copia sao etapas distintas.
- CORROMPIDO nao e contado de novo como PERDIDO.
- Taxa ND usa apenas CORROMPIDO + NAO_DETECTADO no denominador.
- O caso SUM8/modo 2 foi quatro erros nao detectados em 787 quadros com erro, nao quatro em 1000 para essa taxa.
- O padrao extra do modo 4 demonstra uma colisao; ele nao substitui a serie obrigatoria.
- Uma nova rodada usa novos sorteios e nao tem obrigacao de reproduzir os mesmos contadores.
- As sete rodadas nao provaram que todo erro possivel sera detectado por CRC8.

## Registro da predicao ao vivo

Tecnica: ________  Modo escolhido pelo professor: ________

| Integrante | OK previsto | Corrompido previsto | Perdido previsto | ND previsto | Justificativa |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |

Resultado RX: ____________________________________

Validade e observacoes: __________________________
