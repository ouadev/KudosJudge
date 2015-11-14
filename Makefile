# Global Makefile to compile (gcc -c) the libraries used in the project, and its components
SUBDIRS=iniparser

#compile root directory
all: subdirs log.o config.o ramfs.o sandbox.o compare.o

log.o: log.c
	gcc -c log.c -o log.o

config.o:config.c
	gcc -c config.c -o config.o

ramfs.o:ramfs.c
	gcc -c ramfs.c -o ramfs.o

sandbox.o:sandbox.c
	gcc -c sandbox.c -o sandbox.o 
	
compare.o:compare.c
	gcc -c compare.c -o compare.o
	
#compile subdirs
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


#clean
clean:
	rm log.o config.o ramfs.o iniparser/iniparser.o sandbox.o compare.o


