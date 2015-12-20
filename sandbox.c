#include "sandbox.h"

/*
//TODO:	make the whole stuff thread-safe.
//TODO: remember to check the health of the rootfs before begining.
 *
 *
//TODO: think about using mount namespaces instead of chroot.
//TODO: Compiling layer
//TODO:	Interpreters limits (java, python,...)



@note:	there is a lack of sight for why system() lib function is not captured by
		the tracing technique, even if it's using exec() which a syscall that we can
		capture. for now, it can't do harm, because we're running with very unpriveleged
		user, and actually the attacker can rewrite any linux binary with hand wich is equivalent
		to running it.

@note:	/var/tmp is a directory used to save old files after reboot. obviously we don't need it to run a
		submission, so instead of removing it, which could affects the job of some interpreters, we mounted
		the directory /tmp over it. By doing so the content of /var/tmp (==/tmp) will be cleared after exiting.
		Exiting will destroy the mount namespace of the sandbox, unmounting /tmp.

 */







/**
 * jug_sandbox_run()
 * + the sandbox is supposedly inited
 * + if cgroups are not used, use setrlimit to limit memory usage
 *
 */

int jug_sandbox_run(
		struct run_params* run_params_struct,
		struct sandbox* sandbox_struct,
		char* binary_path,
		char*argv[]){
	//locals
	int watcher_pid;
	int received_bytes=0;
	//some checking
	//if the caller didn't want to specify some limits, replace them with the Config's defaults.
	if(run_params_struct->mem_limit_mb<0)
		run_params_struct->mem_limit_mb=sandbox_struct->mem_limit_mb_default;
	if(run_params_struct->time_limit_ms<0)
		run_params_struct->time_limit_ms=sandbox_struct->time_limit_ms_default;

	//debug: print limits as (cputime,walltime,memory)
	debugt("watcher","limits (%dms,%dms,%dMB)",
			run_params_struct->time_limit_ms,
			sandbox_struct->walltime_limit_ms_default,
			run_params_struct->mem_limit_mb);
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
	///more information on (pipe & stdout buffering) :
	/// http://www.pixelbeat.org/programming/stdio_buffering/
	if( pipe(run_params_struct->out_pipe)==-1){
		debugt("jug_sandbox_run","cannot create output pipe");
		return -3;
	}
	if(pipe(run_params_struct->in_pipe)==-1){
		debugt("jug_sandbox_run","cannot create input pipe");
		return -8;
	}
	close(run_params_struct->in_pipe[0]);

	if(fd_datasource_dir){
		fail=dup2(run_params_struct->in_pipe[1],fd_datasource);
		if(fail==-1){
			debugt("jug_sandbox_run","cannot dup2 in_pipe: %s",strerror(errno));
			return -9;
		}
	}

	/* receive output at out_pipe[0], */
	//init the execution state, not started yet.

	//clone inside the sandbox
	struct clone_child_params* ccp=(struct clone_child_params*)malloc(sizeof(struct clone_child_params)*1);
	ccp->run_params_struct=run_params_struct;
	ccp->sandbox_struct=sandbox_struct;
	ccp->binary_path=binary_path;
	ccp->argv=argv;
	char* stack;
	char *stacktop;
	int STACK_SIZE=sandbox_struct->stack_size_mb*1000000;
	stack=(char*)malloc(STACK_SIZE);
	if(!stack){
		debugt("jug_sandbox_run","cannot malloc the stack for this submission");
		return -1;
	}
	stacktop=stack;
	stacktop+=STACK_SIZE;
	watcher_pid=clone(jug_sandbox_child,stacktop,CLONE_NEWUTS|CLONE_NEWNET|CLONE_NEWPID|CLONE_NEWNS|SIGCHLD,(void*)ccp);
	if(watcher_pid==-1){
		debugt("jug_sandbox_run","cannot clone() the submission executer, linux error: %s",strerror(errno));
		return -2;
	}

	//set the global inner watcher process id
	run_params_struct->watcher_pid=watcher_pid;
	/*
	 *
	 * read the output from the submission
	 *
	 */
	int flags=fcntl(run_params_struct->out_pipe[0],F_GETFL,0);
	fcntl(run_params_struct->out_pipe[0],F_SETFL,flags|O_NONBLOCK);
	pid_t wpid;
	int count=0,endoffile=0,compout_detected=0,iw_status,nequal,spawn_succeeded=1;
	int watcher_alive=1,were_done=0;
	char buffer[256];
	short cnfg_kill_on_compout=sandbox_struct->kill_on_compout;
	jug_sandbox_result watcher_result,comp_result,result;


	while(!were_done){
		//pull watcher state
		if(watcher_alive){
			wpid=waitpid(watcher_pid,&iw_status,WNOHANG);
			if(wpid<0){
				debugt("spawner","error waiting for the inner watcher");
				watcher_alive=0;
			}else if(wpid>0){
				if(WIFEXITED(iw_status)){
					//normal exit (voluntary exit)
					//I use the range (-127,127) to express my exit codes, so that I could
					// transmit information about errors.
					char exit_code=WEXITSTATUS(iw_status);
					if(exit_code<0)
						spawn_succeeded=0;
					else{
						watcher_result=(jug_sandbox_result)exit_code;
					}
					debugt("spawner","watcher exited normally, exit code : %d",exit_code);
					watcher_alive=0;

				}else if(WIFSIGNALED(iw_status)){
					//killed by a signal
					debugt("spawner","watcher killed by a signal, signal: %d",WTERMSIG(iw_status));
					watcher_result=JS_UNKNOWN;
					watcher_alive=0;
				}else{
					debugt("spawner","unhandled event from the watcher");
					watcher_result=JS_UNKNOWN;
					watcher_alive=0;
				}
			}
		}
		//process's available output from the watcher.
		if(endoffile || compout_detected){
			if(!watcher_alive)
				break;
			else
				continue;
		}
		//use simultaneous read
		//ps:ublocking read
		count=read(run_params_struct->out_pipe[0],buffer,255);
		if(ccp->sandbox_struct->show_submission_output && count!=-1 )
			write(STDOUT_FILENO,buffer,count);
		if(count>0){ //some bytes are read
			nequal=ccp->run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,0);
			if(nequal==0)		comp_result=JS_CORRECT;
			else if(nequal>0)	comp_result=JS_WRONG;
			else				comp_result=JS_COMP_ERROR;
			//if an inequality is reported by the comparing function, and simultaneous mode is on, kill.
			if(nequal != 0){
				compout_detected=1;
				//communicate with the child via SIGUSR1 signal to inform them we detected false output
				if(cnfg_kill_on_compout)
					kill(watcher_pid,SIGUSR1);
			}
			//detect too much of output (not test feature)
			//SIGUSR1 handler will kill the binary and make sure the watcher reports a good execution
			// with exit(JS_CORRECT). to report the output overflow to sandbox user, we will change the
			// value of comp_result flag to JS_OUTPUTLIMIT
			received_bytes+=count;
			if( (received_bytes/1000000)> ccp->sandbox_struct->output_size_limit_mb_default){
				debugt("watcher","too much generated output");
				kill(watcher_pid,SIGUSR1);
				comp_result=JS_OUTPUTLIMIT;
			}

		}
		else if(count==0){//end of file received
			endoffile=1;
			nequal=ccp->run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,1);
			if(nequal==0)			comp_result=JS_CORRECT;
			else if(nequal>0)		comp_result=JS_WRONG;
			else					comp_result=JS_COMP_ERROR;
		}else if(count<0){
			if(errno==EAGAIN){
				if(	!watcher_alive ){
					//actually in case the submission is killed or suicide, we won't need to compare at all, but let's do it.
					//last call to compare_output
					nequal=ccp->run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,1);
					if(nequal==0)			comp_result=JS_CORRECT;
					else if(nequal>0)		comp_result=JS_WRONG;
					else					comp_result=JS_COMP_ERROR;
					//we're done then
					were_done=1;
				}
				continue;
			}else{
				comp_result=JS_PIPE_ERROR;
				compout_detected=1;
				//send SIGUSR1
				if(cnfg_kill_on_compout)
					kill(watcher_pid,SIGUSR1);

			}
		}


	}
	//end while (we're done)
	//fflush(stderr);
	//fflush(stdout);
	if(spawn_succeeded){
		if(watcher_result==JS_CORRECT){
			result=comp_result;
		}
		else result=watcher_result;
		debugt("spawner","Watcher result : %s",jug_sandbox_result_str(watcher_result));
		debugt("spawner","Judging result : %s",jug_sandbox_result_str(result));
	}

	else
		debugt("spawner","spawning failed before execvp()");

	//result
	run_params_struct->result=result;

	/*
	 * Close all opened file descriptors
	 * Free All Malloced memory areas
	 */

	close(run_params_struct->out_pipe[0]);
	close(run_params_struct->out_pipe[1]);
	free(ccp->sandbox_struct->chroot_dir);
	free(ccp);
	free(stack);


	return 0;
}


/**
 * jug_sandbox_child(), aka Watcher
 *
 *
 */
int jug_sandbox_child(void* arg){
	int fail=0;
	//
	debugt("watcher","Starting...");
	//
	struct clone_child_params* ccp=(struct clone_child_params*)arg;

	sethostname("KudosJudgeHostName",30);
	setdomainname("KudosJudgeDomain",30);
	//we're using linux cgroups
	if(ccp->sandbox_struct->use_cgroups){
		fail=cgroup_attach_task(ccp->sandbox_struct->sandbox_cgroup);
		if(fail){
			debugt("jug_sandbox_child",cgroup_strerror(fail) );
			exit(-1);
		}
	}

	//chrooting
	char wd[100];
	fail=chdir(ccp->sandbox_struct->chroot_dir);
	if(fail){
		debugt("jug_sandbox_child","chdir() failed");
		exit(-2);
	}

	fail= chroot(".");
	if( fail){
		debugt("jug_sandbox_child","error chrooting, linux error : %s\n",strerror(errno));
		exit(-3);
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
	while (1){
		fail=dup2(ccp->run_params_struct->out_pipe[1],STDOUT_FILENO);
		if(fail==-1){
			debugt("jug_sandbox_child","cannot dup2 the output of the pipe : %s",strerror(errno));
			if(errno==EINTR)
				debugt("jug_sandbox_child","interruption");
			else return -5;
		}else break;
	}

	//if the user provides a direct fd to read from, redirect the stdin to it
	if(ccp->run_params_struct->fd_datasource_dir==0){
		dup2(ccp->run_params_struct->fd_datasource,STDIN_FILENO);
		//but, if they provide a fd where they supposed to write the data, redirect stdin to the
		// receiving end of the input transferring pipe.
	}else{
		dup2(ccp->run_params_struct->in_pipe[0],STDIN_FILENO);
	}


	close(ccp->run_params_struct->out_pipe[0]);
	close(ccp->run_params_struct->out_pipe[1]);
	close(ccp->run_params_struct->in_pipe[0]);
	close(ccp->run_params_struct->in_pipe[1]);

	//Fork to the submission binary
	//set up an alarm of maximum execution (wall clock) time.
	signal(SIGALRM,watcher_sig_handler);
	alarm(ccp->sandbox_struct->walltime_limit_ms_default/1000);

	////fork
	debugt("watcher","forking to the binary...");
	binary_pid=fork();
	if(binary_pid<0){
		debugt("watcher","cannot fork the grandchild, exiting...");
		exit(-6);
	}
	if(binary_pid>0){
		//close(out_pipe[0]);
		//close(out_pipe[1]);
		//i'm the direct parent of the submission
		binary_state=JS_BIN_RUNNING;
		unsigned long mem_used=0;
		int estatus,stop_sig;
		int sigxcpu_received=0,sigsegv_received=0,mem_limit_exceeded=0,bin_killed=0;
		int d_=0;
		long binary_forked_pid;
		//consume the first stopping event

		wait(&estatus);
		debugt("[watcher]","first stop with sig:%d, binary:%d",WSTOPSIG(estatus),binary_pid);
		//		if (ptrace(PTRACE_SETOPTIONS, binary_pid, 0, PTRACE_O_TRACEEXIT)) {
		//			debugt("watcher"," ptrace(PTRACE_SETOPTIONS, ...) failed : %s",strerror(errno));
		//			exit(-21);
		//		}

		if(ptrace(PTRACE_SETOPTIONS,binary_pid,0,
				PTRACE_O_TRACEEXIT |
				PTRACE_O_TRACEFORK |
				PTRACE_O_TRACEVFORK|
				PTRACE_O_TRACECLONE
		)){
			debugt("watcher"," ptrace(PTRACE_SETOPTIONS,...) failed: %s",strerror(errno));
			exit(-21);
		}
		//		if(ptrace(PTRACE_SETOPTIONS,binary_pid,0,PTRACE_O_TRACEVFORK)){
		//			debugt("watcher"," ptrace(PTRACE_SETOPTIONS, ...,PTRACE_O_TRACEVFORK) failed: %s",strerror(errno));
		//			exit(-21);
		//		}
		//		if(ptrace(PTRACE_SETOPTIONS,binary_pid,0,PTRACE_O_TRACECLONE)){
		//			debugt("watcher"," ptrace(PTRACE_SETOPTIONS, ...,PTRACE_O_TRACECLONE) failed: %s",strerror(errno));
		//			exit(-21);
		//		}
		//		if(ptrace(PTRACE_SETOPTIONS,binary_pid,0,PTRACE_O_TRACEEXEC)){
		//			debugt("watcher"," ptrace(PTRACE_SETOPTIONS, ...,PTRACE_O_TRACEEXEC) failed: %s",strerror(errno));
		//			exit(-21);
		//		}
		ptrace(PTRACE_CONT,binary_pid,0,NULL);
		//watcher main loop
		while(1){
			waitpid(binary_pid,&estatus,0);

			if(WIFSTOPPED(estatus)){
				stop_sig=WSTOPSIG(estatus);
				debugt("watcher","binary stopped by the signal : %d",stop_sig);
				//stopped right before exiting ???
				if( (stop_sig==SIGTRAP) && (estatus & (PTRACE_EVENT_EXIT<<8))){
					mem_used=jug_sandbox_memory_usage(binary_pid);
					int cputime_used=jug_sandbox_cputime_usage(binary_pid);
					debugt("watcher","before exit check : mem:%ldB\tcputime:%ldms",mem_used,cputime_used);
					if(mem_used>0 && (mem_used/1000000) > ccp->run_params_struct->mem_limit_mb){
						debugt("watcher","out of allowed memory");
						mem_limit_exceeded=1;
					}
					//NOTE: I think flushing is not necessary, because the read() in the other part
					//of the pipe is clearing the buffer each time it reads.
					//fflush(stdout);
				}
				//shit, he's trying to fork. kill it.

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
				//stopped by SIGCHLD, may be trying to execute system()
				//actually this shouldn't happen: sigchld shouldn't be delivered to the binary
				//because that means that the binary successeded to fork.
				//				if(stop_sig==SIGCHLD){
				//					debugt("watcher","binary received SIGCHLD");
				//					kill(binary_pid,SIGKILL);
				//					bin_killed=1;
				//				}

				fail = ptrace(PTRACE_CONT,binary_pid,NULL,NULL);
				//debugt("watcher","binary continuing ...");
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
	//ptraceed ...

	if(ptrace(PTRACE_TRACEME, 0, (char *)1, 0) < 0){
		debugt("binary","cannot PTRACE_TRACEME");
		char* p;char c;c=*p;
		exit(99);
	}
	//printf("Hacked By Ouadev01\n");
	//fflush(stdout);
	//set rlimits

	//limit cpu usage
	struct rlimit cpu_rlimit;
	getrlimit(RLIMIT_CPU,&cpu_rlimit);
	cpu_rlimit.rlim_cur=ccp->run_params_struct->time_limit_ms/1000;
	cpu_rlimit.rlim_max=cpu_rlimit.rlim_cur+2;//2 seconds of margin
	fail=setrlimit(RLIMIT_CPU,&cpu_rlimit);
	if(fail){
		debugt("jug_sandbox_run","cannot set cpu rlimit");
		exit(100);
	}
	//limit memory usage
	struct rlimit as_rlimit;
	as_rlimit.rlim_cur=1.1*ccp->run_params_struct->mem_limit_mb*1000000;
	as_rlimit.rlim_max=as_rlimit.rlim_cur;
	fail=setrlimit(RLIMIT_AS,&as_rlimit);
	if(fail){
		debug("jug_sandbox_run","cannot setrlimit Adress Space");
		exit(101);
	}
	//unlimit stack
	struct rlimit stack_rlimit;
	stack_rlimit.rlim_cur=stack_rlimit.rlim_max=RLIM_INFINITY;
	setrlimit(RLIMIT_STACK,&stack_rlimit);

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
		debugt("binary","still root, exiting ...");
		exit(113);
	}

	//
	long mem_used=jug_sandbox_memory_usage(getpid());
	int cputime_used=jug_sandbox_cputime_usage(getpid());
	debugt("...Binary..","Memory Initial : mem:%ldB\tcputime:%ldms",mem_used,cputime_used);

	//oof, execute
	char* sandbox_envs[2]={
			"MESSAGE=Nice, you got here? send me ENV to ouadimjamal@gmail.com",
			NULL};


	ccp->argv[0]="/KudosBinary";
	debugt("binary","line before launching (%s)",ccp->binary_path);
	execvpe(ccp->binary_path,ccp->argv,sandbox_envs);
	debugt("binary","error execvp, %d, %s",errno==EFAULT,strerror(errno));
	return -999;
}



/**
 * jug_sandbox_init()
 *  all the initialization that should be done before executing a submission.
 */

int jug_sandbox_init(struct sandbox* sandbox_struct){

	//fill the sandbox structure

	int error;
	struct sandbox* sb=sandbox_struct;
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
	//output size limit
	//output_size_limit_mb_default
	if( ( value=jug_get_config("Executer","output_size_limit_mb") )==NULL ){
		debugt("sandbox","cannot read output_size_limit_mb config");
		return -1;
	}
	sb->output_size_limit_mb_default=atoi(value);
	//stack size
	if( ( value=jug_get_config("Executer","stack_size_mb") )==NULL ){
		debugt("sandbox","cannot read time_limit_ms config");
		return -1;
	}
	sb->stack_size_mb=atoi(value);
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
	//ERFS path
	if( (value=jug_get_config("Executer","chroot_dir_path"))==NULL){
		debugt("sandbox_init","cannot read chroot_dir_path");
		return -1;
	}
	sb->chroot_dir=(char*)malloc( sizeof(char)*(strlen(value)+5) );
	strcpy(sb->chroot_dir,value);
	//kill on wrong
	if( (value=jug_get_config("Executer","kill_on_compout") )==NULL ){
		debugt("sandbox","cannot read kill_on_compout config");
		return -1;
	}
	sb->kill_on_compout= atoi(value);
	//show submission's output on the screen
	//show_submission_output
	if( (value=jug_get_config("Executer","show_submission_output") )==NULL ){
		debugt("sandbox","cannot read show_submission_output config");
		return -1;
	}
	sb->show_submission_output= atoi(value);
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
unsigned long jug_sandbox_memory_usage(pid_t pid){
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

	debugt("mem_usage","content : %ld %ld %ld %ld %ld %ld %ld ",
			vmsize*page_size,
			resident*page_size,
			share*page_size,
			text*page_size,
			lib*page_size,
			data*page_size,
			dt*page_size);

	//
	close(procfd);
	return (vmsize*page_size);

}


/*
 * void jug_sandbox_print_stats()
 */
unsigned long jug_sandbox_cputime_usage(pid_t pid){
	int procfd,r,i;
	int utime,stime;
	long cputime;
	long page_size=sysconf(_SC_PAGESIZE);

	char stat[200], content[200];

	sprintf(stat,"/proc/%d/stat",(int)pid);
	FILE* file=fopen(stat,"r");
	if(file==NULL){
		debugt("js_print_usage","failed to open statm : %s",strerror(errno));
		return ;
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
		debugt("watcher_sig_handler","binary process %d (inside pid_ns) has been succesfully killed",binary_pid);
	}

	//SIGUSR1: sent from the parent, wrong output detectedn or pipe error
	if(sig==SIGUSR1){
		debugt("watcher_sig_handler","SIGUSR1 received, wrong output detected");
		error=kill(binary_pid,SIGKILL);
		if(error){
			debugt("watcher_sig_handler","binary process %d(inside pid_ns) can't be killed", binary_pid);
			return;
		}
		binary_state=JS_BIN_COMPOUT;
	}
}






















