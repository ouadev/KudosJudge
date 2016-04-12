# Global Makefile to compile (gcc -c) the libraries used in the project, and its components
SUBDIRS=iniparser proc_maps_parser

#build the kudos deamon
kudosd: all
	gcc kudosd.o iniparser/iniparser.o config.o log.o sandbox.o \
	compare.o queue.o interface.o protocol.o lang.o \
	-o bin/kudosd  -lpthread

#build the experimental client
client: client.o kudosd
	gcc client.o -o bin/client

client.o: client.c
	gcc -c client.c -o client.o
#compile root directory
all: subdirs log.o config.o ramfs.o sandbox.o compare.o queue.o interface.o protocol.o kudosd.o  lang.o
	
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
	
queue.o: queue.c
	gcc -c queue.c -o queue.o
	
interface.o:interface.c
	gcc -c interface.c -o interface.o

protocol.o:protocol.c
	gcc -c protocol.c -o protocol.o
	
lang.o:lang.c
	gcc -c lang.c -o lang.o
#compile subdirs
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


#clean
clean:
	rm *.o iniparser/*.o proc_maps_master/*.o 


