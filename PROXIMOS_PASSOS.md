# O que fazer agora — checklist da revisão CORPO64

**Comece pela gravação de RX e RUÍDO e pelo teste de dez quadros sem ruído.** O TX anterior e os fios continuam compatíveis.

Os programas novos compilaram e passaram nos testes de software. Os itens físicos abaixo aguardam confirmação pelos logs da revisão; os dados antigos são de ruído apenas no payload.

## 1. Preparar os arquivos e gravar

- [ ] Baixar o repositório completo: na página inicial do GitHub, clicar **Code → Download ZIP** e extrair. O repositório é privado: entre na sua conta para acessar.
- [ ] Dentro da pasta extraída, usar **`revisoes/corpo64/`**. Os programas da pasta `firmware/` na raiz são da série anterior.
- [ ] Conectar os três ESP32 e identificar a porta de cada placa na Arduino IDE.
- [ ] Parar o TX com `x` e fechar o coletor e os Monitores Seriais antes de gravar.
- [ ] Gravar [o novo RX](revisoes/corpo64/firmware/rx/rx.ino) na placa RX, mantendo `sequence_link.h` na mesma pasta.
- [ ] Gravar [o novo RUÍDO](revisoes/corpo64/firmware/noise/noise.ino) na placa RUÍDO, mantendo `capture_schedule.h` e `noise_targets.h` na mesma pasta.
- [ ] Conferir as mensagens `RX CORPO64 v1.0` e `RUIDO CORPO64 v2.0` nos respectivos monitores.

Configuração: **ESP32 Dev Module / WROOM clássico, CPU 240 MHz, Arduino-ESP32 3.3.12**. O [TX compatível está incluído](revisoes/corpo64/firmware/tx/tx.ino), mas pode manter o anterior instalado. A montagem não muda.

## 2. Primeiro teste: SUM8, modo 0, dez quadros

- [ ] Deixar o TX parado e seu Monitor Serial aberto em **115200 baud / New Line**.
- [ ] Fechar os Monitores Seriais do RX e do RUÍDO para liberar suas portas.
- [ ] Abrir **`revisoes/corpo64/coletor/Abrir_Coletor.cmd`**, na pasta extraída no computador.
- [ ] Selecionar as portas corretas de RX e RUÍDO, **SUM8**, **modo 0**, **10 quadros**.
- [ ] Clicar **Preparar e gravar** e aguardar **PRONTO E GRAVANDO**.
- [ ] No Monitor Serial do TX, enviar `s` e depois `t`, separadamente.
- [ ] Aguardar o coletor salvar a pasta e abrir `revisao.txt`.
- [ ] Confirmar **10 OK, CORROMPIDO=0, PERDIDO=0, NAO_DETECTADO=0**, `rodada_valida=SIM`, **80 amostras do RUÍDO** e revisão automática sem problemas.

Se essa conferência falhar, guarde os logs e resolva a falha antes de começar os ensaios com ruído. Não altere as contagens manualmente.

**Primeiro arquivo para encaminhar para análise: `revisao.txt` da pasta que o coletor acabou de salvar.**

## 3. Validar os outros modos com dez quadros

Para cada linha, parar o TX, preparar uma nova coleta, aguardar PRONTO e só então enviar os comandos no TX.

| Concluído | Técnica | Modo | Quadros | Comandos no TX |
| --- | --- | --- | --- | --- |
| [ ] | SUM8 | 1 | 10 | `s`, depois `t` |
| [ ] | SUM8 | 2 | 10 | `s`, depois `t` |
| [ ] | SUM8 | 3 | 10 | `s`, depois `t` |
| [ ] | CRC8 | 1 | 10 | `c`, depois `t` |
| [ ] | CRC8 | 2 | 10 | `c`, depois `t` |
| [ ] | CRC8 | 3 | 10 | `c`, depois `t` |

As contagens aleatórias podem variar. O critério é passar na revisão automática, ter uma classificação por posição e sincronismo confirmado. LEN corrompido pode aparecer como PERDIDO; um alvo já zero pode não causar mudança. O guia também descreve o modo extra 4 como controle adicional; ele não substitui as sete condições abaixo.

## 4. Fazer as sete coletas novas de mil quadros

Iniciar somente depois da validação curta. Selecionar **1000 quadros** no coletor.

| Concluído | Técnica | Modo | Quadros | Comandos no TX após PRONTO |
| --- | --- | --- | --- | --- |
| [ ] | SUM8 | 0 | 1000 | `s`, depois `g` |
| [ ] | SUM8 | 1 | 1000 | `s`, depois `g` |
| [ ] | SUM8 | 2 | 1000 | `s`, depois `g` |
| [ ] | SUM8 | 3 | 1000 | `s`, depois `g` |
| [ ] | CRC8 | 1 | 1000 | `c`, depois `g` |
| [ ] | CRC8 | 2 | 1000 | `c`, depois `g` |
| [ ] | CRC8 | 3 | 1000 | `c`, depois `g` |

Cada transmissão leva aproximadamente 101 segundos. O coletor cria uma pasta por execução em **`revisoes/corpo64/coletor/Coletas/`**, na cópia extraída no computador.

Guardar cada pasta completa, com:

- `rx.csv` e `ruido.csv`;
- `rx_bruto.log` e `ruido_bruto.log`;
- `metadados.json` e `revisao.txt`.

Encaminhar as sete pastas completas, preferencialmente em um ZIP, para consolidar os resultados. Se houver falha, preservar a pasta e identificar a repetição. Não misturar esses registros com as coletas antigas de payload ou com os dados sintéticos dos testes de software.

## 5. Registrar as evidências complementares

- [ ] Guardar a saída do TX em controles de dez quadros para SUM8 e CRC8, incluindo `calculo_us`.
- [ ] Guardar os tempos de verificação do RX e os intervalos entre quadros dos novos CSVs.
- [ ] Realizar o [ensaio separado de recuperação, seção 5 do guia](revisoes/corpo64/LEIA_PRIMEIRO.md#5-recuperação-após-perda--controle-separado), com ruído desligado, e preservar os logs de desligamento, reativação e recuperação.
- [ ] Localizar um erro não detectado real nas novas coletas e cruzar os bytes com o log do injetor. Se não aparecer, coletar mais quadros.

O ensaio de recuperação é separado das sete condições aleatórias. Não comparar timestamps absolutos entre placas: seus relógios têm origens diferentes.

## 6. Atualizar os materiais depois dos dados físicos

- [ ] Recalcular as duas tabelas, taxas e gráficos com as novas coletas válidas.
- [ ] Explicar no relatório o novo alcance do ruído e o tratamento do cabeçalho.
- [ ] Incluir o caso real de erro não detectado, com bytes, logs e cálculo manual.
- [ ] Atualizar tempos, recuperação, conclusões e apresentação.
- [ ] Publicar os novos CSVs e o PDF revisado no GitHub, identificando a revisão. Preservar os ensaios anteriores como históricos.
- [ ] Conferir o link do GitHub no PDF e o acesso do professor ao repositório privado.
- [ ] Ensaiar a apresentação de 20 minutos, a explicação do código e a predição dos próximos dez quadros; depois entregar e realizar a defesa.

A especificação prévia registrada se refere a esta revisão. Ela não comprova a ordem de elaboração da versão original. As orientações do curso sobre assistência de IA continuam aplicáveis; concluir este checklist não garante a aceitação acadêmica.

## Referências práticas

- [Guia completo da revisão e comandos](revisoes/corpo64/LEIA_PRIMEIRO.md).
- [Estado dos testes de software e compilação](revisoes/corpo64/validacao/STATUS_VALIDACAO.json).
- [Guia dos fios e da montagem](montagem/GUIA_MONTAGEM_FISICA_ESP32.md).

**O próximo passo imediato é concluir a seção 2 e conferir seu `revisao.txt`.**
