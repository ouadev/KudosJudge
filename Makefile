# Global Makefile to compile (gcc -c) the libraries used in the project, and its components
SUBDIRS=iniparser proc_maps_parser

#build the kudos deamon
kudosd: all
	gcc kudosd.o iniparser/iniparser.o config.o log.o sandbox.o compare.o \
	-o bin/kudosd -lcgroup -lpthread

#build the experimental client
client: client.o
	gcc client.o -o bin/client

client.o: client.c
	gcc -c client.c -o client.o
#compile root directory
all: subdirs log.o config.o ramfs.o sandbox.o compare.o kudosd.o
	
kudosd.o: kudosd.c
	gcc -c kudosd.c -o kudosd.o

log.o: log.c
	gcc -c log.c -o log.o

config.o:config.c
	gcc -c config.c -o config.o

ramfs.o:ramfs.c
	gcc -c ramfs.c -o ramfs.o

sandbox.o:sandbox.c
	gcc -c sandbox.c -o sandbox.o  -ggdb
	
compare.o:compare.c
	gcc -c compare.c -o compare.o
	
#compile subdirs
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


#clean
clean:
	rm *.o iniparser/*.o proc_maps_master/*.o 


