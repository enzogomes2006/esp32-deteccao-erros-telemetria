# Coletas usadas no relatório

Sete séries CORPO64 de 19/10/2026: SUM8 modos 0 a 3 e CRC8 modos 1 a 3, com mil quadros em cada pasta. Total: 7.000 quadros e 56.000 amostras do injetor. As tabelas estão em [TABELAS.csv](TABELAS.csv) e no relatório PDF.

Cada pasta conserva os dois CSVs, os dois logs brutos, os metadados originais e a revisão do coletor. Os conteúdos foram preservados byte a byte durante a reorganização. Os caminhos originais que aparecem em metadados são registros da execução; não foram corrigidos manualmente.

Para conferir outra vez uma pasta, use coletor/Coletor.ps1 com -ReviewFolder e o caminho completo da coleta. Os ensaios antigos e os controles preparatórios permanecem no histórico Git e no backup local; não entram nestas tabelas.
