ppi: ppi.c
	gcc -Wall -ggdb ppi.c -o ppi

clean: 
	rm -fv ppi
