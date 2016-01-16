#
# force building the judge before doing any lab related stuff
#
BUILD_JUDGE:FORCE_MAKE_PROJECT_CODE
	cd .. && make -f Makefile
FORCE_MAKE_PROJECT_CODE:



#test-sandbox-tpl
test_sandbox_tpl:BUILD_JUDGE test_sandbox_tpl.o
	gcc test_sandbox_tpl.o ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o \
	-o test_sandbox_tpl -lcgroup -lpthread
	cd .. && make -f Makefile

test_sandbox_tpl.o: test_sandbox_tpl.c
	gcc -c test_sandbox_tpl.c -o test_sandbox_tpl.o -lpthread

#test-sandbox
test_sandbox: test_sandbox.o  ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o
	gcc test_sandbox.o ../iniparser/iniparser.o ../config.o ../log.o ../sandbox.o ../compare.o \
	-o test_sandbox -lcgroup
	cd .. && make -f Makefile

test_sandbox.o: test_sandbox.c
	gcc -c test_sandbox.c -o test_sandbox.o -ggdb
	
#test-config
test_config: test_config.o
	gcc ../iniparser/iniparser.o  ../log.o ../config.o test_config.o -o test_config

test_config.o:test_config.c
	gcc -c test_config.c -o test_config.o