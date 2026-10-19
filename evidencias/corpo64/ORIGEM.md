# Origem dos controles complementares

Os dois logs TX foram fornecidos pelo usuário no chat de acompanhamento em 19/10/2026, desde INICIO até FIM. O controle SUM8 foi seguido do CRC8, ambos de dez quadros. Os arquivos são transcrições do conteúdo enviado, sem captura serial automática pelo agente.

O log de recuperação do RX une os dois trechos enviados pelo usuário. A sobreposição entre os trechos, de RX_REATIVADO até a decisão do evento 3, foi mantida uma única vez. As linhas e os valores fornecidos foram preservados. O início disponível começa em load, sem reconstruir linhas anteriores de boot. A configuração de ruído zero foi instruída durante a execução, mas não há captura do monitor do injetor nesse controle.

As coletas em revisoes/corpo64/dados/ e triagem/ são cópias dos arquivos do coletor local, distintas destas transcrições. Os dados de triagem das 10h21 e 10h23 correspondem aos controles de cálculo. Os corpos TX devem coincidir com as referências desses controles; o script de consolidação verifica essa correspondência.

Não subtrair horários absolutos de placas diferentes. A pausa e a recuperação são calculadas exclusivamente com horários do RX.
