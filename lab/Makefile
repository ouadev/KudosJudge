#
# force building the judge before doing any lab related stuff
#
BUILD_JUDGE:FORCE_MAKE_PROJECT_CODE
	cd .. && make -f Makefile
FORCE_MAKE_PROJECT_CODE:

#malware
malware: BUILD_JUDGE  malware.o ../log.o
	gcc malware.o ../log.o -o malware
	cp malware /home/odev/chrootdeb/opt/malware

malware.o:malware.c
	gcc -c malware.c -o malware.o 
	
#runner_php
runner_php: BUILD_JUDGE runner_php.o 
	gcc runner_php.o ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o ../ramfs.o \
	-o runner_php -lcgroup
	
	
runner_php.o:runner_php.c
	gcc -c runner_php.c -o runner_php.o -ggdb


#launcher
launcher: BUILD_JUDGE launcher.o 
	gcc launcher.o ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o ../ramfs.o \
	-o launcher -lcgroup
	
launcher.o: launcher.c
	gcc -c launcher.c -o launcher.o 

	
#signal
signal: BUILD_JUDGE signal.c
	gcc signal.c -o signal
	
	
#flush

flush: BUILD_JUDGE flush.o ../log.o
	gcc flush.o ../log.o -o flush
	
flush.o:flush.c
	gcc -c flush.c -o flush.o
	
#stopper
stopper: stopper.c
	gcc stopper.c -o stopper
	
	
