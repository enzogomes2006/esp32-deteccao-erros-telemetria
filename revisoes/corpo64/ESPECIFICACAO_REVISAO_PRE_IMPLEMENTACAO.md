# Revisão proposta: ruído em todo o corpo do quadro

Estado: proposta escrita antes das alterações desta revisão. Firmware novo e validação física ainda pendentes.

Este documento não afirma que a especificação da versão original antecedeu o código. Registra o projeto da próxima revisão. Os resultados existentes continuam pertencendo ao ensaio com ruído restrito ao payload; não demonstram o comportamento aqui proposto.

## 1. Objetivo e limites

Incluir LEN, sequência, payload e CHK no sorteio dos modos de ruído. O marcador permanece preservado: o injetor primeiro o reconhece na linha limpa e só depois arma os pulsos. A cópia de depuração também permanece preservada.

Isso amplia o ensaio aos 64 bits do corpo, mas não demonstra resistência à corrupção do marcador, dos bits UART de start/stop ou a qualquer falha elétrica possível. A recuperação após perda de recepção continua exigindo um ensaio separado.

## 2. Formato, detector e bancada

Preservar o formato existente:

`A5 5A C3 3C | 07 seqL seqH p0 p1 p2 p3 CHK`

| Índice no corpo | Campo | Alvo nos modos 1 a 3 |
| --- | --- | --- |
| 0 | LEN, valor original 07 | Sim |
| 1 e 2 | Sequência uint16, little-endian | Sim |
| 3 a 6 | Payload de quatro bytes | Sim |
| 7 | CHK | Sim |

Referência: `D5 6A B4 4B | oito bytes originais do corpo`.

Manter SUM8 e CRC-8/SMBUS com os parâmetros já documentados, uma técnica por rodada. A checagem cobre os oito bytes recebidos, incluindo CHK. Resíduo esperado zero. Manter UART 115200 8N1, pausas do TX, pinos, resistores, transistor e ligações. O TX pode permanecer igual, sujeito à conferência de compatibilidade antes da coleta.

## 3. Sorteio e injeção física

- Modo 0: nenhuma máscara aplicada.
- Modo 1: sortear um alvo entre 64 posições, índice de byte = posição / 8, bit = posição % 8.
- Modo 2: sortear duas posições distintas entre as mesmas 64 posições. Podem estar no mesmo byte.
- Modo 3: sortear um dos oito bytes, comprimento de 3 a 8 bits e uma posição inicial que mantenha a rajada dentro desse byte.
- Modo 4: preservar a demonstração específica de colisão SUM8 nos dois primeiros bytes do payload; é um controle adicional, fora das sete condições obrigatórias.

Manter a captura de borda por hardware e os prazos por bit. Puxar a linha para zero não garante uma inversão: registrar separadamente máscara escolhida e mudança realmente observada. Preservar os diagnósticos de atraso, amostragem, efeito elétrico e sincronismo. Desarmar ao terminar o oitavo byte e antes da referência.

## 4. Decisão do RX antes da referência

Ao reconhecer o marcador, o parser espera oito bytes: esse é o tamanho máximo e único do formato suportado. Um LEN corrompido não pode comandar uma leitura arbitrária nem consumir a cópia seguinte.

1. Corpo incompleto ou LEN diferente de 07: registrar falha de enquadramento, sem checagem concluída; resultado do evento de dados = PERDIDO.
2. Corpo completo com LEN=07: calcular o resíduo e registrar a decisão imediatamente.
3. Resíduo diferente de zero: DESCARTADO/CORROMPIDO, sem entregar payload e sem confiar na sequência desse corpo para avançar o controle do enlace.
4. Resíduo zero: ACEITO pela checagem. Registrar a sequência recebida e aplicar a regra de ordenação uint16, distinguindo aceitação pelo detector da entrega em ordem à aplicação.
5. Só depois da decisão, processar a referência para auditoria. Ela nunca recalcula nem altera a decisão, o payload entregue ou o estado de sequência do enlace.

Um erro que escape do detector também pode alterar a sequência. Isso é uma limitação real do detector; não pode ser ocultado corrigindo o enlace pela referência.

## 5. Sequência, perdas e contagem: alteração necessária

A regra atual de usar a sequência de todo corpo descartado deixa de ser válida. Substituí-la, antes de autorizar a coleta, por duas trilhas explícitas:

- **Enlace:** sequência dos corpos aceitos, eventos descartados sem sequência confiável, eventos malformados e timeouts. Usar comparação uint16 de meia faixa; repetidos e atrasados não são novas entregas. Um descarte já observado não pode ser contado novamente como perda em uma lacuna posterior. Quando não for possível atribuir uma posição exata, registrar a ambiguidade, sem inventar uma sequência.
- **Auditoria experimental:** guardar o resultado imutável de cada evento e associá-lo posteriormente à referência válida correspondente. A sequência original da referência identifica o registro experimental; não passa a ser a sequência usada pelo enlace. Registrar ambas separadamente.

Como o marcador é preservado neste modelo, cada evento de dados reconhecido pode receber um identificador local crescente. A associação à referência exige ordem temporal inequívoca: um evento pendente, referência completa antes do próximo evento e ausência de conflitos. Uma referência isolada não prova que o quadro foi perdido, e referência ausente não prova perda do quadro de dados. Associação ambígua ou referência inválida torna a medição incompleta/inválida.

As lacunas e os timeouts são diagnósticos do enlace. Não gerar uma segunda classificação definitiva para um evento já checado. Antes da coleta física, os testes de software devem demonstrar como eventos sem sequência confiável são conciliados com a próxima sequência aceita. Se essa conciliação for ambígua, o programa deve declará-la; não publicar uma tabela aparentemente completa.

Manter prazos iniciais de 60 ms para bloco/referência e 250 ms para silêncio do enlace, com passo nominal de 100 ms. Revalidar as origens e os efeitos desses prazos na lógica nova. Nenhum prazo de enlace é reiniciado pela referência.

## 6. Classificação e logs novos

Cada posição experimental pode ter uma única classificação final:

| Classe | Regra |
| --- | --- |
| OK | Checagem aceitou e payload coincide com referência válida |
| CORROMPIDO | Checagem concluída e resíduo diferente de zero |
| PERDIDO | Evento de dados não chegou à checagem por LEN inválido, corpo incompleto ou perda inferida pelo enlace sem duplicar descartes |
| NAO_DETECTADO | Checagem aceitou e payload difere de referência válida |

Manter a definição de não detectado baseada no payload, como no enunciado. Se apenas a sequência estiver errada e a checagem passar, destacar `cabecalho_diverge=SIM` e seus efeitos no enlace; não esconder o caso na tabela nem chamá-lo de payload corrompido.

O log do injetor deve substituir `byte_payload` por `byte_corpo` (0 a 7), acrescentar o nome do campo e manter máscara-alvo, bytes antes/depois, mudança real, timestamp e flags de sincronismo. O modo 0 deve permitir conferir todos os oito bytes.

O RX deve distinguir pelo menos: `evento_id`, `seq_rx`, `seq_referencia`, `seq_enlace_esperada`, LEN recebido, resíduo ou NA, decisão anterior à cópia, classe experimental, igualdade do payload, divergência do cabeçalho, tempos, corpos hexadecimais e motivo. Não reutilizar o nome `seq_enlace` para uma sequência obtida da referência.

O coletor e o validador de CSV precisam aceitar o novo esquema e cruzar os oito bytes do corpo com as amostras do injetor. Ausência de referência, mudança inesperada ou conflito de identificação deve produzir diagnóstico explícito.

## 7. Validação antes das sete coletas

Antes de gravar nas placas, compilar e testar o parser/contagem com vetores que incluam:

- Baseline; alteração sem efeito em bit já zero; erro somente em CHK.
- LEN corrompido, corpo parcial e retomada no próximo marcador.
- Sequência corrompida em corpo descartado; dois descartes consecutivos; descarte seguido de lacuna, sem duplicação.
- Colisão SUM8 com payload incorreto e colisão com sequência incorreta: decisão permanece anterior à auditoria.
- Duplicados, atrasados e passagem 65535 para 0.
- Referência ausente, inválida ou sem evento correspondente; nenhuma aceitação fictícia.
- Compatibilidade entre os novos logs e o coletor; uma classificação por posição e soma consistente.

Depois, na bancada, conferir dez quadros por modo e os registros de sincronismo. Só iniciar as sete condições de mil quadros quando RX, injetor e coletor passarem nessa validação. Hardware e latência só podem ser confirmados pelas placas, não pela compilação.

Guardar as novas coletas em série separada: SUM8 modos 0 a 3, CRC8 modos 1 a 3. Recalcular tabelas e procurar um erro não detectado real; ampliar a coleta se ele não aparecer. Não misturar contagens da série antiga com a revisão.

## 8. Próximos arquivos a alterar

1. Injetor: sorteio, índices dos logs e identificação de campo.
2. RX: decisões para LEN inválido, eventos sem sequência confiável, auditoria separada e contagem sem duplicação.
3. Coletor/validador: novo esquema e cruzamento do corpo completo.
4. Documentação e relatório: atualizar depois da validação, identificando a revisão e suas limitações.

Este documento é uma proposta prévia, não uma declaração de conformidade já demonstrada. Ajustes de projeto precisam ser registrados aqui antes da respectiva implementação; a validação pode revelar limitações adicionais.

## 9. Decisões detalhadas registradas antes da implementação

O resultado experimental e a entrega ao aplicativo são coisas distintas. `ACEITO` significa resíduo zero; duplicado/atrasado pode ter resíduo zero sem nova entrega. `seq_experimento` identifica a posição da tabela; para eventos recebidos é associada pela referência depois da decisão. `seq_rx` conserva os bytes recebidos. Não confundir esses campos.

O controle do enlace conserva a próxima sequência após o último aceito em ordem. Cada descarte ou evento malformado acrescenta um evento anônimo desde essa âncora. Ao aceitar uma sequência adiante, calcula a lacuna uint16 e desconta os eventos anônimos: apenas o restante constitui evidência de ausência de eventos. Timeouts já registrados nesse intervalo também são descontados, impedindo dupla contagem. Delta menor que o número de anônimos, sequência antiga ou contagem incompatível gera diagnóstico de ambiguidade, sem inventar a identidade dos descartados. A aritmética de meia faixa só ordena distâncias menores que 32768.

O timeout de silêncio começa no primeiro marcador e é renovado pelo progresso do bloco de dados (marcador, corpo completo ou timeout do corpo). Após 250 ms sem esse progresso, registra ausência de um evento; cada ausência adicional exige mais 100 ms. Referências não renovam esse prazo. Esses registros são evidência agregada, não linhas duplicadas de eventos já auditados.

Quando todos os eventos da janela foram associados inequivocamente e classificados, a tabela experimental pode estar completa mesmo que uma colisão tenha corrompido a sequência aceita pelo enlace. Nesse caso a divergência e a ambiguidade do enlace precisam aparecer separadamente. Não chamar essa tabela de prova de entrega correta em sequência.

Para preencher posições sem evento de dados reconhecido, exigir cumulativamente: término observado da janela ou timeout suficiente; número exato de ausências sustentado pelo enlace; ausência de ambiguidade do enlace; nenhuma divergência de cabeçalho em corpo aceito auditado. Caso contrário, declarar rodada incompleta/inválida. Uma referência órfã invalida a associação; não gera uma linha PERDIDO por si só.

A pausa automática de recuperação fica disponível apenas como ensaio separado de dez quadros, com ruído desligado. Ela começa após auditar a posição 1, mantém a UART desligada por 350 ms e mede a primeira aceitação posterior. A comprovação física do novo comportamento continua pendente.

O novo CSV terá, nesta ordem: `evento_id,seq_experimento,seq_rx,seq_referencia,seq_enlace_esperada,seq_enlace_proxima,tecnica,len_rx,residuo,decisao,status,payload_confere,cabecalho_diverge,entregue_aplicacao,verificacao_us,intervalo_inicio_us,recuperacao_ms,corpo_rx_hex,corpo_tx_hex,motivo,decisao_us,referencia_us`. Perdas inferidas sem evento usarão `evento_id=0` e referências/corpos ausentes, nunca uma cópia inventada.

O coletor deverá exigir a confirmação explícita `escopo=CORPO64` do injetor, além do modo. Isso evita aceitar por engano o firmware anterior. A revisão automática deverá comprovar, por evento, que a linha DECISAO_ANTES_DA_COPIA antecede a linha final de auditoria, além de conferir os timestamps. Os oito bytes serão registrados pelo injetor em todos os modos, inclusive os não selecionados, para cruzamento integral do corpo.
