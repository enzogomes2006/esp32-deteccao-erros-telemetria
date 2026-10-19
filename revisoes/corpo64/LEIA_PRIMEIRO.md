# Revisão CORPO64 - guia atual

Sete rodadas físicas de mil quadros foram coletadas e conferidas em 19/10/2026. Os resultados atuais estão em resultados_consolidados.json nesta pasta e na raiz. Abra ../../Relatorio_Final_ESP32.pdf e ../../GUIA_DEMONSTRACAO.md.

Programas: firmware/tx/tx.ino, firmware/rx/rx.ino com sequence_link.h e firmware/noise/noise.ino com capture_schedule.h e noise_targets.h. Mantenha cada .h junto de seu sketch. O TX anterior é compatível. Use ESP32 Dev Module/WROOM clássico, CPU 240 MHz e Arduino-ESP32 3.3.12 conforme a configuração documentada. A montagem permanece em ../../montagem/GUIA_MONTAGEM_FISICA_ESP32.md.

O coletor atual é coletor/Abrir_Coletor.cmd; extraia o repositório completo antes de executá-lo. Pare TX, feche monitores RX/RUÍDO, configure técnica/modo/quantidade, prepare e espere PRONTO E GRAVANDO. Só então selecione s ou c no TX e transmita com t (dez) ou g (mil).

Dados atuais: dados/. Controles curtos: triagem/. Tentativas iniciais fora da série: triagem/tentativas_iniciais/. Nova execução local fica em coletor/Coletas/ e não substitui os dados publicados automaticamente.

Os estados prévios de compilação/testes e a especificação prévia foram preservados. Para o estado físico atual consulte validacao/VALIDACAO_FISICA.json e validacao/REVISAO_OFICIAL_COLETAS.json. O guia anterior fica em validacao/LEIA_PRIMEIRO_ANTES_DAS_COLETAS.md.

Não combinar com RX, RUÍDO e coletor da raiz, que pertencem à série anterior com ruído apenas no payload. Hash dos fontes não comprova o binário gravado; metadados originais foram mantidos. A defesa e a predição presencial ainda precisam ser realizadas.
