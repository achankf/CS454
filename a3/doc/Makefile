TEX = $(shell find -name "*.tex")

all: document.pdf

document.pdf: $(TEX)
	pdflatex document.tex

.phony: clean

clean:
	rm -f *.log *.aux *.pdf
