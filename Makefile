all: fdt2.c
	gcc -o fdt2 fdt2.c -lfdt  -lssl -lcrypto
