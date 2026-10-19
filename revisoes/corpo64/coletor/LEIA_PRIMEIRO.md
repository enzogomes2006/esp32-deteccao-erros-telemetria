# Coletor v3 — CORPO64

Abra `Abrir_Coletor.cmd` depois de gravar RX CORPO64 v1.0 e RUÍDO CORPO64 v2.0. O TX anterior é compatível. Consulte o guia principal `../LEIA_PRIMEIRO.md`.

O coletor usa Windows PowerShell 5.1 e a interface do Windows; não exige Python. O arquivo `Revisar.ps1` precisa ficar junto de `Coletor.ps1`. Ele não grava firmware nem inicia o TX. As portas de RX e RUÍDO devem estar livres, com seus Monitores Seriais fechados.

Os dados são salvos em `Coletas/`, em uma pasta nova por execução. A revisão exige confirmação explícita do escopo CORPO64 e compara os oito bytes com RX e referência. Cabeçalho corrompido esperado não é confundido com referência inválida. A linha de decisão tem de anteceder a auditoria; as referências não podem reescrever a decisão.

Os timestamps absolutos de placas diferentes não são comparados. A ordem decisão/referência usa o relógio do próprio RX. A ordem dos bytes do injetor usa seu próprio relógio.

Para revisar outra vez uma pasta salva, execute:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Coletor.ps1 -ReviewFolder 'C:\caminho\da\coleta'
```

O comando termina com código 0 se os registros passarem na revisão e 1 se houver problemas. Isso não certifica a montagem ou a forma de onda elétrica. Logs parciais e diagnósticos devem ser preservados.
