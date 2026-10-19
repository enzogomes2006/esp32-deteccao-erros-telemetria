# Reproduzir a verificação de software

Os testes incluem o próprio `rx.ino`, o seletor de alvos do injetor e seu agendamento por bit. A interface Serial e o relógio são substituídos por implementações locais somente durante os testes. Não há acesso às placas.

Na raiz deste pacote, usando um compilador C++11 e Windows PowerShell:

```powershell
New-Item -ItemType Directory -Force -Path teste_local,teste_local/fixtures | Out-Null
g++ -D__USE_MINGW_ANSI_STDIO=1 -std=c++11 -Wall -Wextra -Itestes testes/test_rx.cpp -o teste_local/test_rx.exe
.\teste_local\test_rx.exe teste_local/fixtures
powershell.exe -NoProfile -ExecutionPolicy Bypass -File testes/test_coletor.ps1 -FixtureRoot teste_local/fixtures
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File coletor/Coletor.ps1 -CheckGui
```

As fixtures geradas contêm **dados sintéticos de teste**, com relógios de origens diferentes para RX e injetor. Nunca use essas fixtures no relatório como medições físicas. O coletor também verifica uma fixture com três eventos ausentes, sustentados por evidência do enlace, e rejeita a versão ambígua.

Para compilar os sketches com Arduino CLI, configure os diretórios locais e instale o core Arduino-ESP32 3.3.12. Cada sketch tem pasta própria e os respectivos headers precisam acompanhá-lo:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/tx
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/rx
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/noise
```

A compilação e os testes de software não demonstram latência de ISR, pulsos elétricos, sincronismo real nem as contagens da futura coleta.
