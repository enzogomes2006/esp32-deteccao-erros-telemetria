# Fonte do relatório

O relatório é de Enzo Aurélio, Luís Fernando e Vinicius Campos. O arquivo `main.tex` e a pasta `figuras/` geram o PDF de entrega da raiz. Não há uma segunda versão do relatório para escolher.

Com pdfLaTeX, execute duas vezes nesta pasta para atualizar sumário e referências:

```text
pdflatex main.tex
pdflatex main.tex
```

Também é possível usar `tectonic main.tex`, que repete as passagens necessárias. Depois da compilação, copie `main.pdf` para `Relatorio_Final_ESP32.pdf` na raiz. As observações originais ficam em `dados/` e não devem ser alteradas para ajustar as tabelas.
