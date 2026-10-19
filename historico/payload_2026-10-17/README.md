# Deteccao de erros em telemetria com ESP32

Trabalho prático de **Enzo Aurélio**, IFMT - Octayde, disciplina **TCD - Transmissão e Comunicação de Dados**, professor **Ruy Oliveira**.

Tres ESP32 formam um enlace UART com ruido fisico: TX transmite, RX decide aceitar ou descartar usando SUM8 ou CRC-8/SMBUS, e RUIDO aciona um BC547 para forcar bits do payload a zero. A referencia de depuracao e consultada apenas depois da decisao do RX.

Repositório **privado**: https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria. Para acesso pelo professor ou pela equipe, o proprietário deve conceder permissão às contas correspondentes.

**[O que fazer agora — checklist de gravação, testes e conclusão](PROXIMOS_PASSOS.md).** Comece pelo teste SUM8, modo 0, dez quadros; confira o `revisao.txt` antes das próximas coletas.

## Revisão CORPO64 — preparada, validação física pendente

Foi preparada uma revisão com ruído em LEN, sequência, payload e CHK. Os programas compilaram e passaram nos testes de software; **ainda não há novas coletas físicas dessa revisão**. A montagem e o TX continuam compatíveis.

Use o [pacote da revisão e guia para gravar/testar](revisoes/corpo64/LEIA_PRIMEIRO.md), a [especificação registrada antes desta revisão](revisoes/corpo64/ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md) e o [estado de validação](revisoes/corpo64/validacao/STATUS_VALIDACAO.json). O RX, o injetor e o coletor novos estão juntos nessa pasta. Não combinar versões entre as duas séries.

Os códigos da pasta `firmware/`, as tabelas abaixo, os CSVs em `dados/` e o relatório PDF permanecem como documentação da **série anterior com ruído somente no payload**. Não representam medições CORPO64. O relatório só poderá incorporar conclusões da revisão depois de coletadas e verificadas as novas evidências. O registro prévio da revisão não comprova a ordem de elaboração da versão original.

## Arquivos principais

- [Guia da montagem física em PDF](montagem/GUIA_MONTAGEM_FISICA_ESP32.pdf) e [guia dos fios no GitHub](montagem/GUIA_MONTAGEM_FISICA_ESP32.md): nove jumpers, resistores, BC547 e furos da protoboard P2.
- [Relatorio final em PDF](Relatorio_Final_ESP32.pdf) e [fonte LaTeX](latex/main.tex).
- [Sete PDFs originais por etapa](pdfs_por_etapa/README.md): partes 1 a 7, preservadas para consulta ao desenvolvimento.
- [Especificacao do protocolo](ESPECIFICACAO_PROTOCOLO.md).
- [Guia da demonstracao e comandos](GUIA_DEMONSTRACAO.md).
- [Roteiro para os 20 minutos](APRESENTACAO_20_MIN.md).
- [TX](firmware/tx/tx.ino), [RX completo v0.2](firmware/rx/rx.ino) e [RUIDO captura v1.3.1](firmware/noise/noise.ino).
- [Sete coletas originais](dados/), com CSVs, logs e metadados; [resultados consolidados](resultados_consolidados.json).
- [Evidencias complementares](evidencias/) dos tempos e da recuperacao.
- [Coletor CSV opcional](coletor/) e [manifesto de integridade](MANIFESTO_SHA256.json).

## Resultados medidos — série anterior, ruído no payload

Cada linha representa 1000 quadros da serie final de 17/10/2026.

| Tecnica | Modo | OK | Corrompido | Perdido | Nao detectado |
|---|---:|---:|---:|---:|---:|
| SUM8 | 0 | 1000 | 0 | 0 | 0 |
| SUM8 | 1 | 469 | 531 | 0 | 0 |
| SUM8 | 2 | 213 | 783 | 0 | 4 |
| SUM8 | 3 | 44 | 956 | 0 | 0 |
| CRC8 | 1 | 443 | 557 | 0 | 0 |
| CRC8 | 2 | 195 | 805 | 0 | 0 |
| CRC8 | 3 | 26 | 974 | 0 | 0 |

A taxa de nao deteccao e `ND / (CORROMPIDO + ND)`. Para SUM8/modo 2: `4/787 = 0,5083%`. Os quadros sem erro nao entram nesse denominador. O caso natural de sequencia 232 e analisado byte a byte no relatorio. Zero erros nao detectados nas rodadas CRC8 nao significa garantia contra qualquer corrupcao possivel.

## Abrir e gravar os programas da série anterior

Use **ESP32 Dev Module / WROOM classico**, CPU **240 MHz**, pacote **Arduino-ESP32 3.3.12** e monitores USB a **115200 baud**, com **New Line**. O injetor usa APIs especificas desse hardware/core.

Abra cada `.ino` na sua propria pasta e grave apenas na placa correspondente. Mantenha `capture_schedule.h` junto de `firmware/noise/noise.ino`. O conteudo do firmware e o das coletas foi preservado em relacao aos arquivos validados; as mensagens de boot de versoes candidatas tambem foram preservadas.

As portas observadas na serie final foram TX COM5, RX COM6 e RUIDO COM3. Confirme as portas no computador usado na apresentacao.

Com TX parado, uma verificacao curta sem ruido usa: RUIDO `0`; RX `x`, depois `s`, depois `d`; TX `s`, depois `t`. Envie cada comando separadamente. O esperado e dez OK e `rodada_valida=SIM`. O [guia completo](GUIA_DEMONSTRACAO.md) explica CRC8, modos de ruido, colisao SUM8, recuperacao e o coletor.

## Limites e demonstracao

O ruido dos ensaios atinge apenas o payload, somente de 1 para 0. Os registros tem validacao de sincronismo e comparacao com a referencia; nao certificam toda montagem ou condicao de temporizacao possivel. Os resultados sao medicoes da serie indicada, nao previsoes exatas de novas rodadas.

As sete coletas estao incluidas. A demonstracao presencial e a predicao individual dos dez quadros precisam ser realizadas ao vivo; sua realizacao nao e afirmada por este repositorio. Todos os integrantes precisam explicar o sistema inteiro.
