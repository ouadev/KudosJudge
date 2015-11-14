

#test-sandbox
test_sandbox: test_sandbox.o  ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o
	gcc test_sandbox.o ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o \
	-o test_sandbox -lcgroup
	cd .. && make -f Makefile

test_sandbox.o: test_sandbox.c
	gcc -c test_sandbox.c -o test_sandbox.o 
	
#test-config
test_config: test_config.o
	gcc ../iniparser/iniparser.o  ../log.o ../config.o test_config.o -o test_config

test_config.o:test_config.c
	gcc -c test_config.c -o test_config.o