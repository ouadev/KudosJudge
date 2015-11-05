
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libcgroup.h>
#include <pwd.h>
#include <grp.h>

#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <signal.h>
#include <time.h>

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
//global variables
pid_t innerwatcher_pid;
char* jamal="hello casablanca";

int  create_sandbox(struct cgroup* sandbox);

//what the sandboxed child is going to execute.

static int child(void*arg);


//timout_handler
void timeout_handler(int sig){
	printf("Outer watcher: timeout received\n");
	printf("killing the Inner Watcher ... \n");
	//kill(0,9);
}

/*
main
*/



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


/* clone */
		char* stack;
		char *stacktop;
		int STACK_SIZE=1024*1024;
		stack=(char*)malloc(STACK_SIZE);
		if(!stack){
			printf("cannot malloc() stack\n");
			return -10;
		}
		stacktop=stack;
		stacktop+=STACK_SIZE;
		pid_t pid=clone(child,stacktop,CLONE_NEWUTS|CLONE_NEWNET|CLONE_NEWPID|SIGCHLD,argv[1]);
		if(pid==-1){
			printf("clone() failed \n");
			return -5;
		}

		innerwatcher_pid=pid;


//the parent
//puts("i'm the father");
		//setting the timer, to kill the 
		signal(SIGALRM,timeout_handler);
		// timer_t timerid;
		// struct sigevent event;
		// event.sigev_notify=SIGEV_SIGNAL;
		// event.sigev_signo=SIGUSR1;
		// ret=timer_create(CLOCK_REALTIME,&event,&timerid);
		// struct timespec timeval={5,0};
		// struct timespec interval={0,0};
		// struct itimerspec timerspec={interval,timeval};
		// timer_settime(timerid,0,&timerspec,NULL);
		// if(ret){
		// 	printf("Inner watcher: cannot create timer\n");
		// 	exit(88);
		// }
		alarm(5);

		int status;
		//wait(&status);
		waitpid(pid,&status,0);
		// sleep(2);
		// kill(pid,9);
		printf("the InWatcher is done with code : %d\n",status);
		exit(77);

		return 0;

	}


/*

create_sandbox

*/
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



/*

child 

*/

static int child(void*arg){
	int ret;
	printf("%s\n",jamal);
	//
	printf("ppid: %d\n",getppid());
	//this is child area
	//puts("I'm the child");
	// sleep(0.3);
	sethostname("freeJugSandbox",40);
	setdomainname("freeDomain",15);
#ifdef CGROUPS
	ret=cgroup_attach_task(sandbox);
	if(ret){
		puts(cgroup_strerror(ret));
		exit(4);
	}
#endif	


	//chrooting
	char wd[100];
	ret=chdir("/home/odev/chrootdeb");
	if(ret){
		puts("error changing working directory");
	}
	//TODO: use mount namespaces instead
	ret=chroot(".");
	if( ret){
		printf("error chrooting : %s\n",strerror(ret));
	}

	//remount the proc so it prints the new stuff (CLONE_NEWPID to work)
	umount("/proc");
	mount("none","/proc","proc",MS_RDONLY,NULL);

	//TEST FORKING
	// pid_t ns_pid=fork();
	// if(ns_pid>0){
	// 	//i'm the direct father of the submission
		
	// 	int exitstatus;
	// 	wait(&exitstatus);
	// 	printf("BinChild exited : %d\n",exitstatus);
	// 	exit(56);
	// 	//parent
	// }else if(ns_pid>0){
	// 	printf("cannot fork inside container, exit\n");
	// 	exit(4);
	// }

	//child of child code
	//set rlimits
	struct rlimit cpu_rlimit;
	getrlimit(RLIMIT_CPU,&cpu_rlimit);
	//printf("current cpu rlimit : %d,%d\n",cpu_rlimit.rlim_cur,cpu_rlimit.rlim_max);
	cpu_rlimit.rlim_cur=1;
	cpu_rlimit.rlim_max=3;
	ret=setrlimit(RLIMIT_CPU,&cpu_rlimit);
	if(ret){
		printf("cannot set cpu rlimit");
		exit(5);
	}

	//edit signals mask
	sigset_t init_sigs;
	sigemptyset(&init_sigs);
	sigaddset(&init_sigs,SIGXCPU);
	sigaddset(&init_sigs,SIGKILL);
	sigprocmask(SIG_UNBLOCK,&init_sigs,NULL);


	//user == odev
	//drop all the priveleges
	struct passwd* pwd_nobody=getpwnam("nobody");
	uid_t nobody_id=pwd_nobody->pw_uid;
	ret=setgid(pwd_nobody->pw_gid);
	if(ret){
		printf("error setgid(), exiting ...\n");
		exit(0);
	}

	ret=setgroups(0,NULL);
	if(ret<0){
		printf("cannot remove supplementary groups, exiting ...");
		exit(0);
	}

	ret=setuid(pwd_nobody->pw_uid);
	if(ret){
		printf("error setuid, exiting ...\n");
		exit(0);
	}
	if(!setuid(0)){
		printf("still root, exiting ...\n");
		exit(0);
	}else{
		printf("root properly droped :) \n");
	}
	printf("return of seteuid(): %d\n",ret);
	printf("EUID:%d, RUID: %d\n",geteuid(),getuid());

	gid_t listg[20];
	ret=getgroups(15,listg);
	int i=0;
	for(i=0;i<ret;i++)
		printf("group %d : %d\n",i,listg[i]);


	execvp(arg,NULL);

}