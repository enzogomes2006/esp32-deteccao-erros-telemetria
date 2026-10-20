# Coletas usadas no relatório

Sete séries CORPO64 de 19/10/2026: SUM8 modos 0 a 3 e CRC8 modos 1 a 3, com mil quadros em cada pasta. Total: 7.000 quadros e 56.000 amostras do injetor. As tabelas estão em [TABELAS.csv](TABELAS.csv) e no relatório PDF.

Cada pasta contém os dois CSVs, os dois logs brutos, os metadados originais e a revisão do coletor. Os caminhos em metadados registram o local da execução original.

Para conferir outra vez uma pasta, use coletor/Coletor.ps1 com -ReviewFolder e o caminho completo da coleta. Os controles de cálculo do TX e de recuperação do RX estão em evidencias/ e são analisados separadamente das sete séries de mil quadros.
