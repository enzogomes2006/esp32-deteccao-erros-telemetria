# Guia da demonstração final

São três programas, um por placa. Os nomes abaixo foram simplificados somente nas cópias deste pacote; o conteúdo é idêntico ao usado nas últimas sete coletas.

| Placa | Abrir na Arduino IDE | Porta observada |
|---|---|---|
| TX | firmware/tx/tx.ino | COM5 |
| RX | firmware/rx/rx.ino | COM6 |
| RUÍDO | firmware/noise/noise.ino | COM3 |

Mantenha capture_schedule.h na mesma pasta de noise.ino. Não coloque os três sketches na mesma pasta. Use ESP32 Dev Module, ESP32 clássico WROOM, CPU 240 MHz e core Espressif 3.3.12. Os números COM podem mudar; confira a placa. O código de RUÍDO usa APIs LL dessa versão, não é uma versão para C3/S3/S2.

Se não gravou outro programa desde as sete coletas, os programas já estão nas placas: não precisa regravar para usar este pacote. As mensagens de inicialização ainda dizem candidato porque os arquivos validados foram copiados sem edição; a seleção para a entrega é baseada nos testes registrados, não na troca desse texto.

## 1. Conferência curta para o dia da apresentação

Use monitores USB em 115200 baud e New Line. Se o coletor estiver aberto com portas ocupadas, encerre a coleta antes de abrir os monitores RX e RUÍDO. Envie cada comando separadamente e espere sua confirmação.

1. No TX, envie x para parar.
2. No RUÍDO, envie 0.
3. No RX, envie x, depois s e depois d.
4. No TX, envie s e depois t.
5. O RX deve resumir dez OK, demais classes zero e rodada_valida=SIM, se a bancada estiver funcionando.

Essa é uma verificação opcional da montagem no dia, não uma exigência de repetir as sete coletas concluídas.

## 2. Teste de predição solicitado pelo professor

1. Pare o TX com x. Selecione no RUÍDO o modo escolhido (0, 1, 2 ou 3).
2. Anote a técnica. Cada integrante prevê os quatro números para dez quadros e sua justificativa antes do envio. Use as frequências do relatório; previsões inteiras somam dez.
3. No RX, envie x, depois s para SUM8 ou c para CRC8, e depois d.
4. No TX, envie a mesma técnica e depois t.
5. Compare previsão e resultado, incluindo contadores de validade. Não prometa dez resultados idênticos à média de mil.

| Integrante | OK | CORROMPIDO | PERDIDO | NAO_DETECTADO | Justificativa |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |

Técnica e modo escolhidos: __________. Resultado observado: __________.

## 3. Mostrar por que SUM8 deixa passar um erro

1. Pare TX com x. Envie 4 ao RUÍDO.
2. No RX: x, s, d. No TX: s, t.
3. O padrão tenta apagar o bit 7 de p0/p1, que o TX fixou em 1. Com atuação correta, reduz a soma em 256, conserva resíduo zero e produz NAO_DETECTADO. Confira os corpos e os avisos do RUÍDO.
4. Compare com CRC8: RX x, c, d; TX c, t; RUÍDO permanece em 4. Esse padrão de dois bits deve ser descartado se a injeção estiver válida.
5. Explique que o modo 4 é extra; o caso natural 232 do relatório veio do modo 2. As rodadas usam novos bytes e não são o mesmo fluxo original/recebido entre técnicas.

## 4. Mostrar recuperação, se solicitado

1. Pare TX com x e envie 0 ao RUÍDO.
2. No RX: x, c, a. No TX: c, t.
3. O RX pausa sua própria UART por 350 ms após checar e auditar seq1. Mostre RX_DESLIGADO, RX_REATIVADO, RECUPERACAO_PERDA e RECUPERACAO_UART.
4. As três perdas e os tempos do relatório são o que foi medido naquele ensaio. Uma nova apresentação pode ter valores diferentes: registre os números mostrados, sem reutilizar o tempo antigo como nova medição.

Não troque para rx_recuperacao: o RX deste pacote já faz a recuperação e as quatro classificações.

## 5. Coletor opcional, para salvar os dez quadros comuns

1. Deixe TX parado. Feche apenas os monitores RX e RUÍDO; mantenha o monitor TX aberto.
2. Execute coletor/Abrir_Coletor.cmd, depois de extrair o ZIP em pasta gravável.
3. Confira RX COM6, RUÍDO COM3, técnica, modo e dez quadros. Clique Preparar e gravar.
4. Espere PRONTO E GRAVANDO. Só então envie s ou c e depois t no TX, conforme a indicação.
5. Espere o término e preserve a nova pasta em coletor/Coletas. O coletor não grava firmware nem inicia o TX.

O coletor normal prepara d/r; não use esse fluxo para a pausa a. Se aparecer revisão, preserve os logs e não trate a tentativa como válida só porque terminou. Usar o coletor e monitores nas mesmas portas ao mesmo tempo causa conflito.

## Comandos resumidos

| Placa | Comandos |
|---|---|
| TX | s SUM8; c CRC8; t dez; g mil; x parar |
| RX | s SUM8; c CRC8; d dez; r mil; a dez com pausa; x cancelar |
| RUÍDO | 0 desligado; 1 um alvo; 2 dois alvos; 3 rajada; 4 extra SUM8 |

O TX reinicia a sequência em zero em cada rodada. Selecione a mesma técnica nos dois com a rodada parada. Todos os integrantes precisam explicar o sistema inteiro.
