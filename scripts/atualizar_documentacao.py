"""Gera os materiais atuais a partir dos resultados consolidados CORPO64."""
from pathlib import Path
import csv, json

ROOT=Path(__file__).resolve().parents[1]
D=json.loads((ROOT/'resultados_consolidados.json').read_text(encoding='utf-8'))

def write(name,text):
    p=ROOT/name; p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(text.strip()+'\n',encoding='utf-8')

def fmt(n, places=3): return f'{n:.{places}f}'.replace('.',',')
def table(headers,rows):
    return '\n'.join(['| '+' | '.join(headers)+' |','| '+' | '.join('---' for _ in headers)+' |']+['| '+' | '.join(map(str,r))+' |' for r in rows])

COUNTS=table(['Técnica','Modo','OK','Corrompido','Perdido','Não detectado'],[[s['tecnica'],s['modo'],*[s['contagens'][k] for k in ['OK','CORROMPIDO','PERDIDO','NAO_DETECTADO']]] for s in D['series']])
RATES=table(['Condição','Erros checados','ND','Taxa ND','Perdidos por LEN'],[[f"{s['tecnica']}/{s['modo']}",s['denominador_nao_deteccao'],s['contagens']['NAO_DETECTADO'],'n.a.' if s['taxa_nao_deteccao'] is None else fmt(100*s['taxa_nao_deteccao'],4)+'%',s['contagens']['PERDIDO']] for s in D['series']])
TIMES=table(['Condição','N checados','Média (µs)','Mediana (µs)','Mín.','Máx.'],[[f"{s['tecnica']}/{s['modo']}",s['verificacao_rx_us']['n'],fmt(s['verificacao_rx_us']['media']),fmt(s['verificacao_rx_us']['mediana'],1),s['verificacao_rx_us']['min'],s['verificacao_rx_us']['max']] for s in D['series']])
INTERVALS=table(['Condição','N intervalos','Média (ms)','Mín. (ms)','Máx. (ms)'],[[f"{s['tecnica']}/{s['modo']}",s['intervalo_inicio_rx_us']['n'],fmt(s['intervalo_inicio_rx_us']['media']/1000),fmt(s['intervalo_inicio_rx_us']['min']/1000),fmt(s['intervalo_inicio_rx_us']['max']/1000)] for s in D['series']])
SOURCES=table(['Condição','Pasta original em revisoes/corpo64/dados/'],[[f"{s['tecnica']}/{s['modo']}",s['pasta_original']] for s in D['series']])

def main():
    report=f'''# Detecção de erros em telemetria sob ruído real

Enzo Aurélio · IFMT - Octayde · TCD - Transmissão e Comunicação de Dados

Professor Ruy Oliveira · Revisão CORPO64 · 19 de outubro de 2026

Repositório público: https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria

## Resumo

Este trabalho compara SUM8 e CRC-8/SMBUS em um enlace UART com três ESP32: transmissor, receptor e injetor de ruído físico. Na revisão CORPO64, o sorteio alcança LEN, sequência, payload e CHK, cobrindo os 64 bits do corpo do quadro. Sete rodadas válidas de 1.000 quadros totalizaram 7.000 posições experimentais e 56.000 amostras do injetor. SUM8 apresentou dois erros não detectados no modo 2, com taxa de 2/621 = 0,3221% entre os erros checados dessa condição. CRC8 apresentou zero casos não detectados nas três rodadas ensaiadas. Os tempos médios de verificação no RX foram {fmt(D['verificacao_rx_agregada_us']['SUM8']['media'])} µs e {fmt(D['verificacao_rx_agregada_us']['CRC8']['media'])} µs, respectivamente. Em controle separado, o RX permaneceu sem recepção por 350,443 ms e voltou a aceitar um quadro 38,691 ms depois de reativar a UART. As conclusões se restringem ao modelo de perturbação ensaiado e distinguem checagem, entrega à aplicação e auditoria posterior.

Palavras-chave: ESP32; UART; checksum; CRC8; ruído físico; telemetria.

## 1. Objetivo e identificação da série

O objetivo é comparar duas técnicas de detecção no mesmo formato de quadro e observar falhas reais de comunicação. O payload contém quatro bytes sintéticos, não medições de um sensor. Cada condição usa uma técnica por vez. A série física atual foi coletada em 19/10/2026 após sete testes curtos de dez quadros. Dois controles adicionais de dez quadros registraram os tempos de cálculo do TX.

O relatório anterior, de 17/10/2026, usava ruído restrito ao payload e está preservado em historico/payload_2026-10-17/. Seus dados continuam em dados/ na raiz. Nenhuma contagem antiga foi somada à CORPO64. As tabelas deste relatório usam somente revisoes/corpo64/dados/.

## 2. Bancada e modelo de ruído

Três ESP32 clássicos WROOM exercem os papéis TX, RX e RUÍDO. A configuração documentada dos programas é ESP32 Dev Module, CPU de 240 MHz e Arduino-ESP32 3.3.12. A montagem usa GND comum, dois resistores de 1 kΩ e um BC547. TX usa GPIO17, RX usa GPIO16; o injetor observa a linha limpa em GPIO16, a perturbada em GPIO18 e comanda a base pelo resistor em GPIO19. O guia de montagem e os esquemas do repositório detalham as ligações.

O injetor força a linha ao nível baixo em instantes selecionados. Um alvo em bit já zero não muda o byte. O log separa máscara selecionada de máscara efetivamente alterada e registra os oito bytes antes/depois, sincronismo e atrasos. Os resultados dependem da captura e amostragem dessa bancada; a revisão lógica dos registros não substitui uma medição de forma de onda ou calibração elétrica independente.

{table(['Modo','Seleção de alvos no corpo'],[['0','Sem pulsos de ruído'],['1','Uma posição entre os 64 bits'],['2','Duas posições distintas entre os 64 bits'],['3','Rajada contígua de 3 a 8 bits em um dos oito bytes'],['4','Controle extra nos bits 7 de P0 e P1; fora das sete rodadas']])}

O marcador e a referência de depuração são preservados. Os ensaios não cobrem corrupção deliberada do marcador nem dos bits UART de start/stop. A rajada do modo 3 fica dentro de um byte, conforme o sorteio implementado.

## 3. Protocolo serial e detectores

O enlace é UART 115200 baud, 8N1, unidirecional, sem ACK nem retransmissão. O quadro tem doze bytes:

```text
A5 5A C3 3C | 07 seqL seqH p0 p1 p2 p3 CHK
```

{table(['Campo','Bytes','Regra'],[['Marcador','4','A5 5A C3 3C; fora da checagem'],['LEN','1','07; sete bytes posteriores a LEN'],['Sequência','2','uint16 little-endian'],['Payload','4','Telemetria sintética; bits 7 de P0/P1 fixados em 1'],['CHK','1','SUM8 ou CRC8, conforme a rodada']])}

O RX reconhece o marcador, coleta oito bytes de corpo e exige LEN=7. Um LEN alterado não aumenta o limite de leitura. Se LEN for inválido, o quadro não chega à checagem e é classificado PERDIDO por enquadramento. O parser volta à busca de marcadores. Um marcador dentro do corpo é tratado como dado.

### 3.1. SUM8

O TX calcula CHK = -soma(LEN, seqL, seqH, p0, p1, p2, p3) módulo 256. O RX soma os oito bytes, incluindo CHK. Resíduo diferente de zero implica descarte. Alterações cuja diferença total seja múltipla de 256 mantêm o resíduo zero e podem produzir aceitação indevida.

### 3.2. CRC-8/SMBUS

O programa usa o polinômio x^8 + x^2 + x + 1, representação 0x07 sem o termo x^8, init=0, refin=false, refout=false e xorout=0. O TX calcula sobre sete bytes; o RX processa os oito e espera resíduo zero. O cálculo percorre cada byte MSB primeiro, enquanto a UART transmite seus bits LSB primeiro. O teste padrão da implementação é CRC8 de "123456789" = F4.

O CRC calcula um resto polinomial em GF(2), com XOR. O resultado experimental de zero falhas nesta série não deve ser convertido em garantia contra todo padrão de corrupção. LEN inválido é tratado pelo enquadramento antes da verificação de qualquer detector.

### 3.3. Decisão, enlace e referência

Após o quadro, o TX envia D5 6A B4 4B seguido de uma cópia dos oito bytes originais. A linha DECISAO_ANTES_DA_COPIA documenta a decisão antes da leitura da referência. A cópia serve para auditar e associar a posição experimental; não corrige o payload entregue nem o estado da sequência do enlace.

Corpos descartados não têm sequência confiável. O RX mantém eventos anônimos até conciliá-los com uma sequência aceita, sem transformar cada descarte em uma segunda perda. Sequências antigas ou repetidas não geram nova entrega. seq_rx conserva o que chegou, e seq_referencia/seq_experimento identificam a auditoria posterior. cabecalho_diverge registra divergências de LEN ou sequência, inclusive se um detector aceitar. ACEITO pelo detector e entregue_aplicacao são campos distintos.

Os timeouts são 60 ms para bloco/auditoria e 250 ms para ausência de progresso no enlace, com avanço de 100 ms por ausência adicional. Referências não renovam o prazo de enlace. Referência ausente, órfã, inválida ou associação ambígua invalida a medição; nenhum OK é inventado.

## 4. Método e conferência dos dados

Foram selecionadas quatro condições SUM8, modos 0 a 3, e três CRC8, modos 1 a 3, com mil quadros cada. O controle sem ruído CRC8 foi registrado entre os controles curtos, sem substituir nenhuma das sete condições. Cada pasta conserva rx.csv, ruido.csv, os dois logs brutos, metadados.json e revisao.txt. O manifesto fornece hashes SHA256. Os arquivos foram copiados byte a byte das pastas locais do coletor. Tentativas iniciais fora da série e controles curtos estão identificados em triagem/ e não entram nas tabelas principais.

O script scripts/consolidar.py recalcula contagens e resíduos, cruza os oito bytes com as amostras do injetor e verifica máscaras, efeito físico, sincronismo, atrasos, identidade das posições e a ordem de decisão/auditoria. Também compara os registros CSV com os logs brutos. A revisão original do coletor é executável separadamente. Nenhuma fixture sintética integra as medições físicas.

{table(['Classe','Critério experimental'],[['OK','Checagem aceitou e payload coincide com referência válida'],['CORROMPIDO','Checagem concluída; resíduo diferente de zero'],['PERDIDO','Sem checagem concluída: enquadramento ou ausência evidenciada'],['NAO_DETECTADO','Checagem aceitou, mas payload difere da referência válida']])}

Cada posição recebe uma única classe. A taxa de não detecção é ND/(CORROMPIDO+ND). OK e PERDIDO ficam fora desse denominador. Quando não há erro checado, a taxa é n.a., não zero por convenção. Um alvo já zero não constitui erro real. Contagens de condições diferentes não são amostras pareadas do mesmo ruído.

## 5. Resultados das sete rodadas

{COUNTS}

Cada linha soma 1.000 posições; cada rodada tem 8.000 amostras do injetor e rodada_valida=SIM. A conferência encontrou zero cabeçalhos divergentes em corpos aceitos nas sete rodadas. Divergências em corpos descartados ou malformados permanecem nos registros.

![Contagens por condição](revisoes/corpo64/graficos/contagens.svg)

Figura 1. Classes registradas por condição, com mil quadros em cada barra. Os dois ND do SUM8/2 são identificados na tabela e no estudo de caso; sua largura é pequena nessa escala.

### 5.1. Taxas e perdas de enquadramento

{RATES}

Na rodada SUM8/2 ocorreram 621 erros checados, sendo 619 detectados e 2 não detectados. A taxa é 0,3221%. Em relação aos mil quadros transmitidos, os dois casos são 0,2%; esse é outro denominador e não substitui a taxa definida. Os 90 perdidos são excluídos da taxa de falha do detector.

Todas as perdas registradas nas sete condições tiveram motivo LEN_INVALIDO. Portanto, nesta série PERDIDO representa falha de enquadramento observada, e não comprova desaparecimento completo do quadro. A ampliação do ruído ao cabeçalho explica por que a CORPO64 contém perdas mesmo com os marcadores preservados. O ensaio de recepção desligada é apresentado separadamente.

### 5.2. Dois casos naturais não detectados

As sequências 415 e 553 da rodada SUM8/2 foram aceitas e entregues à aplicação com payload errado. Em ambos os casos, o log do injetor confirma alteração de 1 para 0 nos dois alvos, sincronismo SIM e ausência de flags de amostragem ou pulso tardio.

{table(['Sequência','Campo','Transmitido','Recebido','Máscara real'],[[415,'P0','87','07','80'],[415,'CHK','E7','67','80'],[553,'P3','B3','33','80'],[553,'CHK','89','09','80']])}

Todos os valores de bytes e máscaras nessa tabela são hexadecimais. Os corpos completos são:

```text
seq 415 TX: 07 9F 01 87 BA 1E 13 E7
seq 415 RX: 07 9F 01 07 BA 1E 13 67
seq 553 TX: 07 29 02 EA CF D9 B3 89
seq 553 RX: 07 29 02 EA CF D9 33 09
```

Na sequência 415, a soma dos oito bytes transmitidos é 768, e a soma recebida é 512. Na sequência 553, as somas são 1.024 e 768. Apagar dois bits 7 reduz a soma em 128+128=256. Os dois valores de cada par continuam múltiplos de 256; o detector mantém resíduo zero. Em cada caso LEN e sequência permanecem intactos, enquanto payload e CHK mudam. A colisão foi observada em ruído aleatório do modo 2, sem usar o controle extra do modo 4.

O evento 416, correspondente à sequência 415, decide ACEITO em 709748318 µs e só depois registra referência em 709762509 µs. O evento 554, sequência 553, decide em 723686327 µs e recebe referência em 723700531 µs. Esses pares pertencem ao relógio do RX. Os timestamps absolutos do injetor não foram subtraídos dos timestamps do RX.

## 6. Tempos, taxa e overhead

### 6.1. Cálculo no transmissor

Controles separados de dez quadros, sem ruído, registraram 1 µs por cálculo SUM8 e 3 µs por cálculo CRC8. Em cada técnica, os nove intervalos após o primeiro quadro foram 101000 µs, ou 101 ms. O primeiro intervalo zero foi excluído. Os corpos fornecidos no chat foram cruzados com as referências das respectivas coletas curtas e coincidem. As transcrições estão em evidencias/corpo64/.

### 6.2. Verificação no receptor

{TIMES}

Os corpos com LEN inválido não chegaram à checagem e têm tempo NA. Foram excluídos das estatísticas de verificação, sem substituir ausência por zero. A média agregada SUM8 usa 3.774 checagens e vale {fmt(D['verificacao_rx_agregada_us']['SUM8']['media'])} µs. CRC8 usa 2.764 checagens e vale {fmt(D['verificacao_rx_agregada_us']['CRC8']['media'])} µs. Essas médias são ponderadas pelas amostras individuais, e não médias simples das linhas da tabela.

![Tempo médio de verificação](revisoes/corpo64/graficos/tempos_rx.svg)

Figura 2. Média de verificação do RX nas sete condições. Unidade: microssegundos. Os tamanhos amostrais estão na tabela. A resolução registrada é de microssegundos; o log e o processamento da bancada podem afetar os extremos.

### 6.3. Intervalos e custo de transmissão

{INTERVALS}

Os intervalos são medidos pelo RX entre inícios de quadros e usam 999 diferenças por rodada. A mediana é 101 ms em todas as condições. Não são tempos de recuperação nem incluem o primeiro zero.

Cada quadro de dados tem 4 bytes úteis e 8 de overhead: marcador 4, LEN 1, sequência 2 e CHK 1. A eficiência em bytes é 4/12=33,33%; overhead/total=66,67% e overhead/payload=200%. A cópia de depuração adiciona 12 bytes à instrumentação, totalizando 24 bytes e eficiência útil de 4/24=16,67% antes das pausas.

Em 8N1, cada byte ocupa dez bits na linha. Os 12 bytes de dados ocupam teoricamente 120/115200 s, cerca de 1,042 ms. Incluindo a cópia, o tempo teórico de serialização é cerca de 2,083 ms. Esses valores não incluem as pausas programadas de 10 ms após o marcador, 3 ms por byte do corpo e 10 ms antes da referência. O período observado de 101 ms resulta de todo o agendamento, não de uma mudança na taxa UART. Com quatro bytes úteis a cada 101 ms, a vazão útil aproximada é 39,60 bytes/s, ou 316,83 bit/s.

## 7. Recuperação após interrupção de recepção

O controle SUM8 de dez quadros usa ruído configurado em zero e desliga a UART do RX após auditar a sequência 1. É uma interrupção programada da recepção, sem alterar o firmware entre cálculo e recuperação. O log foi fornecido pelo usuário em dois trechos sobrepostos, unidos sem duplicar as linhas comuns; a procedência está em evidencias/corpo64/ORIGEM.md. Não há captura complementar do monitor do injetor para esse controle.

{table(['Evento do RX','Instante (µs)'],[['RX_DESLIGADO',94561283],['Primeira detecção de ausência',94797303],['RX_REATIVADO',94911726],['Aceitação da sequência 5',94950417]])}

A janela efetiva sem recepção foi 94911726-94561283=350443 µs, ou 350,443 ms. O tempo da reativação até a primeira aceitação foi 94950417-94911726=38691 µs, ou 38,691 ms. Desde a primeira detecção de perda até a aceitação foram 153114 µs, ou 153,114 ms. As medidas têm origens diferentes e não devem receber o mesmo rótulo.

O resumo registra 7 OK, 3 PERDIDOS (sequências 2, 3 e 4), zero CORROMPIDOS e zero ND. As sete auditorias somadas às três ausências evidenciadas contabilizam dez posições. O RX informa rodada_valida=SIM, zero ambiguidades e sequência esperada final 10. As perdas não são preenchidas com referências inventadas. Esse controle não entra nas sete condições de ruído.

## 8. Discussão e limites

Os dois casos SUM8 demonstram uma limitação concreta da soma modular: alterações no payload e no próprio detector podem preservar o resto. O CRC8 não apresentou ND nos modos ensaiados. As diferenças de OK entre técnicas também dependem dos sorteios e dos valores dos bytes; as rodadas não reutilizaram exatamente o mesmo ruído, portanto a diferença bruta de contagens não mede isoladamente uma vantagem do algoritmo.

A cópia posterior torna a auditoria possível, mas integra a instrumentação experimental. O resultado OK da tabela é definido pelo payload; divergências de cabeçalho precisam ser avaliadas nos campos específicos. A observação de zero cabeçalhos aceitos divergentes nesta série não elimina a possibilidade de colisões futuras no cabeçalho.

A ordenação uint16 e a passagem de 65535 para zero foram previstas/testadas em software, mas as rodadas físicas de 0 a 999 não exercitam esse cruzamento. As mensagens de versão foram observadas durante a gravação. Os metadados originais conservam firmware_instalado_verificado_pelo_programa=false e ajuste_isr_us_verificado_pelo_programa=false; não houve leitura de flash nem prova criptográfica do binário instalado. Os hashes publicados atestam integridade dos arquivos, não identidade do firmware em execução.

O registro de especificação prévia se refere somente à revisão CORPO64 e não comprova a ordem de elaboração do projeto original. Houve assistência de IA nos programas, na análise e na documentação. As regras da disciplina sobre autoria e defesa continuam aplicáveis. A demonstração presencial e a predição individual permanecem atividades a realizar, sem serem declaradas concluídas neste relatório.

## 9. Conclusão

A revisão CORPO64 produziu sete séries físicas consistentes e ampliou os alvos de ruído aos oito bytes do corpo. Dois payloads incorretos passaram pelo SUM8 no modo 2 e foram entregues, com evidência byte a byte e resíduo zero. O CRC8 registrou zero ND nas três condições ensaiadas. A verificação custou aproximadamente 1 µs no SUM8 e 3 µs no CRC8, muito abaixo do período de 101 ms adotado na bancada. O controle separado mostrou retomada após uma pausa de recepção, com distinção entre tempo sem UART, tempo após reativação e tempo desde a detecção de perda. Os dados, scripts e hashes permitem reproduzir a consolidação, dentro dos limites instrumentais descritos.

## Apêndice A. Rastreabilidade e reprodução

{SOURCES}

Cada pasta contém seis arquivos originais. PROVENIENCIA_COLETAS.json identifica a cópia e seus hashes. validacao/VALIDACAO_FISICA.json registra a conferência independente; validacao/REVISAO_OFICIAL_COLETAS.json registra a revisão pelo validador do coletor. Os logs de testes sintéticos e o estado prévio de compilação continuam preservados com sua data.

Para recalcular a partir dos dados versionados, instalar Python 3 e reportlab e executar, na raiz do repositório:

```text
python scripts/consolidar.py
python scripts/atualizar_documentacao.py
python scripts/gerar_pdf.py
python scripts/manifestos.py
```

## Fontes do trabalho

1. Implementações TX, RX e RUÍDO da revisão CORPO64: revisoes/corpo64/firmware/. O código é a fonte dos parâmetros do protocolo, sorteio, temporização e critérios de decisão.
2. Especificação registrada antes desta revisão: revisoes/corpo64/ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md. Documento histórico, não atualização retrospectiva da proposta.
3. Sete coletas físicas de 19/10/2026: revisoes/corpo64/dados/, identificadas no Apêndice A.
4. Controles de cálculo e recuperação transcritos do chat: evidencias/corpo64/, com procedência em ORIGEM.md.
5. Documentação e medições anteriores: historico/payload_2026-10-17/, dados/, evidencias/ e pdfs_por_etapa/. Servem à rastreabilidade histórica e não às contagens atuais.
'''
    write('Relatorio_Final_ESP32.md',report)
    write('README.md',f'''# Detecção de erros em telemetria com ESP32

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

{COUNTS}

Taxa ND = NAO_DETECTADO/(CORROMPIDO+NAO_DETECTADO). SUM8/2: 2/621 = **0,3221%**. Os 90 perdidos dessa condição não entram no denominador. Nas sete séries, todos os perdidos foram LEN inválido, sem checagem concluída.

![Contagens por condição](revisoes/corpo64/graficos/contagens.svg)

## O que o sistema faz

TX envia dados sintéticos por UART 115200 8N1. RUÍDO aciona BC547 para forçar bits a zero em LEN, sequência, payload ou CHK. RX calcula SUM8 ou CRC-8/SMBUS e registra a decisão antes de consultar a cópia de depuração. Referência não corrige a decisão ou o enlace. Marcador e cópia ficam preservados. Não há ACK nem retransmissão.

Tempos médios RX: SUM8 **{fmt(D['verificacao_rx_agregada_us']['SUM8']['media'])} µs** em 3.774 checagens; CRC8 **{fmt(D['verificacao_rx_agregada_us']['CRC8']['media'])} µs** em 2.764. Controles TX: 1 µs e 3 µs. Intervalo TX: 101 ms. Recuperação após reativar UART: 38,691 ms, após pausa efetiva de 350,443 ms.

## Versões e histórico

Use os programas de **revisoes/corpo64/firmware/** e o coletor v3 dessa revisão. Os programas em firmware/ na raiz e as coletas em dados/ na raiz pertencem à série anterior, com ruído somente no payload. Não combinar as versões.

O [relatório e os documentos anteriores](historico/payload_2026-10-17/README_HISTORICO.md), os PDFs por etapa e as evidências antigas foram preservados. A especificação prévia CORPO64 e o estado original de testes de software também permanecem como registros datados.

## Reproduzir a análise

Requer Python 3 e reportlab para o PDF. Na raiz: `python scripts/consolidar.py`, `python scripts/atualizar_documentacao.py`, `python scripts/gerar_pdf.py`, `python scripts/manifestos.py`. Os dois primeiros passos usam os CSVs versionados, sem precisar de portas seriais. O coletor no Windows funciona sem Python.

Zero ND em CRC8 nas rodadas não garante detecção universal. Hashes garantem integridade dos arquivos, sem atestar o binário gravado. A defesa e a predição ao vivo ainda precisam ser realizadas. Os programas e documentos tiveram assistência de IA, conforme registrado no relatório.
''')
    write('LEIA_PRIMEIRO.md','''# Começar pela série CORPO64

Os ensaios físicos de 19/10/2026 estão consolidados. Abra README.md para os resultados e Relatorio_Final_ESP32.pdf para o relatório atual.

Para repetir a bancada, use somente firmware e coletor em revisoes/corpo64/. O roteiro é GUIA_DEMONSTRACAO.md. As pastas firmware/, coletor/ e dados/ na raiz são da série anterior.

As pendências humanas de entrega estão em PROXIMOS_PASSOS.md. A apresentação, defesa e predição ao vivo não foram declaradas realizadas.
''')
    write('PROXIMOS_PASSOS.md','''# Estado da conclusão e entrega

Atualização: 19/10/2026, série CORPO64.

## Concluído com registros

- [x] Inicialização do RX CORPO64 v1.0 e RUÍDO CORPO64 v2.0 observada durante a gravação.
- [x] Sete testes curtos de dez quadros e controles complementares sem ruído.
- [x] Sete rodadas de mil quadros e 56.000 amostras do injetor, preservadas com CSVs, logs e metadados.
- [x] Recalcular contagens, resíduos, máscaras e tempos; cruzar decisão anterior à referência e bytes do injetor.
- [x] Identificar dois erros não detectados naturais do SUM8, sequências 415 e 553.
- [x] Registrar cálculo TX, verificação RX, intervalos e controle de recuperação após pausa de recepção.
- [x] Atualizar relatório, gráficos, protocolo, roteiro de apresentação e documentos de entrada do repositório.

Os arquivos de dados e metadados originais não foram corrigidos manualmente. A revisão dos registros não certifica montagem nem calibração elétrica. Os hashes dos fontes não são leitura do firmware instalado nas placas.

## Ainda precisa ser feito pelo integrante

- [ ] Ler o relatório atualizado e conferir a identificação, os requisitos originais do professor e a declaração de assistência de IA.
- [ ] Ensaiar a explicação de TX, RX e RUÍDO, incluindo LEN inválido, sequência e auditoria posterior.
- [ ] Preparar cada integrante para prever os contadores de dez quadros e justificar a previsão antes de executar o cenário escolhido pelo professor.
- [ ] Conferir acesso ao GitHub público e levar uma cópia local do repositório/PDF para a aula.
- [ ] Entregar pelo canal indicado e realizar a demonstração/defesa presencial.

Use APRESENTACAO_20_MIN.md e GUIA_DEMONSTRACAO.md. A presença de dados e documentos não substitui a avaliação presencial nem declara conformidade com regras de autoria que não foram fornecidas integralmente.
''')
    write('historico/payload_2026-10-17/README_HISTORICO.md','''# Histórico da série com ruído no payload

Esta pasta preserva byte a byte as versões dos documentos substituídos na consolidação CORPO64. Inclui também os checklists anteriores à validação física, que retratam o estado do projeto até 19/10/2026 antes das novas coletas.

Os links relativos internos dos documentos arquivados foram mantidos como no original. Para navegar, use os arquivos da raiz atual: os dados antigos estão em dados/, programas em firmware/, coletor antigo em coletor/, PDFs de etapas em pdfs_por_etapa/ e evidências antigas em evidencias/. O relatório atual é Relatorio_Final_ESP32.pdf na raiz e os dados atuais ficam em revisoes/corpo64/dados/.

As referências a repositório privado e validação pendente nestas cópias são históricas. Não foram atualizadas retrospectivamente.
''')
    write('GUIA_DEMONSTRACAO.md','''# Demonstração CORPO64

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
''')
    write('APRESENTACAO_20_MIN.md',f'''# Roteiro de apresentação CORPO64 - 20 minutos

Resultados físicos de 19/10/2026. Use o relatório atual e GUIA_DEMONSTRACAO.md. A defesa e a predição precisam ser realizadas ao vivo.

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
- Média RX: {fmt(D['verificacao_rx_agregada_us']['SUM8']['media'])} µs SUM8 (3.774 checagens) e {fmt(D['verificacao_rx_agregada_us']['CRC8']['media'])} µs CRC8 (2.764).
- Intervalo TX: 101 ms. Overhead: 8 bytes para 4 úteis; quadro de dados de 12 bytes, sem contar cópia.
- Referência não corrige sequência nem payload entregue. Descarte não é contado de novo como perda.
- Zero ND em CRC8 nos ensaios não é garantia universal. Marcador e start/stop não foram alvos.

## Preparação

Levar as placas, montagem, cabos USB e cópias locais do PDF/repositório. Identificar portas e deixar TX parado antes de preparar. Todos devem explicar o sistema inteiro e informar a assistência de IA conforme as regras da disciplina.

## Registro da predição presencial

Técnica e modo escolhidos pelo professor: ___________________

| Integrante | OK previsto | Corrompido previsto | Perdido previsto | ND previsto | Justificativa |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |

Resultado e validade observados: __________________________
''')
    previous=ROOT/'historico/payload_2026-10-17/ESPECIFICACAO_PROTOCOLO.md'
    format_part=previous.read_text(encoding='utf-8-sig').split('# Regras do receptor e timeouts')[0]
    write('ESPECIFICACAO_PROTOCOLO.md',format_part+'''
# Regras atuais do receptor CORPO64

O parser limita o corpo a oito bytes e exige LEN=7. LEN inválido gera PERDIDO por enquadramento, SEM_CHECAGEM. Em corpos válidos, resíduo não zero gera DESCARTADO/CORROMPIDO e não avança a sequência usando cabeçalho não confiável. Resíduo zero gera ACEITO; o enlace aplica ordenação uint16 de meia faixa e distingue essa aceitação da entrega à aplicação.

Descartes e eventos malformados acumulam eventos anônimos. A próxima sequência aceita permite evidenciar a lacuna, descontando os anônimos e as ausências já diagnosticadas. Isso impede dupla contagem de corrompidos como perdidos. Associações incompatíveis são registradas como ambiguidades.

A auditoria associa cada evento à referência somente depois da decisão. seq_rx mantém a sequência recebida; seq_referencia/seq_experimento são identidade de auditoria. A referência nunca reescreve o enlace. Divergências de cabeçalho são registradas à parte; a classe NAO_DETECTADO da tabela usa divergência do payload. ACEITO não implica automaticamente entregue_aplicacao.

Timeouts: 60 ms para bloco/auditoria e 250 ms para ausência de progresso de dados, com passo de 100 ms. Progresso de dados, não a referência, renova o prazo de enlace. Referência órfã, ausente, inválida ou associação ambígua invalida a medição. Perdas sem evento exigem evidência de ausência e conciliação sem ambiguidade.

Ruído CORPO64: modo 1 sorteia um dos 64 bits; modo 2 dois distintos; modo 3 rajada contígua de 3 a 8 bits dentro de um byte. Modo 0 é controle sem pulsos. Modo 4 é controle extra em P0/P1. Marcador e referência permanecem preservados.

Os detalhes de projeto prévios estão em revisoes/corpo64/ESPECIFICACAO_REVISAO_PRE_IMPLEMENTACAO.md, preservada como documento histórico. Implementação atual em revisoes/corpo64/firmware/. A especificação antiga está em historico/payload_2026-10-17/ESPECIFICACAO_PROTOCOLO.md.
''')
    # Atualiza o guia operacional preservando a versão anterior do documento.
    guide=ROOT/'revisoes/corpo64/LEIA_PRIMEIRO.md'
    old=guide.read_text(encoding='utf-8-sig')
    historical=REV_PATH('validacao/LEIA_PRIMEIRO_ANTES_DAS_COLETAS.md')
    if not historical.exists(): historical.write_text(old,encoding='utf-8')
    write('revisoes/corpo64/LEIA_PRIMEIRO.md','''# Revisão CORPO64 - guia atual

Sete rodadas físicas de mil quadros foram coletadas e conferidas em 19/10/2026. Os resultados atuais estão em resultados_consolidados.json nesta pasta e na raiz. Abra ../../Relatorio_Final_ESP32.pdf e ../../GUIA_DEMONSTRACAO.md.

Programas: firmware/tx/tx.ino, firmware/rx/rx.ino com sequence_link.h e firmware/noise/noise.ino com capture_schedule.h e noise_targets.h. Mantenha cada .h junto de seu sketch. O TX anterior é compatível. Use ESP32 Dev Module/WROOM clássico, CPU 240 MHz e Arduino-ESP32 3.3.12 conforme a configuração documentada. A montagem permanece em ../../montagem/GUIA_MONTAGEM_FISICA_ESP32.md.

O coletor atual é coletor/Abrir_Coletor.cmd; extraia o repositório completo antes de executá-lo. Pare TX, feche monitores RX/RUÍDO, configure técnica/modo/quantidade, prepare e espere PRONTO E GRAVANDO. Só então selecione s ou c no TX e transmita com t (dez) ou g (mil).

Dados atuais: dados/. Controles curtos: triagem/. Tentativas iniciais fora da série: triagem/tentativas_iniciais/. Nova execução local fica em coletor/Coletas/ e não substitui os dados publicados automaticamente.

Os estados prévios de compilação/testes e a especificação prévia foram preservados. Para o estado físico atual consulte validacao/VALIDACAO_FISICA.json e validacao/REVISAO_OFICIAL_COLETAS.json. O guia anterior fica em validacao/LEIA_PRIMEIRO_ANTES_DAS_COLETAS.md.

Não combinar com RX, RUÍDO e coletor da raiz, que pertencem à série anterior com ruído apenas no payload. Hash dos fontes não comprova o binário gravado; metadados originais foram mantidos. A defesa e a predição presencial ainda precisam ser realizadas.
''')
    write('revisoes/corpo64/dados/README.md','''# Série física CORPO64 - 19/10/2026

Sete pastas, uma por condição, cada uma com seis arquivos originais do coletor. Não alterar contagens nem logs manualmente. Os nomes conservam o horário local e a configuração da coleta. A série começa na sequência zero e tem mil posições por rodada.

Os CSVs foram cruzados byte a byte com os logs brutos e com o injetor. Os dados antigos de payload ficam em dados/ na raiz. Os controles curtos estão em ../triagem/ e não entram nas contagens destas sete rodadas.

Consulte ../PROVENIENCIA_COLETAS.json, ../validacao/VALIDACAO_FISICA.json e ../../../Relatorio_Final_ESP32.md. Os metadados preservam as portas observadas e os campos de verificação de firmware, sem afirmar leitura de flash.
''')
    write('scripts/requirements.txt','reportlab>=4.0\n')
    # CSV compacto de resultados, sem alterar os CSVs originais de observações.
    with (ROOT/'revisoes/corpo64/resumo_series.csv').open('w',encoding='utf-8',newline='') as f:
        writer=csv.writer(f); writer.writerow(['tecnica','modo','quadros','OK','CORROMPIDO','PERDIDO','NAO_DETECTADO','denominador_ND','taxa_ND','rx_verificacao_n','rx_verificacao_media_us'])
        for s in D['series']: writer.writerow([s['tecnica'],s['modo'],s['quadros'],*[s['contagens'][k] for k in ['OK','CORROMPIDO','PERDIDO','NAO_DETECTADO']],s['denominador_nao_deteccao'],s['taxa_nao_deteccao'] if s['taxa_nao_deteccao'] is not None else 'NA',s['verificacao_rx_us']['n'],s['verificacao_rx_us']['media']])
    print('Relatório, tabelas e guias atuais gerados.')

def REV_PATH(name): return ROOT/'revisoes/corpo64'/name

if __name__=='__main__': main()
