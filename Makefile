# Global Makefile to compile (gcc -c) the libraries used in the project, and its components
SUBDIRS=iniparser proc_maps_parser buffer

#buidl everything :  kudosd (daemon) and the experimental client
everything: kudosd client

#build the kudos deamon
kudosd: all
	gcc log.o kudosd.o iniparser/iniparser.o buffer/buffer.o config.o  sandbox.o runner.o \
	 compare.o queue.o interface.o lang.o feed.o ramfs.o \
	-o bin/kudosd  -lpthread

#build the experimental client
client: client.o 
	gcc client.o  -o bin/client

client.o: client.c
	gcc -c client.c -o client.o
#compile root directory
all: subdirs log.o config.o ramfs.o runner.o sandbox.o compare.o queue.o interface.o  kudosd.o  lang.o  feed.o
	
kudosd.o: kudosd.c
	gcc -c kudosd.c -o kudosd.o

log.o: log.c
	gcc -c log.c -o log.o

config.o:config.c
	gcc -c config.c -o config.o

ramfs.o:ramfs.c
	gcc -c ramfs.c -o ramfs.o

sandbox.o:sandbox.c
	gcc -c sandbox.c -o sandbox.o 

runner.o: runner.c
	gcc -c runner.c -o runner.o 

compare.o:compare.c
	gcc -c compare.c -o compare.o
	
queue.o: queue.c
	gcc -c queue.c -o queue.o
	
interface.o:interface.c
	gcc -c interface.c -o interface.o
	
lang.o:lang.c
	gcc -c lang.c -o lang.o

feed.o:feed.c
	gcc -c feed.c -o feed.o
#compile subdirs
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	make -C $@


#clean
clean:
	rm  -f *.o iniparser/*.o proc_maps_master/*.o  buffer/*.o

#install
#i install the binaries at /opt/kudosJudge, 
# and create the needed symlinks.
install:
	./install.sh

#uninstall: reverse the install process
uninstall:
	rm -Rf /opt/kudosJudge;
	rm /usr/local/bin/kudos*;
	
	
#pack : create a package containing the source code, 
#		useful for testing in other environments.
pack:clean
	tar -zcvf bin/kudosJudge.tar.gz ./