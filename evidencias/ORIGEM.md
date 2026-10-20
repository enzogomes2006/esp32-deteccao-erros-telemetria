# Origem dos controles complementares

Os dois logs TX foram fornecidos pelo usuário no chat de acompanhamento em 19/10/2026, desde INICIO até FIM. O controle SUM8 foi seguido do CRC8, ambos de dez quadros. Os arquivos são transcrições do conteúdo enviado, sem captura serial automática pelo agente.

O log de recuperação do RX une os dois trechos enviados pelo usuário. A sobreposição entre os trechos, de RX_REATIVADO até a decisão do evento 3, foi mantida uma única vez. As linhas e os valores fornecidos foram preservados. O início disponível começa em load, sem reconstruir linhas anteriores de boot. A configuração de ruído zero foi instruída durante a execução, mas não há captura do monitor do injetor nesse controle.

As sete coletas em dados/ são cópias dos arquivos do coletor local, distintas destas transcrições. Os controles curtos usados para cruzar os corpos TX com as referências permanecem no [registro anterior](https://github.com/enzogomes2006/esp32-deteccao-erros-telemetria/tree/2649ac9/revisoes/corpo64/triagem). Essa correspondência foi conferida na consolidação da série.

Não subtrair horários absolutos de placas diferentes. A pausa e a recuperação são calculadas exclusivamente com horários do RX.
