CC = gcc
CFLAGS = -g
default: lab3a

lab3a: lab3a.c
	$(CC) $(CFLAGS) lab3a.c -o $@

clean: default
	rm lab3a
	rm *.csv

dist:  lab3a-304441183.tar.gz
lab_files = lab3a.c README Makefile 

lab3a-304441183.tar.gz: $(lab_files)
	tar -czf $@ $(lab_files)
