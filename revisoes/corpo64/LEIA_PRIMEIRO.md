# Revisão CORPO64 — começar por aqui

**Estado: programas preparados para validação nas placas. Nenhuma coleta física nova está incluída.** A versão anterior, seus sete experimentos e seu relatório continuam preservados e se referem ao ruído somente no payload.

Esta revisão sorteia alvos em **LEN, sequência, payload e CHK**, ou seja, os 64 bits do corpo do quadro. O marcador de início e a cópia de depuração ficam preservados. O resultado é uma ampliação do alcance experimental; não demonstra corrupção do marcador ou dos bits UART de start/stop.

## O que já foi preparado

- [Especificação prévia](ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md), registrada antes de criar os programas desta revisão.
- [RX CORPO64 v1.0](firmware/rx/rx.ino), com cabeçalho recebido separado da identidade da auditoria.
- [RUÍDO CORPO64 v2.0](firmware/noise/noise.ino), com sorteio no corpo inteiro e registro de todos os oito bytes.
- [TX compatível](firmware/tx/tx.ino), idêntico ao TX anterior.
- [Coletor v3](coletor/Abrir_Coletor.cmd), com conferência do escopo CORPO64, resíduo, máscaras, bytes recebidos e ordem da decisão.
- Testes de software, logs de compilação e registro de integridade em `testes/` e `validacao/`.

O registro prévio documenta a ordem desta revisão. Não é prova de que a especificação original foi escrita antes da implementação original. Os programas e a documentação tiveram assistência de IA; esta revisão não muda a regra de avaliação informada pelo professor nem garante sua aceitação.

## 1. Gravar nas placas — mesmos fios

Use ESP32 Dev Module / WROOM clássico, CPU 240 MHz, Arduino-ESP32 3.3.12. A montagem continua a mesma: TX GPIO17, RX GPIO16, RUÍDO GPIO16 na linha limpa, GPIO18 na linha perturbada e GPIO19 na base pelo resistor; GND comum, dois resistores de 1 kΩ e BC547.

1. Pare o TX com `x`. Feche o coletor e os Monitores Seriais antes de gravar.
2. Na Arduino IDE, abra **`firmware/rx/rx.ino`** deste pacote e grave na placa RX. Mantenha `sequence_link.h` na mesma pasta.
3. Abra **`firmware/noise/noise.ino`** deste pacote e grave na placa RUÍDO. Mantenha `capture_schedule.h` e `noise_targets.h` na mesma pasta.
4. O TX anterior pode continuar instalado. Se precisar regravar, use **`firmware/tx/tx.ino`** deste pacote na placa TX.
5. Confira individualmente as portas das placas na IDE. COM3/COM5/COM6 da série antiga não são garantidas.

Abra os sketches pelas pastas completas extraídas do ZIP. Não cole apenas um trecho: os arquivos `.h` fazem parte dos programas.

Para conferir os firmwares, os Monitores Seriais devem mostrar `RX CORPO64 v1.0` e `RUIDO CORPO64 v2.0`. O injetor também informa `ESCOPO_RUIDO,CORPO64`. Se houver erro de relógio, UART compartilhada ou sincronismo, preserve a mensagem e interrompa a coleta.

## 2. Primeiro teste: dez quadros, sem ruído

1. Mantenha o TX parado. Deixe aberto somente o Monitor Serial do TX, em 115200 baud e New Line.
2. Feche os Monitores Seriais do RX e do RUÍDO: o coletor precisará dessas portas.
3. Abra **`coletor/Abrir_Coletor.cmd`**. Não precisa de Python nem de administrador.
4. Selecione as portas corretas, técnica **SUM8**, modo **0**, **10** quadros.
5. Clique **Preparar e gravar** e aguarde **PRONTO E GRAVANDO**.
6. No Monitor Serial do TX, envie `s` e depois `t`, um comando por vez.
7. Aguarde o coletor salvar a pasta. O esperado é **10 OK**, zero nas demais classes, `rodada_valida=SIM` e revisão automática sem problemas. O RUÍDO deve produzir **80 amostras**, oito por quadro.

As pastas ficam em `coletor/Coletas/`, com data/hora e configuração. Guarde `rx.csv`, `ruido.csv`, os dois logs brutos, `metadados.json` e `revisao.txt`. São as novas evidências físicas; os logs dos testes de software não as substituem.

## 3. Conferir o ruído com testes curtos

Repita dez quadros para SUM8 nos modos 1, 2 e 3; depois CRC8 nos modos 1, 2 e 3. Para cada rodada, pare o TX, selecione a configuração no coletor, prepare e só então inicie o TX. No TX, SUM8 é `s`, CRC8 é `c`; dez quadros é `t`.

As contagens variam: um alvo já zero pode não mudar nada. LEN alterado pode gerar PERDIDO por enquadramento; sequência alterada em um descarte não deve invalidar a rodada por si só. Exija sincronismo confirmado, oito amostras por quadro, coerência entre os corpos e uma classificação por posição experimental.

O modo **4** é um controle adicional, fora das sete coletas obrigatórias. Ele tenta apagar o bit 7 dos dois primeiros bytes do payload, que o TX fixa em 1. Com efeito físico confirmado, espera-se que SUM8 aceite os payloads errados (NAO_DETECTADO) e CRC8 os descarte (CORROMPIDO). Uma falha de sincronismo exige revisão da bancada/temporização, não alteração manual das contagens.

## 4. Sete coletas novas, depois da validação curta

| Ordem | Técnica no coletor e no TX | Modo | Quadros | Comandos no TX após PRONTO |
| --- | --- | --- | --- | --- |
| 1 | SUM8 | 0 | 1000 | `s`, depois `g` |
| 2 | SUM8 | 1 | 1000 | `s`, depois `g` |
| 3 | SUM8 | 2 | 1000 | `s`, depois `g` |
| 4 | SUM8 | 3 | 1000 | `s`, depois `g` |
| 5 | CRC8 | 1 | 1000 | `c`, depois `g` |
| 6 | CRC8 | 2 | 1000 | `c`, depois `g` |
| 7 | CRC8 | 3 | 1000 | `c`, depois `g` |

Cada condição leva aproximadamente 101 segundos de transmissão, além da preparação. Não misture estas pastas com as coletas antigas de payload. Ao concluir, disponibilize as sete pastas completas para recalcular as tabelas e atualizar o relatório. Se não aparecer erro não detectado real, será preciso coletar mais quadros; a simulação ou o modo extra 4 não substituem a identificação das evidências solicitadas.

Guarde também a saída do TX nos controles de dez quadros para SUM8 e CRC8, com `calculo_us`. O RX registra os tempos de verificação de cada corpo checado. Os programas preservam a mesma taxa e as mesmas pausas do TX, mas os tempos da nova bancada precisam ser registrados novamente para documentar a revisão.

## 5. Recuperação após perda — controle separado

Depois da triagem, pode-se usar o modo manual: feche o coletor, abra os três Monitores Seriais em 115200/New Line, coloque RUÍDO em `0`, prepare RX com `x`, `s`, `a`; só depois mande `s`, `t` no TX. A UART do RX fica desligada por 350 ms após auditar a posição 1. Guarde os logs completos `RX_DESLIGADO`, `RX_REATIVADO`, `RECUPERACAO_UART`, `RESUMO` e `VALIDACAO`. Esse é um ensaio de ausência de recepção; não é ruído elétrico e não entra nas sete condições aleatórias.

## Como interpretar os campos novos

- `seq_rx`: sequência que realmente chegou, inclusive se estiver errada.
- `seq_referencia` / `seq_experimento`: identidade usada depois da decisão para organizar a evidência experimental; não corrige o enlace.
- `seq_enlace_esperada` e `seq_enlace_proxima`: estado real do enlace antes/depois da decisão.
- `entregue_aplicacao`: aceitação em ordem pelo enlace. Um corpo com resíduo zero pode ser antigo/duplicado e não ser entregue de novo.
- `cabecalho_diverge`: aponta erro nos bytes LEN/seq, inclusive quando o detector aceitou. Payload correto com sequência errada não é prova de entrega correta.
- `DECISAO_ANTES_DA_COPIA`: registro anterior à auditoria; o coletor verifica sua coerência com a linha final.

Os relógios do RX e do injetor são independentes. Compare intervalos e ordem dentro de cada placa; não subtraia seus timestamps absolutos entre si.

## O que ainda falta para concluir a revisão

Gravar e conferir nas placas; realizar a triagem curta; coletar as sete séries; selecionar o caso real byte a byte; repetir/documentar tempos e recuperação; atualizar relatório e tabelas com os novos dados; preparar a defesa e a predição ao vivo. Nada disso é declarado concluído pelos testes automáticos deste pacote.
