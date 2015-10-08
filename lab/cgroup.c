
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libcgroup.h>

 #include <sys/utsname.h>

 #include <errno.h>
 #include <memory.h>


//#define CGROUPS

//1- get the state of mounted hierarchies and the subsystems.
//2- mount the cgroup pseudo filesystem if there is not.
//3- create sandbox cgroup in the appropriate controllers (subsystems).
//4- Actually you may use a pool of ready-to-use cgroups

//now we're using a ready vm-like directory to chroot the submissin and run it inside.
//to limit the resources, you may use setrlimit() alongside with already implemented cgroups.

/* 	cgroup_modify_cgroup() is a tricky buisness, 
	user problems occure (setuid may be the solution)
*/
int  create_sandbox(struct cgroup* sandbox);


int main(int argc,char*argv[]){

	int ret=0;
	if(argc<2){
		puts("not enough arguments");
		exit(0);
	}

#ifdef CGROUPS
	ret=cgroup_init();
	if(ret){
		printf("error init : (%s)\n",cgroup_strerror(ret));
		exit(0);
	}

	
	//create 'sandbox' cgroup in memory related hierarchies
	struct cgroup* sandbox=cgroup_new_cgroup("sandbox");
	if(sandbox==NULL){
		puts("new cgroup error");
		exit(0);
	}

	//check if already exists
	ret=cgroup_get_cgroup(sandbox);
	int sd;
	//cgroup exist
	if(strcmp(argv[1],"-d")==0 ){
		if(!ret){
			cgroup_delete_cgroup(sandbox,1);
				printf("cgroup sandbox deleted\n");
				exit(55);
		}
		printf("cannot delete an inexistant sandbox cgroup\n");
		exit(0);
	}
	//always delete
	//cgroup_delete_cgroup(sandbox,1);
	
	if(ret && (sd=create_sandbox(sandbox)) ){
			printf("cannot create sandbox cgroup:%d\n",sd);
			exit(0);
		}
#endif

	//fork into the examined process
	pid_t chld_id=fork();
	if(chld_id==-1){puts("cannot fork");exit(0);}
	if(chld_id==0){
		//this is child area
			//puts("I'm the child");
			sleep(0.3);
			//FILES: unshare file descriptors
			unshare(CLONE_FS|CLONE_NEWUTS|CLONE_NEWNET);
			sethostname("freeSandbox",40);
			setdomainname("freeDomain",15);
#ifdef CGROUPS
			ret=cgroup_attach_task(sandbox);
			if(ret){
				puts(cgroup_strerror(ret));
				exit(4);
			}
#endif	
			char wd[100];
			ret=chdir("/home/odev/chrootdeb");
			if(ret){
				puts("error changir working directory");
			}
			ret=chroot(".");
			if( ret){
				printf("error chrooting : %s\n",strerror(ret));
			}
			//user == odev
			ret=seteuid((uid_t)1001);
			if(ret){
				printf("error setuid\n");
			}


			execvp(argv[1],argv+1);
	}else{
			//the parent
			//puts("i'm the father");
			int status;
			wait(&status);
			printf("the child is done with code : %d\n",status);
			exit(77);
		}

	
	

	return 0;
}


int  create_sandbox(struct cgroup* sandbox){
	int suc;
	//config it
	//get user & group ids
	pid_t uid=getuid();
	gid_t gid=getgid();
	cgroup_set_uid_gid(sandbox,uid,gid,(uid_t)uid,(gid_t)gid);
	struct cgroup_controller* sandbox_memory=cgroup_add_controller(sandbox,"memory");
	struct cgroup_controller* sandbox_devices=cgroup_add_controller(sandbox,"devices");
	if(sandbox_memory==NULL || sandbox_devices==NULL){
		puts("adding controller error");
		return 9;
	}
	//limit memory in bytes
	cgroup_set_value_int64(sandbox_memory,"memory.limit_in_bytes",25000000);
	//deny all devices but (/dev/random, /dev/urandom, /dev/null)
	suc=cgroup_add_value_string(sandbox_devices,"devices.deny","a");
	if(suc){
		printf("error denying device : (%s)\n",cgroup_strerror(suc));
		return 5;
	}

	suc=cgroup_add_value_string(sandbox_devices,"devices.allow","c 1:8 rwm");
	if(suc){
		printf("error allowing /dev/random : (%s)\n",cgroup_strerror(suc));
		return 6;
	}

	//create group
	suc=cgroup_create_cgroup(sandbox,0);
	if(suc){
		puts(cgroup_strerror(suc));
		return 10;
	}


	return 0;
}