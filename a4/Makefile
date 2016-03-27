TEX = $(shell find -name "*.tex")

all: a4response.pdf

a4response.pdf: $(TEX) thm.sty
	pdflatex a4response.tex

.phony: clean

clean:
	rm -f a4response.pdf *.aux *.log
