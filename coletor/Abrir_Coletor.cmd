@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Coletor.ps1"
if errorlevel 1 (
  echo.
  echo O coletor encontrou um erro. Copie a mensagem acima para analise.
  pause
)
endlocal
