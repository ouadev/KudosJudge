#malware
malware: malware.o ../log.o
	gcc malware.o ../log.o -o malware
	cp malware /home/odev/chrootdeb/opt/malware

malware.o:malware.c
	gcc -c malware.c -o malware.o 
	
	
	
#signal
signal: signal.c
	gcc signal.c -o signal
	
	
#flush

flush: flush.o ../log.o
	gcc flush.o ../log.o -o flush
	
flush.o:flush.c
	gcc -c flush.c -o flush.o