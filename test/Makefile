t:t_ramfs.o ramfs.o log.o
	gcc log.o ramfs.o t_ramfs.o   -o t

t_ramfs.o: t_ramfs.c
	gcc -c t_ramfs.c -o t_ramfs.o

ramfs.o:ramfs.c ramfs.h
	gcc -c ramfs.c -o ramfs.o

log.o:log.c
	gcc -c log.c -o log.o