# Origem dos controles

Os logs TX são transcrições enviadas no chat de acompanhamento em 19/10/2026, desde INICIO até FIM: primeiro dez quadros SUM8 e depois dez CRC8. Não são arquivos capturados automaticamente pelo coletor.

O log de recuperação do RX reúne dois trechos enviados no mesmo acompanhamento. As linhas sobrepostas entre RX_REATIVADO e a decisão do evento 3 aparecem uma única vez. O início disponível começa em load; nenhuma linha anterior de boot foi reconstruída. A configuração de ruído zero foi instruída durante o ensaio, mas não há captura do monitor do injetor para esse controle.

Os corpos dos controles TX foram comparados com as referências das coletas curtas, disponíveis no [registro anterior](https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria/tree/2649ac9/revisoes/corpo64/triagem). As sete coletas de mil quadros ficam em dados/.

Os relógios das placas têm origens diferentes. Calcule pausa e recuperação com os horários do próprio RX, sem subtrair seus timestamps dos horários do injetor.
