# Entrega organizada: telemetria ESP32

Este pacote reúne a série final de 17/10/2026 e os três programas escolhidos para a demonstração. Não é um instalador e não precisa ser executado para consultar o trabalho.

- README.md: apresentação do repositório e links dos arquivos.
- APRESENTACAO_20_MIN.md: roteiro da exposição e ficha de predição ao vivo.

## O que abrir

- Relatorio_Final_ESP32.pdf: relatório acadêmico diagramado em LaTeX, com 14 páginas, identificado com Enzo Aurélio; IFMT - Octayde; TCD - Transmissão e Comunicação de Dados; professor Ruy Oliveira.
- latex/main.tex: fonte LaTeX editável; este é o documento usado para gerar o PDF.
- Relatorio_Final_ESP32.md: conteúdo editável em texto do relatório acadêmico.
- ESPECIFICACAO_PROTOCOLO.md: regras atuais do protocolo e receptor.
- GUIA_DEMONSTRACAO.md: quais códigos usar, comandos separados e roteiro dos testes ao vivo.
- montagem/: guia físico em PDF e Markdown, com tabela dos nove fios, dois resistores, BC547 e mapas vetoriais da P2. Descreve a fiação já indicada; não exige remontagem.
- firmware/: somente TX atual, RX completo v0.2 e RUÍDO captura v1.3.1, sem alterações nos arquivos de origem.
- dados/: sete coletas completas, seis arquivos originais por pasta, preservados byte a byte.
- resultados_consolidados.json: contagens, taxas, tempos e quatro erros não detectados da série.
- coletor/: programa opcional de captura CSV v2. Extrair o ZIP antes de abrir Abrir_Coletor.cmd.
- evidencias/: dois PDFs históricos com capturas que sustentam os tempos TX e a recuperação do RX completo. As instruções antigas nesses anexos não substituem o guia atual.
- pdfs_por_etapa/: os sete PDFs originais das partes 1 a 7. Alguns documentam etapas anteriores; o relatório final e o guia atual consolidam a versão de entrega.
- PROVENIENCIA.json e MANIFESTO_SHA256.json: origem e hashes das cópias.

Os sketches foram copiados para nomes de entrega tx/tx.ino, rx/rx.ino e noise/noise.ino. O conteúdo de rx.ino corresponde ao antigo arquivo rx_completo/rx_completo.ino, não ao RX antigo; noise.ino corresponde a noise_captura_v131/noise_captura_v131.ino. Mantenha capture_schedule.h junto de noise.ino. Os nomes de versão nas mensagens de boot foram preservados.

## Situação

As sete coletas obrigatórias estão concluídas e conferidas. A identificação informada foi incluída no relatório. Repositório privado do trabalho: https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria. O endereço privado exige acesso concedido pelo proprietário para ser consultado por outras contas. A demonstração/predição individual de dez quadros é atividade ao vivo, não algo concluído pela criação deste ZIP.

Os sete PDFs das etapas anteriores foram preservados para consulta na pasta pdfs_por_etapa. Os logs de tentativas inválidas permanecem fora das sete coletas finais selecionadas. As tabelas atuais não misturam esses testes. A referência experimental só é consultada após a decisão do enlace. O modelo de erro é restrito ao payload; não é um protocolo genérico para qualquer cabeçalho corrompido.

Configuração: ESP32 Dev Module / WROOM clássico, CPU 240 MHz, Arduino-ESP32 3.3.12; USB e UART2 115200 baud. Portas observadas: TX COM5, RX COM6, RUÍDO COM3. Confirme as portas se necessário. Não precisa regravar placas que continuam com os programas das últimas coletas.
