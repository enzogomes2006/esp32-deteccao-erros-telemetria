# Demonstração CORPO64

Use RX CORPO64 v1.0, RUÍDO CORPO64 v2.0 e o TX compatível da pasta revisoes/corpo64/firmware/. Preserve os .h nas pastas dos sketches. A montagem está em montagem/GUIA_MONTAGEM_FISICA_ESP32.md. As portas variam; identifique cada placa na máquina usada.

## Coletor v3

1. Com TX parado (`x`), feche Monitores Seriais de RX e RUÍDO.
2. Baixe o repositório inteiro e extraia o ZIP. Abra revisoes/corpo64/coletor/Abrir_Coletor.cmd com Coletor.ps1 e Revisar.ps1 ao lado.
3. Selecione portas, técnica, modo e quantidade. Clique Preparar e gravar.
4. Espere PRONTO E GRAVANDO. No TX, envie `s` para SUM8 ou `c` para CRC8, depois `t` para dez ou `g` para mil. Envie um comando por vez.
5. Espere COLETA SALVA e confira revisao.txt. Preserve a pasta inteira, inclusive quando inválida.

Uma rodada sem ruído deve produzir dez OK e 80 amostras. Com ruído, o alvo pode já ser zero. LEN inválido aparece como PERDIDO, sem checagem. As contagens variam e não devem ser copiadas do relatório como previsão exata.

## Comandos manuais

USB: 115200 baud, New Line. Feche o coletor antes de abrir os três monitores.

| Placa | Comandos |
|---|---|
| TX | x para; s seleciona SUM8; c CRC8; t transmite dez; g mil |
| RX | x cancela; s SUM8; c CRC8; d prepara dez; r mil; a dez com pausa |
| RUÍDO | 0 desligado; 1 um alvo; 2 dois alvos; 3 rajada; 4 controle extra |

Prepare sempre RX antes de iniciar TX. Para um teste manual SUM8/0/dez: RUÍDO `0`; RX `x`, `s`, `d`; TX `s`, `t`. Para CRC8, troque s por c no RX e TX.

## Colisão e recuperação

Os casos naturais para explicar são SUM8/2, sequências 415 e 553. Em cada um, um byte do payload e o CHK perdem o bit 7; a soma diminui 256 e mantém resíduo zero. O modo 4 é um controle adicional, separado das sete condições aleatórias.

Recuperação manual, sem ruído: TX `x`; RUÍDO `0`; RX `x`, `s`, `a`; TX `s`, `t`. A UART do RX pausa por 350 ms após auditar seq 1. Guarde o log inteiro, incluindo RX_DESLIGADO, RX_REATIVADO, RECUPERACAO_UART, RESUMO e VALIDACAO. O resultado medido foi 7 OK e 3 PERDIDOS, retomada 38,691 ms após reativar; nova execução pode ter pequenas variações.

## Predição e defesa

Antes do cenário escolhido pelo professor, cada integrante anota OK, CORROMPIDO, PERDIDO e ND previstos, somando dez, com justificativa. Só depois executar e comparar com o resultado e a validade. Explicar separadamente resíduo, entrega ao aplicativo e auditoria da cópia. Este roteiro não declara que a avaliação já ocorreu.
