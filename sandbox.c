#include "sandbox.h"

/*
 *
//TODO: create the full file tree with : interface.h, protocol.h, queue.h, ...
 * and a main.c that daemonize the judge, add restart.sh script to relaunch the judge to ease
 * the debugging loop. add a client that send simple data to the daemon, in order to simplify the task for Bq.
//TODO: configure a mode where the judge doesn't need the ERFS to work,
 * in order to let other developers test their parts without caring about the ERFS.
//TODO: edit signal mask in the watcher process to ignore unused signals
//TODO: remember to add file size rlimit : RLIMIT_FSIZE
//TODO: remember to close file descriptors before execing.
//TODO: remember to check the health of the rootfs before begining.
 *
 *
//TODO: think about using mount namespaces instead of chroot.
//TODO: Compiling layer
//TODO:	Interpreters limits (java, python,...)
 *
 *
@note:	/var/tmp is a directory used to save old files after reboot. obviously we don't need it to run a
		submission, so instead of removing it, which could affects the job of some interpreters, we mounted
		the directory /tmp over it. By doing so the content of /var/tmp (==/tmp) will be cleared after exiting.
		Exiting will destroy the mount namespace of the sandbox, unmounting /tmp.

 */





/**
 * jug_sandbox_child(), aka Watcher
 *
 *
 */
//reside@[main_process], run@[template_forked_process|watcher_process]
int jug_sandbox_watcher(void* arg){
	int fail=0;
	char buffer[500];
	
	
	debugt("watcher","Starting...");

	//
	struct clone_child_params* ccp=(struct clone_child_params*)arg;

	//
	sethostname("KudosJudgeHostName",30);
	setdomainname("KudosJudgeDomain",30);

	//chrooting
	if(ccp->sandbox_struct->use_ERFS){
		//ERFS MODE ON
		char wd[100];
		fail=chdir(ccp->sandbox_struct->chroot_dir);
		if(fail){
			debugt("jug_sandbox_child","chdir() failed, dir:%s",ccp->sandbox_struct->chroot_dir);
			exit(-2);
		}
		fail= chroot(".");
		if( fail){
			debugt("jug_sandbox_child","error chrooting, linux error : %s\n",strerror(errno));
			exit(-3);
		}
	}else{
		debugt("watcher","ERFS mode off");
		fail=chdir("/");
		if(fail){
			debugt("jug_sandbox_child","chdir() to '/' failed");
			exit(-2);
		}
	}

	//remount the proc so it prints the new stuff (CLONE_NEWPID to work)
	//the clone CLONE_NEWNS makes it possible that each submission has its own mount points
	fail= umount("/proc");
	if(fail && errno!=EINVAL){
		//because EINVAL means /proc is not a mount point
		debugt("jug_sandbox_child","error unmount sandbox /proc, linux error : (%d,%s)\n",errno,strerror(errno));
		exit(-4);
	}
	fail= mount("none","/proc","proc",MS_RDONLY,NULL);
	if(fail){
		debugt("jug_sandbox_child","error remounting sandbox /proc, linux error : %s\n",strerror(errno));
		exit(-4);
	}
	
	//remount the /tmp directory, each submission sees its own stuff
	fail=umount("/tmp");
	if(fail && errno!=EINVAL){
		debugt("watcher","cannot umount /tmp : %s",strerror(errno));
		exit(-9);
	}

	fail=mount("none","/tmp","tmpfs",MS_NODEV|MS_NOEXEC,NULL);
	if(fail){
		debugt("watcher","error mounting /tmp: %s\n",strerror(errno));
		exit(-9);
	}
	//bind-mount /tmp to /var/tmp
	fail=mount("/tmp","/var/tmp","none",MS_BIND,NULL);
	if(fail){
		debugt("watcher","cannot bind /tmp to /var/tmp:%s",strerror(errno));
		exit(-9);
	}
	//	pipe stuff, doing it in InnerWatcher side
	//	redirect stdout to the output transferring pipe
	fail=dup2(ccp->run_params_struct->out_pipe[1],STDOUT_FILENO);
	if(fail==-1){
		debugt("watcher","cannot dup2 the output of the pipe : %s",strerror(errno));
		if(errno==EINTR)
			debugt("watcher","interruption");
		else return -5;
	}
	fail=close(ccp->run_params_struct->out_pipe[1]);
	if(fail==-1) debugt("XXXXXXXXXX", "close failed 1 : %s", strerror(errno));

	//fearture-test
	int binary_input_pipe[2];
	int flags;
	signal(SIGPIPE,watcher_sig_handler);

	fail=pipe(binary_input_pipe);
	if(fail==-1){
		debugt("watcher", "cannot create the input pipe to Binary : %s", strerror(errno));
		exit(-9);
	}
	//debugt("watcher", "binary_input_pipe (%d, %d)", binary_input_pipe[0], binary_input_pipe[1]);
	//link Binary input to the reading end of the pipe.
	fail=dup2(binary_input_pipe[PIPE_READFROM], STDIN_FILENO);
	//debugt("watcher", "binary_input_pipe[0] is dup2 to %d", fail);


	//close(binary_input_pipe[PIPE_READFROM]);

	//check the writing end of our binary pipe
	flags=fcntl(binary_input_pipe[PIPE_WRITETO],F_GETFD);
	if(flags<0){
		debugt("watch", "binary_input_pipe[PIPE_WRITETO] is invalid");
		exit(-50);
	}
	//ublock write to binary pipe
	flags=fcntl(binary_input_pipe[PIPE_WRITETO],F_GETFL,0);
	fcntl(binary_input_pipe[PIPE_WRITETO],F_SETFL,flags|O_NONBLOCK);

 	flags=fcntl(binary_input_pipe[PIPE_WRITETO],F_GETFD);
	if(flags<0){
		debugt("watch", "binary_input_pipe[PIPE_WRITETO] is invalid");
		exit(-45);
	}
	
		//ublock read from in_pipe
	flags=fcntl(ccp->run_params_struct->in_pipe[PIPE_READFROM],F_GETFL,0);
	fcntl(ccp->run_params_struct->in_pipe[PIPE_READFROM],F_SETFL,flags|O_NONBLOCK);

	
	//close(ccp->run_params_struct->in_pipe[1]); //already closed in the Template process.

		//check fds
	flags=fcntl(binary_input_pipe[PIPE_WRITETO],F_GETFD);
	if(flags<0){
		debugt("watch", "binary_input_pipe[PIPE_WRITETO] is invalid");
		exit(-30);
	}

	//Fork to the submission binary
	//set up an alarm of maximum execution (wall clock) time.
	signal(SIGALRM,watcher_sig_handler);
	if(signal(SIGUSR2,watcher_sig_handler)==SIG_ERR){
		debugt("watcher", "error subscribing to SIGUSR1, %s", strerror(errno));
		exit(-3);
	}
	
	alarm(ccp->sandbox_struct->walltime_limit_ms_default/1000);

	////forking to the process that will execute the binary.
	debugt("watcher","forking to the binary...");
	//WATCHER FORKs TO THE SUBMISSION
		//check fds
	flags=fcntl(binary_input_pipe[PIPE_WRITETO],F_GETFD);
	if(flags<0){
		debugt("watch", "binary_input_pipe[PIPE_WRITETO] is invalid");
		exit(-40);
	}
	binary_pid=fork();
	if(binary_pid<0){
		debugt("watcher","cannot fork the grandchild, exiting...");
		exit(-6);
	}
	if(binary_pid>0){
		//I'm the direct parent of the submission
		binary_state=JS_BIN_RUNNING;
		 long mem_used=0;
		int estatus,stop_sig;
		int sigxcpu_received=0,sigsegv_received=0,mem_limit_exceeded=0,bin_killed=0;
		int d_=0;
		long binary_forked_pid;

		//consume the first stopping event
		wait(&estatus);
		if(ptrace(PTRACE_SETOPTIONS,binary_pid,0,
				PTRACE_O_TRACEEXIT |
				PTRACE_O_TRACEFORK |
				PTRACE_O_TRACEVFORK|
				PTRACE_O_TRACECLONE
		)){
			debugt("watcher"," ptrace(PTRACE_SETOPTIONS,...) failed: %s",strerror(errno));
			exit(-21);
		}
		ptrace(PTRACE_CONT,binary_pid,0,NULL);


		//watcher main loop
		int _datasource_transfer_size=40000;
		ssize_t splice_size=0;
		ssize_t splice_size_acc=0;
		char splice_buffer[100000];

		debugt("watcher", "datasource size : %d", ccp->run_params_struct->datasource_size);
	
		while(1){
			//write data
			if(splice_size_acc < ccp->run_params_struct->datasource_size){
				
		
				splice_size=splice(	ccp->run_params_struct->in_pipe[PIPE_READFROM], NULL ,
									binary_input_pipe[PIPE_WRITETO], NULL,
									_datasource_transfer_size,
									SPLICE_F_NONBLOCK);

				if(splice_size<0 && errno!=EAGAIN){
					//error transferring data to Binary
					debugt("watcher", "error transferring feed to Binary : %s", strerror(errno));
					close(binary_input_pipe[PIPE_WRITETO]);
					exit(-22);
				}else if(splice_size>=0) {
					debugt("watcher", "transferred to Binary : %d", (int)splice_size);
				}
				if(splice_size>=0){
						splice_size_acc += splice_size;
					}
				if(  splice_size_acc>= ccp->run_params_struct->datasource_size){
					debugt("watcher", "closing the binary_input_pipe[write to]");
					close( binary_input_pipe[PIPE_WRITETO]);
				}
			}

			//check execution status
			fail=waitpid(binary_pid,&estatus,WNOHANG);
			if(fail==0){
				//WNOHANG specified, so 0 means no change occurred , continue
				continue;
			}else if(fail==-1){
				debugt("watcher", "error in waitpid on Binary : %s", strerror(errno));
				exit(-60);
			}

			if(WIFSTOPPED(estatus)){
				stop_sig=WSTOPSIG(estatus);
				debugt("watcher","binary stopped by the signal : %d",stop_sig);
				//stopped right before exiting ???
				if( (stop_sig==SIGTRAP) && (estatus & (PTRACE_EVENT_EXIT<<8))){
					mem_used=jug_sandbox_memory_usage(binary_pid);
					int cputime_used=jug_sandbox_cputime_usage(binary_pid);
					if(mem_used>0 && cputime_used>0) debugt("watcher","before exit check : mem:%ldMB\tcputime:%ldms",mem_used/1000000,cputime_used);
					if(mem_used>0 && (mem_used/1000000) > ccp->run_params_struct->mem_limit_mb){
						debugt("watcher","out of allowed memory");
						mem_limit_exceeded=1;
					}
				}
				//sh  it, he's trying to fork. kill it.

				if( 	(estatus>>8 == (SIGTRAP | (PTRACE_EVENT_FORK<<8))) ||
						(estatus>>8 == (SIGTRAP | (PTRACE_EVENT_CLONE<<8)))||
						(estatus>>8 == (SIGTRAP | (PTRACE_EVENT_VFORK<<8)))  ){
					debugt("watcher","before Clone() flavor stopping...");
					ptrace(PTRACE_GETEVENTMSG,binary_pid,0,&binary_forked_pid);
					debugt("watcher","Binary's child pid : %ld",binary_forked_pid);
					kill(binary_forked_pid,SIGKILL);
					kill(binary_pid,SIGKILL);
					bin_killed=1;//so that the result be : runtime error
				}
				//shit, he's trying to exec(), kill it
				if( estatus>>8 == (SIGTRAP | (PTRACE_EVENT_EXEC<<8)) ){
					debugt("watcher","before exec() flavor stopping...-");
					kill(binary_pid,SIGKILL);
					bin_killed=1;
				}
				//out of cputime signal
				if(stop_sig==SIGXCPU && !sigxcpu_received){
					debugt("watcher","binary received SIGXCPU");
					sigxcpu_received=1;
				}
				//memory related stuff
				if(stop_sig==SIGSEGV){
					debugt("watcher","binary received SIGSEGV");
					sigsegv_received=1;
					mem_used=jug_sandbox_memory_usage(binary_pid);
					if(mem_used>=0 && (mem_used/1000000) >= ccp->run_params_struct->mem_limit_mb){
						debugt("watcher","binary received SIGSEGV, and mem limit exceeded");
						mem_limit_exceeded=1;
					}
					//
					//sigsegv if segv must be killed
					kill(binary_pid,SIGKILL);
				}

				fail = ptrace(PTRACE_CONT,binary_pid,NULL,NULL);
				debugt("watcher","binary continuing ...");
				if(fail==-1) debugt("watcher","ptrace_cont failed");
			}
			else if(WIFEXITED(estatus)){
				debugt("watcher","binary exited, exit code : %d",WEXITSTATUS(estatus) );
				//we will return JS_CORRECT
				break;
			}
			else if(WIFSIGNALED(estatus)){
				debugt("watcher","binary killed by a signal : %d",WTERMSIG(estatus) );
				//CHECK: also exit with the verdict.
				bin_killed=1;
				break;
			}
			else{
				debugt("watcher","binary received unknown event, status : %d",estatus );
				break;
			}
		}
		//analyzing the situation & return a convenient execution verdict.
		//fflush(stdout);
		//fflush(stderr);
		debugt("watcher", "(%d) Total transferred to Binary : %d",ccp->run_params_struct->thread_order, splice_size_acc);
		unsigned int tl=4000000000;
		int flush_acc=0;
		while(1){
			fail=read(ccp->run_params_struct->in_pipe[PIPE_READFROM], buffer, sizeof(buffer) );
			if(fail<0 && errno!=EAGAIN){
				debugt("watcher", "error flushing in pipe : %s", strerror(errno));
				exit(JS_RUNTIME);
			}
			if(fail>=0){
				flush_acc += fail;
			}
			if( (splice_size_acc+flush_acc) >= ccp->run_params_struct->datasource_size ){
				break;
			}
		}
	
		
		debugt("watcher", "(%d) in_pipe flushed : %d",ccp->run_params_struct->thread_order, flush_acc);

		if(sigxcpu_received)
			exit(JS_TIMELIMIT);
		else if(mem_limit_exceeded)
			exit(JS_MEMLIMIT);
		else if(binary_state==JS_BIN_WALLTIMEOUT)
			exit(JS_WALL_TIMELIMIT);
		else if(binary_state==JS_BIN_COMPOUT)
			exit(JS_CORRECT);
		else if(sigsegv_received || bin_killed){
			//bin_killed can detect by-zero-division exception for example
			//sometimes, being killed by a signal results from consuming all the memory, that's why
			// we check first for the mem_limit
			if(mem_limit_exceeded)
				exit(JS_MEMLIMIT);
			else exit(JS_RUNTIME);
		}else{
			exit(JS_CORRECT);
		}

	}






	//GrandChild code (binary)
	return jug_sandbox_binary(ccp, binary_input_pipe);




}

/**
*
* jug_sandbox_binary
*/

int jug_sandbox_binary( struct clone_child_params* ccp, int binary_input_pipe[2] ){

	//ptraceed ...
	if(ptrace(PTRACE_TRACEME, 0, (char *)1, 0) < 0){
		debugt("binary","cannot PTRACE_TRACEME");
		char* p;char c;c=*p;
		exit(99);
	}


	int fail;

	//close unused fds
	fail=close(ccp->run_params_struct->in_pipe[0]);
	if(fail==-1) debugt("YYYYYYYYYY", "close failed 1: %s", strerror(errno));
	fail=close(binary_input_pipe[1]);
	if(fail==-1) debugt("YYYYYYYYY", "close failed 2 : %s", strerror(errno));
	//set rlimits

	//limit cpu usage
	struct rlimit cpu_rlimit;
	getrlimit(RLIMIT_CPU,&cpu_rlimit);
	cpu_rlimit.rlim_cur=ccp->run_params_struct->time_limit_ms/1000;
	cpu_rlimit.rlim_max=cpu_rlimit.rlim_cur+2;//2 seconds of margin
	fail=setrlimit(RLIMIT_CPU,&cpu_rlimit);
	if(fail){
		debugt("binary","cannot set cpu rlimit");
		exit(100);
	}
	//limit memory usage
	struct rlimit as_rlimit;
	as_rlimit.rlim_cur=1.1*ccp->run_params_struct->mem_limit_mb*1000000;
	as_rlimit.rlim_max=as_rlimit.rlim_cur;
	fail=setrlimit(RLIMIT_AS,&as_rlimit);
	if(fail){
		debug("binary","cannot setrlimit Adress Space");
		exit(101);
	}
	//unlimit stack
	struct rlimit stack_rlimit;
	stack_rlimit.rlim_cur=stack_rlimit.rlim_max=RLIM_INFINITY;
	setrlimit(RLIMIT_STACK,&stack_rlimit);

	//drop all the priveleges
	struct passwd* pwd_nobody=getpwnam("nobody");
	uid_t nobody_id=pwd_nobody->pw_uid;
	// fail=setgid(pwd_nobody->pw_gid);
	// if(fail){
	// 	debugt("binary","error setgid(), exiting ...:%s",strerror(errno));
	// 	exit(110);
	// }

	// fail=setgroups(0,NULL);
	// if(fail<0){
	// 	debugt("binary","cannot remove supplementary groups, exiting ...");
	// 	exit(111);
	// }

	// fail=setuid(pwd_nobody->pw_uid);
	// if(fail){
	// 	debugt("binary","fail setuid, exiting ...");
	// 	exit(112);
	// }


	if(!setuid(0)){
		debugt("binary","still root, exiting ...");
		exit(113);
	}

	//
	long mem_used=jug_sandbox_memory_usage(getpid());
	int cputime_used=jug_sandbox_cputime_usage(getpid());
	debugt("Binary","Memory Before Exec : mem:%ldMB\tcputime:%ldms",mem_used/1000000,cputime_used);

	//oof, execute
	char* sandbox_envs[2]={
			"MESSAGE=Nice, you got here? send me ENV to ouadimjamal@gmail.com",
			NULL};




	ccp->argv[0]="/KudosBinary";
	//debugt("binary","%s", ccp->binary_path);
	execvpe(ccp->binary_path,ccp->argv,sandbox_envs);
	debugt("binary","error execvp, %d, %s",errno==EFAULT,strerror(errno));
	return -999;

}



/**
 * jug_sandbox_init()
 *  all the initialization that should be done before executing a submission.
 */

int jug_sandbox_init(){

	//fill the sandbox structure
	
	int error;
	struct sandbox* sb=(struct sandbox* )malloc(sizeof(struct sandbox));
	char* value;
	//add stack_size
	//mem limit mb
	if( (value=jug_get_config("Executer","mem_limit_mb") )==NULL ){
		debugt("sandbox","cannot read mem_limit_mb config");
		return -1;
	}
	sb->mem_limit_mb_default=atoi(value);
	//walltime limit
	if( ( value=jug_get_config("Executer","walltime_limit_ms") )==NULL ){
		debugt("sandbox","cannot read walltime_limit_ms config");
		return -1;
	}
	sb->walltime_limit_ms_default=atoi(value);
	//time limit
	if( ( value=jug_get_config("Executer","time_limit_ms") )==NULL ){
		debugt("sandbox","cannot read time_limit_ms config");
		return -1;
	}
	sb->time_limit_ms_default=atoi(value);
	

	//whether to use cgroups or not
	if( (value=jug_get_config("Executer","use_cgroups") )==NULL ){
		debugt("sandbox","cannot read use_cgroups config");
		return -1;
	}
	sb->use_cgroups=atoi(value);
	//whether to use setrlimit()
	if( (value=jug_get_config("Executer","use_setrlimit") )==NULL ){
		debugt("sandbox","cannot read use_setrlimit config");
		return -1;
	}
	sb->use_setrlimit= atoi(value);
	//whether to usre ERFS (execution root file system)
	if( (value=jug_get_config("Executer","use_ERFS"))==NULL){
		debugt("sandbox_init","cannot read use_ERFS");
		return -1;
	}
	sb->use_ERFS=atoi(value);
	//ERFS path
	if( (value=jug_get_config("Executer","chroot_dir_path"))==NULL){
		debugt("sandbox_init","cannot read chroot_dir_path");
		return -1;
	}
	sb->chroot_dir=(char*)malloc( sizeof(char)*(strlen(value)+5) );
	strcpy(sb->chroot_dir,value);

	


	//check if cgroup filesystem is properly mounted
	//old code here
	//check the sandbox root env.
	struct stat st={0};
	error=stat("/tmp",&st);
	if(error){
		debugt("sandbox_init","chroot's /tmp unfound, jug_rootfs is not ready");
		return -10;
	}
	//set global Sandbow object
	global_sandbox=sb;

	return 0;
}



struct sandbox* jug_sandbox_global(){
	return global_sandbox;
}



/**
 * jug_sandbox_print_result
 * JS_CORRECT,JS_WRONG,JS_TIMELIMIT,JS_WALL_TIMELIMIT,JS_MEMLIMIT,JS_RUNTIME,JS_PIPE_ERROR,JS_COMP_ERROR,JS_UNKNOWN
 */

const char* jug_sandbox_result_str(jug_sandbox_result result){
	const char* strings[12]={
			"CORRECT ANSWER",
			"WRONG ANSWER",
			"TIME LIMIT EXCEEDED",
			"WALL TIME LIMIT EXCEEDED",
			"MEMORY LIMIT EXCEEDED",
			"OUTPUT SIZE LIMIT EXCEEDED",
			"RUNTIME ERROR",
			"PIPE ERROR",
			"COMPARING FUNCTION ERROR",
			"UNKNOWN ERROR"};

	return strings[(int)result];
}

/**
 * jug_sanbdox_meomry_usage()
 * + since we use RLIMT_AS, that limits the whole memory space of the process.
 * + we will take VmSize as the calculated memory_usage
 */
//CHECK: check also the program text size, it seems like a bug to fill the memory with unused text code.
 long jug_sandbox_memory_usage(pid_t pid){
	int procfd,r;

	unsigned long vmsize,resident,share,text,lib,data,dt;
	long page_size=sysconf(_SC_PAGESIZE);
	char statm[200], content[200];

	sprintf(statm,"/proc/%d/statm",(int)pid);
	procfd=open(statm,O_RDONLY);
	if(procfd==-1){
		debugt("memory_usage","failed to open statm : %s",strerror(errno));
		return -1;
	}
	r=read(procfd,content,200);
	if(r<0){
		debugt("memory_usage","failed to read statm");
		return -2;
	}
	content[r]='\0';
	sscanf(content,"%ld %ld %ld %ld %ld %ld %ld",&vmsize,&resident,&share,&text,&lib,&data,&dt);

	//	debugt("mem_usage","content : %ld %ld %ld %ld %ld %ld %ld ",
	//			vmsize*page_size,
	//			resident*page_size,
	//			share*page_size,
	//			text*page_size,
	//			lib*page_size,
	//			data*page_size,
	//			dt*page_size);

	//
	close(procfd);
	return (vmsize*page_size);

}


/*
 * void jug_sandbox_print_stats()
 */
 long jug_sandbox_cputime_usage(pid_t pid){
	int procfd,r,i;
	int utime,stime;
	long cputime;
	long page_size=sysconf(_SC_PAGESIZE);

	char stat[200], content[200];

	sprintf(stat,"/proc/%d/stat",(int)pid);
	FILE* file=fopen(stat,"r");
	if(file==NULL){
		debugt("js_print_usage","failed to open statm : %s",strerror(errno));
		return -1;
	}

	for(i=1;i<=13;i++){
		fscanf(file,"%s",content);

	}

	fscanf(file,"%d %d",&utime,&stime);
	//stats
	cputime=(1000*(utime+stime))/sysconf(_SC_CLK_TCK) ; //ms

	return cputime;
}


/**
 * timout_handler()
 * executed in the watcher.
 */
void watcher_sig_handler(int sig){
	int error;
	//SIGALRM : timeout
	if(sig==SIGALRM){
		debugt("Watcher","timeout received, killing the binary (submission), sign:%d",sig);
		error=kill(binary_pid,SIGKILL);
		if(error){
			debugt("watcher_sig_handler","binary process %d(inside pid_ns) can't be killed", binary_pid);
			return;
		}
		//cancel the alarm
		alarm(0);
		//execution succesfully killed
		binary_state=JS_BIN_WALLTIMEOUT;
		debugt("watcher_sig_handler","binary process %d (inside pid_ns) has been successfully killed",binary_pid);
	}

	//SIGUSR2: sent from the parent, wrong output detected or pipe error
	else if(sig==SIGUSR2){
		//debugt("watcher_sig_handler","SIGUSR2 received, wrong output detected");
		error=kill(binary_pid,SIGKILL);
		if(error){
			debugt("watcher_sig_handler","binary process %d(inside pid_ns) can't be killed", binary_pid);
			return;
		}
		binary_state=JS_BIN_COMPOUT;
	}
	else{
		debugt("watcher_sig_handler", "received SIGNAL %d", sig);
	}
}










