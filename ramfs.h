/***
* ramfs: definition of functions that deal with :
*	+ create and mount a ramfs filesystem.
*	+ put files in the ramfs and watch for memory usage
*
*/

#ifndef H_RAMFS
#define H_RAMFS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "log.h"




/**
* structe ramfs_info : stores all the information about jugramfs;
*/
typedef struct ramfs_info{
	//path to root directory, just add /jug to have the directory where ramfs will reside
	char* root_dir;
	/** ramfs dir */
	char* directory;
	//size to allocate
	int size_alloc;
	//memory length used
	int used;
	//max memory usage allowed
	int mem_max;

}ramfs_info;




/**
* init_ramfs
*/
ramfs_info* init_ramfs(char* rootfs,int size_alloc,int max_size);

/**
* create_ramfs()
*/
int create_ramfs( ramfs_info*info);

/**
* int remove_ramfs()
*/
int rm_ramfs_dir(ramfs_info*info);
/**
* ramfs_ismounted(ramfs_info*info)
*/
int ramfs_ismounted(ramfs_info*info);
/**
* unmount
*/
int umount_ramfs(ramfs_info*info);

/**
* get_ramfs_dir
*/
char* get_ramfs_dir(ramfs_info*info);

/**
* ramfs_cp
*/
int ramfs_cp(ramfs_info*info,char* src,char* target);




//print information about ramfs_info
void print_ramfs_info(ramfs_info*info);

#endif