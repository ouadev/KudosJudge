#include "sandbox.h"

/*
//TODO: think about using mount namespaces instead of chroot.
//TODO: remember to check the health of the rootfs before begining.
//TODO: receive cgroup limit expiration event.
//TODO:	manage /tmp file creation
//TODO:	manage forking/setrlimit relationship
//TODO:	runtime checking of outputs
//TODO:	Interpreters limits (java, python,...)
//TODO: Compiling layer
//TODO:	prepare test infrastructure with convenient logging.
//TODO:	make the whole stuff thread-safe.


 */


//test function to generate load on cpu
int fact(int u){
	if(u==1) return 1;
	return fact(u-1);
}


int jug_sandbox_init(struct sandbox* sandbox_struct){

	//fill the sandbox structure

	int error;
	struct sandbox* sb=sandbox_struct;
	char* value;
	//add stack_size
	if( (value=jug_get_config("Executer","mem_limit_mb") )==NULL ){
		debugt("sandbox","cannot read mem_limit_mb config");
		return -1;
	} 

	sb->mem_limit_mb_default=atoi(value);

	if( ( value=jug_get_config("Executer","walltime_limit_ms") )==NULL ){
		debugt("sandbox","cannot read walltime_limit_ms config");
		return -1;
	}

	sb->walltime_limit_ms_default=atoi(value);

	if( ( value=jug_get_config("Executer","time_limit_ms") )==NULL ){
		debugt("sandbox","cannot read time_limit_ms config");
		return -1;
	}
	sb->time_limit_ms_default=atoi(value);

	if( ( value=jug_get_config("Executer","stack_size_mb") )==NULL ){
		debugt("sandbox","cannot read time_limit_ms config");
		return -1;
	}
	sb->stack_size_mb_default=atoi(value);

	if( (value=jug_get_config("Executer","use_cgroups") )==NULL ){
		debugt("sandbox","cannot read use_cgroups config");
		return -1;
	}
	sb->use_cgroups=atoi(value);
	if( (value=jug_get_config("Executer","use_setrlimit") )==NULL ){
		debugt("sandbox","cannot read use_setrlimit config");
		return -1;
	}
	sb->use_setrlimit= atoi(value);

	if( (value=jug_get_config("Executer","chroot_dir_path"))==NULL){
		debugt("sandbox_init","cannot read chroot_dir_path");
		return -1;
	}
	sb->chroot_dir=(char*)malloc( sizeof(char)*(strlen(value)+5) );
	strcpy(sb->chroot_dir,value);

	//fill sandbox's state
	sb->nbr_instances=0;

	//check if cgroup filesystem is properly mounted
	if(sb->use_cgroups){
		error=cgroup_init();
		if(error){
			debugt("sandbox_init",cgroup_strerror(error));
			return -4;
		}
		//check for memory controller is mounted
		char* mount_mem;char* mountp;
		cgroup_get_subsys_mount_point("memory",&mountp  );
		if(!mountp){
			debugt("sandbox_init","memory controller not mounted");
			return -3;
		}

		struct cgroup* sandbox_cgroup=cgroup_new_cgroup("jug_sandbox");
		if(!sandbox_cgroup){
			debugt("sandbox_init","error cgroup_new_cgroup()");
			return -5;
		}
		error=cgroup_get_cgroup(sandbox_cgroup);
		if(error){
			//cgroup unfound
			error=jug_sandbox_create_cgroup(sandbox_cgroup,sb);
			if(error){
				debugt("sandbox_init","cannot create the sandbox cgroup");
				return -6;
			}
		}
		//cgroup created
		sb->sandbox_cgroup=sandbox_cgroup;
	}
	//check the sandbox root env.
	struct stat st={0};
	error=stat("/tmp",&st);
	if(error){
		debugt("sandbox_init","chroot's /tmp unfound, jug_rootfs is not ready");
		return -10;
	}


	return 0;
}


//jug_sandbox_create_cgroup
//private

int jug_sandbox_create_cgroup(struct cgroup* sandbox,struct sandbox* sandbox_struct){
	int suc;
	//config it
	//the user creating the cgroup is the one who can play with it
	pid_t uid=getuid();
	gid_t gid=getgid();
	cgroup_set_uid_gid(sandbox,uid,gid,(uid_t)uid,(gid_t)gid);
	struct cgroup_controller* sandbox_memory=cgroup_add_controller(sandbox,"memory");
	if(sandbox_memory==NULL){
		debugt("sandbox_create_cgroup","cannot add memory controller");
		return -1;
	}
	//limit memory in bytes
	cgroup_set_value_int64(sandbox_memory,"memory.limit_in_bytes",sandbox_struct->mem_limit_mb_default*1000000);


	//create group
	suc=cgroup_create_cgroup(sandbox,0);
	if(suc){
		debugt("sandbox_create_cgroup",cgroup_strerror(suc));
		return -10;
	}


	return 0;

}

/**
 * medium structure
 */
struct clone_child_params{
	struct run_params* run_params_struct;
	struct sandbox* sandbox_struct;
	char* binary_path;
	char**argv;
};


/**
 * jug_sandbox_run()
 * + the sandbox is supposedly inited
 * + if cgroups are not used, use setrlimit to limit memory usage
 *
 */
//TODO: change memory cgroup limit for each run
int jug_sandbox_run(struct run_params* run_params_struct,
		struct sandbox* sandbox_struct,
		char* binary_path,
		char*argv[]){
	//some checking
	//check fd_in a fd_out
	int fail;
	int fd_datasource,fd_datasource_dir,fd_datasource_flags,fd_datasource_status;
	fd_datasource=run_params_struct->fd_datasource;
	fd_datasource_dir=run_params_struct->fd_datasource_dir;

	if( (fd_datasource_flags=fcntl(fd_datasource,F_GETFD) )<0 ){
		debugt("jug_sandbox_run","fd_datasource is not valid");
		return -4;
	}
	if((fd_datasource_flags&FD_CLOEXEC) ){
		debugt("jug_sandbox_run","fd_datasource is declared close-on-exec");
		return -5;
	}
	fd_datasource_status=fcntl(fd_datasource,F_GETFL);

	if( ! (fd_datasource_status&O_RDWR)){
		if( fd_datasource_dir==0 &&  !(fd_datasource_status&O_RDONLY) ){
			debugt("jug_sandbox_run","fd_datasource dir 0 is not readible , (%d,%d)",fd_datasource_status&O_RDWR,fd_datasource_status&O_RDONLY);
			return -6;
		}
		if(fd_datasource_dir && !(fd_datasource_status&O_WRONLY)){
			debugt("jug_sandbox_run","fd_datasource dir 1 is not writable ");
			return -7;
		}
	}

	////init the out pipe
	if( pipe(out_pipe)==-1){
		debugt("jug_sandbox_run","cannot create output pipe");
		return -3;
	}
	if(pipe(in_pipe)==-1){
		debugt("jug_sandbox_run","cannot create input pipe");
		return -8;
	}

	close(in_pipe[0]);

	if(fd_datasource_dir){
		fail=dup2(in_pipe[1],fd_datasource);
		if(fail==-1){
			debugt("jug_sandbox_run","cannot dup2 in_pipe: %s",strerror(errno));
			return -9;
		}
	}

	/* receive output at out_pipe[0], */
	//init the execution state, not started yet.
	execution_state=0;
	//clone inside the sandbox
	struct clone_child_params* ccp=(struct clone_child_params*)malloc(sizeof(struct clone_child_params)*1);
	ccp->run_params_struct=run_params_struct;
	ccp->sandbox_struct=sandbox_struct;
	ccp->binary_path=binary_path;
	ccp->argv=argv;
	char* stack;
	char *stacktop;
	int STACK_SIZE=run_params_struct->stack_size_mb*1000000;
	stack=(char*)malloc(STACK_SIZE);
	if(!stack){
		debugt("jug_sandbox_run","cannot malloc the stack for this submission");
		return -1;
	}
	stacktop=stack;
	stacktop+=STACK_SIZE;
	pid_t pid=clone(jug_sandbox_child,stacktop,CLONE_NEWUTS|CLONE_NEWNET|CLONE_NEWPID|CLONE_NEWNS|SIGCHLD,(void*)ccp);
	if(pid==-1){
		debugt("jug_sandbox_run","cannot clone() the submission executer, linux error: %s",strerror(errno));
		return -2;
	}
	//set the global inner watcher process id
	innerwatcher_pid=pid;
	execution_state=1;

	//set up an alarm of maximum execution (wall clock) time.
	signal(SIGALRM,timeout_handler);
	alarm(sandbox_struct->walltime_limit_ms_default/1000);
	/*
	 *
	 *
	 *
	 * read the output from the submission
	 *
	 *
	 *
	 *
	 */

	int tmpfd=open("/tmp/out.data",O_CREAT| O_TRUNC | O_WRONLY,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
	if(tmpfd<0){
		debugt("jug_run","cannot open fd : %s",strerror(errno));
	}
	int flags=fcntl(out_pipe[0],F_GETFL,0);
	fcntl(out_pipe[0],F_SETFL,flags|O_NONBLOCK);
	int count=0;
	pid_t wpid;
	int iw_status;
	char buffer[256];
	int nequal;
	while( 1){
		pid_t wpid=waitpid(innerwatcher_pid,&iw_status,WNOHANG|WUNTRACED);
		if(wpid<0){
			debugt("jug_run","error waiting for the Inner Watcher : %s",strerror(errno));
			continue;
		}else if(wpid>0){
			//something happened to the inner watcher.
			if (WIFEXITED(iw_status)){
				debugt("jug_run","inner watcher exited with code %d",WEXITSTATUS(iw_status));
				execution_state=3;
				break;
			}
			else if(WIFSIGNALED(iw_status)){
				debugt("jug_run","inner watcher received a signal : %d",WTERMSIG(iw_status));
				execution_state=3;
				break;
			}else debugt("jug_run","inner watcher event received, but not exited & not signaled");
		}

		if(execution_state>1) break;

		//use simultaneous read


		count=read(out_pipe[0],buffer,255);
		if(count<0 && errno==EAGAIN) continue;
		if(count<0){
			debug("testing, error reading from the output pipe\n");
			//TODO: here, the judgement must be set to (server error);
			break;
		}
		nequal=ccp->run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count);
		if(nequal){
			debugt("jug_sandbox_run","compare_output returns : %d",nequal);
			debugt("jug_sandbox_run","WRONG ANSWER");
			//emulate alarm timeout signal, to kill the children
			kill(getpid(),SIGALRM);
			break;
		}
//		buffer[count]='\0';
//		//size_t ww=write(tmpfd,buffer,count);
//		write(STDOUT_FILENO,buffer,count);
//		//debugt("Captor", "%d written",ww);

	}
	if(!nequal) debugt("jug_sandbox_run","CORRECT ANSWER");

	close(tmpfd);
	close(out_pipe[0]);
	close(out_pipe[1]);
	/*
	 *
	 *
	 *
	 * end
	 *
	 *
	 *
	 *
	 */

	//wait for the submission to be executed
	//may be this has to be outside, who knows

	return 0;
}
/**
 * timout_handler()
 */
void timeout_handler(int sig){
	debugt("OuterWatcher"," timeout received, killing the child (InnerWatcher), sign:%d",sig);
	//if the submission already terminated,
	if( execution_state==3) return;
	int error=kill(innerwatcher_pid,9);
	if(error) kill(innerwatcher_pid,9);
	if(error){
		//we've failed to kill the execution
		execution_state=2;
		debugt("timeout_handler","inner watcher process %d can't be killed", innerwatcher_pid);
		return ;
	}
	//execution succesfully killed
	execution_state=3;
	debugt("timeout_handler","inner watcher %d has been succesfully killed",innerwatcher_pid);
}



/**
 * rlimit_cpu_handler
 *
 */
static void rlimit_cpu_handler(int sig){
	//supposed sig=SIGXCPU
	if(sig==SIGXCPU){
		debugt("rlimit_cpu_handler","SIGXCPU received");

	}else if(sig==SIGXFSZ){
		debugt("rlimit_handler","SIGXFSZ received");
	}
	fflush(stdout);
	execution_state=3;
}
/**
 * jug_sandbox_child()
 *
 *
 */
int jug_sandbox_child(void* arg){
	int fail=0;
	//
	debugt("jug_sandbox_child","InnerWatcher starting...");
	//
	struct clone_child_params* ccp=(struct clone_child_params*)arg;

	sethostname("freeJugSandbox",30);
	setdomainname("freeJugDomain",30);
	//we're using linux cgroups
	if(ccp->sandbox_struct->use_cgroups){
		fail=cgroup_attach_task(ccp->sandbox_struct->sandbox_cgroup);
		if(fail){
			debugt("jug_sandbox_child",cgroup_strerror(fail) );
			exit(1);
		}
	}

	//chrooting
	char wd[100];
	fail=chdir(ccp->sandbox_struct->chroot_dir);
	if(fail){
		debugt("jug_sandbox_child","chdir() failed");
		exit(2);
	}

	fail= chroot(".");
	if( fail){
		debugt("jug_sandbox_child","error chrooting, linux error : %s\n",strerror(errno));
		exit(3);
	}

	//remount the proc so it prints the new stuff (CLONE_NEWPID to work)
	//the clone CLONE_NEWNS makes it possible that each submission has its own mount points
	fail= umount("/proc");
	if(fail && errno!=EINVAL){
		//because EINVAL means /proc is not a mount point
		debugt("jug_sandbox_child","error unmount sandbox /proc, linux error : (%d,%s)\n",errno,strerror(errno));
		exit(4);
	}
	fail= mount("none","/proc","proc",MS_RDONLY,NULL);
	if(fail){
		debugt("jug_sandbox_child","error remounting sandbox /proc, linux error : %s\n",strerror(errno));
		exit(4);
	}

	//pipe stuff, doing it in InnerWatcher side
	//redirect stdout to the output transferring pipe
	while (1){
		fail=dup2(out_pipe[1],STDOUT_FILENO);
		if(fail==-1){
			debugt("jug_sandbox_child","cannot dup2 the output of the pipe : %s",strerror(errno));
			if(errno==EINTR)
				debugt("jug_sandbox_child","interruption");
			else return 78;
		}else break;
	}

	//if the user provides a direct fd to read from, redirect the stdin to it
	if(ccp->run_params_struct->fd_datasource_dir==0){
		dup2(ccp->run_params_struct->fd_datasource,STDIN_FILENO);
		//but, if they provide a fd where they supposed to write the data, redirect stdin to the
		// receiving end of the input transferring pipe.
	}else{
		dup2(in_pipe[0],STDIN_FILENO);
	}

	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);

	//Fork to the submission binary
	////fork
	pid_t ns_pid=fork();
	if(ns_pid>0){
		//i'm the direct father of the submission
		//close(out_pipe[0]);
		//close(out_pipe[1]);
		int exitstatus;
		wait(&exitstatus);
		if(WIFEXITED(exitstatus))
			debugt("child:","GrandChild exited, exit code : %d",WEXITSTATUS(exitstatus) );
		else if(WIFSIGNALED(exitstatus))
			debugt("child:","GrandChild signaled, signal : %d",WTERMSIG(exitstatus) );
		else
			debugt("child:","GrandChild exited with status : %d",exitstatus );

		fflush(stdout);

		exit(5);
		//parent
	}else if(ns_pid<0){
		debug("cannot fork the grandchild, exiting...");
		exit(6);
	}

	//define SIGXCPU handler
	if(signal(SIGXCPU,rlimit_cpu_handler)==SIG_ERR){
		debug("The Signal Is Not Assigned");
	}



	//GrandChild code



	//set rlimits
	//limit cpu usage
	struct rlimit cpu_rlimit;
	getrlimit(RLIMIT_CPU,&cpu_rlimit);
	cpu_rlimit.rlim_cur=ccp->run_params_struct->time_limit_ms/1000;
	cpu_rlimit.rlim_max=cpu_rlimit.rlim_cur+1;//2 seconds of margin
	fail=setrlimit(RLIMIT_CPU,&cpu_rlimit);
	if(fail){
		debugt("jug_sandbox_run","cannot set cpu rlimit");
		exit(100);
	}
	//limit memory usage
	struct rlimit as_rlimit;
	as_rlimit.rlim_cur=ccp->run_params_struct->mem_limit_mb*1000000;
	as_rlimit.rlim_max=as_rlimit.rlim_cur;
	fail=setrlimit(RLIMIT_AS,&as_rlimit);
	if(fail){
		debug("jug_sandbox_run","cannot setrlimit Adress Space");
		exit(101);
	}


	//drop all the priveleges
	struct passwd* pwd_nobody=getpwnam("nobody");
	uid_t nobody_id=pwd_nobody->pw_uid;
	fail=setgid(pwd_nobody->pw_gid);
	if(fail){
		debugt("jug_sandbox_child","error setgid(), exiting ...");
		exit(110);
	}

	fail=setgroups(0,NULL);
	if(fail<0){
		debugt("jug_sandbox_child","cannot remove supplementary groups, exiting ...");
		exit(111);
	}

	fail=setuid(pwd_nobody->pw_uid);
	if(fail){
		printf("fail setuid, exiting ...");
		exit(112);
	}


	if(!setuid(0)){
		debugt("jug_sandbox_child","still root, exiting ...");
		exit(113);
	}else{
		debug("root properly droped :) ");
	}

	debugt("jug_sandbox_child","EUID:%d, RUID: %d",geteuid(),getuid());
	gid_t listg[20];

	//run the binary
	//fflush(stdout);


	execvp(ccp->binary_path,ccp->argv);
}
