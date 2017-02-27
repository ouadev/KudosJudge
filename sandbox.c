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
 * jug_sandbox_start
 */
int jug_sandbox_start(){
	int ret;
	//init sandbox
	global_sandbox=(struct sandbox* )malloc(sizeof(struct sandbox));
	ret=jug_sandbox_init(global_sandbox);
	if(ret){
		debugt("sandbox_start","cannot init the sandbox");
		return -1;
	}
	//init template
	ret=jug_sandbox_template_init();
	if(ret!=0){
		debugt("sandbox_start","cannot init template process");
		return -2;
	}
	//give them time to get ready
	sleep(1.5);

	return 0;
}


/**
 * jug_sanbdox_stop
 */
int jug_sandbox_stop(){
	jug_sandbox_template_term();

	return 0;

}


/**
 * entry function, may changed in the future after add compiling layer
 * (code for experiment only, many hardcoded stuff)
 */
jug_verdict_enum jug_sandbox_judge(jug_submission* submission){


	//////////////////////////////////
	pid_t pid,c_pid;
	int ret,status;
	int infd,rightfd;
	struct stat datasource_st;
	jug_sandbox_result result;

	infd=open(		submission->input_filename,O_RDWR);
	rightfd=open(	submission->output_filename,O_RDWR);

	if(infd==-1 || rightfd==-1){
		debugt("sandbx","infd (%d) or rightfd (%d) invalid : %s", infd, rightfd, strerror(errno) );
		debugt("sandbx","right_fd = %s",submission->output_filename);
		return VERDICT_INTERNAL;
	}
	//get the size of the datasource file
	stat(submission->input_filename, &datasource_st);
	//change the config.ini default parameters with your own stuff
	struct run_params runp;
	runp.mem_limit_mb=-1;//1220000;
	runp.time_limit_ms=-1;//1000;
	runp.fd_datasource=infd;
	runp.compare_output=compare_output;
	runp.fd_output_ref=rightfd;
	runp.thread_order=submission->thread_id;
	runp.datasource_size=datasource_st.st_size;

	int thread_order=submission->thread_id;
	//args
	if(global_sandbox->use_ERFS==1){
		//TODO:if ERFS, should copy/paste the binary to it.
		//may be you can mount the ramfs into the ERFS. think about security implications.
		//Or, copy the binaries to the /tmp of the ERFS...
		debugt("sandbox","use_ERFS enabled, must copy the binary to it, stopping ...");
		return VERDICT_INTERNAL;
	}
	char** args=(char**)malloc(3*sizeof(char**));
	args[0]=(char*)malloc(99);
	strcpy(args[0],submission->bin_path);
	args[1]=NULL;

	//make submission->bin_path binary runnable by less-privileged user.
	chmod(submission->bin_path,0755);//rwx,r-x,r-x
	//RUN
	ret=jug_sandbox_run_tpl(&runp,global_sandbox,args[0],args,submission->thread_id);
	debugt("judge","ALL:%d,SUCC:%d", global_sandbox->count_submissions, global_sandbox->count_submissions_success);
	if(ret!=0 || result==JS_COMP_ERROR || result==JS_UNKNOWN){
		debugt("judge","TESTUNIT-KO");
	}
	////////////

	close(infd);
	close(rightfd);
	free(args[0]);
	free(args);
	//some error duting running
	if(ret!=0){
		return VERDICT_INTERNAL;
	}
	//process sandbox result
	result=runp.result;
	switch(result){
	case JS_CORRECT:
		return VERDICT_ACCEPTED;
	case JS_WRONG:
		return VERDICT_WRONG;
	case JS_TIMELIMIT:
	case JS_WALL_TIMELIMIT:
		return VERDICT_TIMELIMIT;
	case JS_OUTPUTLIMIT:
		return VERDICT_OUTPUTLIMIT;
	case JS_RUNTIME:
		return VERDICT_RUNTIME;
	case JS_PIPE_ERROR:
	case JS_COMP_ERROR:
	case JS_UNKNOWN:
		return VERDICT_INTERNAL;
	}
	//if I Don't know :/
	return VERDICT_INTERNAL;
}



/**
 * run in template mode
 */
// [main_processus/some_thread]
int jug_sandbox_run_tpl(
		struct run_params* run_params_struct,
		struct sandbox* sandbox_struct,
		char* binary_path,
		char*argv[],
		int thread_order){
	//locals
	int i,fail,fd_datasource_flags,fd_datasource_status;
	int watcher_pid;
	int received_bytes=0;
	int compare_stage=0;
	//some checking
	//if the caller didn't want to specify some limits, replace them with the Config's defaults.
	if(run_params_struct->mem_limit_mb<0)
		run_params_struct->mem_limit_mb=sandbox_struct->mem_limit_mb_default;
	if(run_params_struct->time_limit_ms<0)
		run_params_struct->time_limit_ms=sandbox_struct->time_limit_ms_default;

	//debug: print limits as (cputime,walltime,memory)
//	debugt("watcher","limits (%dms,%dms,%dMB)",
//			run_params_struct->time_limit_ms,
//			sandbox_struct->walltime_limit_ms_default,
//			run_params_struct->mem_limit_mb);

	//check fd_in a fd_out
	//ps: fd_datasource is a fd to read from, it will be transmitted to Clone by pipe.
	run_params_struct->fd_datasource_dir=1;
	////use thread_order to route the submission to the appropriate file descriptor
	//parent side of multplixing.
	for(i=0;i<2;i++){
		run_params_struct->out_pipe[i]=io_pipes_out[thread_order][i];
		run_params_struct->in_pipe[i]=io_pipes_in[thread_order][i];
	}

	/* receive output at out_pipe[0], */
	//init the execution state, not started yet.

	//clone inside the sandbox
	struct clone_child_params* ccp=(struct clone_child_params*)malloc(sizeof(struct clone_child_params));
	ccp->run_params_struct=run_params_struct;
	ccp->sandbox_struct=sandbox_struct;
	ccp->binary_path=binary_path;
	ccp->argv=argv;
	//serialize
	void* serial;
	int serial_len=jug_sandbox_template_serialize(&serial,ccp);
	//clone request
	debugt("###","###################");
	debugt("run","new submission");
	//ASK TEMPLATE TO CLONE
	watcher_pid=jug_sandbox_template_clone(serial,serial_len);
	////////
	run_params_struct->watcher_pid=watcher_pid;
	//free memory
	free(serial);
	if(watcher_pid==-1){
		debugt("run_tpl","cannot clone() the template");
		return -2;
	}

//	//FIXME:TEST-UNIT (get out without waiting for output from cloned watcher)
//	debugt("testunit","watcher_id: %d",watcher_pid);
//	run_params_struct->result=JS_CORRECT;
//	return 0;
//	//TEST-UNIT


	/*
	 *
	 * read the output from the submission (Jug_sandbox_run_tpl)
	 *
	 */

	pid_t wpid;
	int count=0,endoffile=0,compout_detected=0,iw_status,nequal,spawn_succeeded=1;
	int watcher_alive=1,were_done=0,flags;;
	char buffer[256];
	short cnfg_kill_on_compout=sandbox_struct->kill_on_compout;
	int _datasource_transfer_size=40000; //40K, actually up to 64K is possible (linux's pipe buffer size).
	int ds_sent=0, ds_sent_acc=0;
	int fff=0;
	int datasource_file_size=0;
	jug_sandbox_result watcher_result,comp_result,result;
	//unblock read from the out_pipe
	flags=fcntl(run_params_struct->out_pipe[0],F_GETFL,0);
	fcntl(run_params_struct->out_pipe[0],F_SETFL,flags|O_NONBLOCK);
	//ublock read from the datasource fd
	flags=fcntl(run_params_struct->fd_datasource,F_GETFL,0);
	fcntl(run_params_struct->fd_datasource,F_SETFL,flags|O_NONBLOCK);
	//ublock write
	flags=fcntl(run_params_struct->in_pipe[PIPE_WRITETO],F_GETFL,0);
	fcntl(run_params_struct->in_pipe[PIPE_WRITETO],F_SETFL,flags|O_NONBLOCK);
	//check fds
	flags=fcntl(run_params_struct->in_pipe[PIPE_WRITETO],F_GETFD);
	if(flags<0){
		debugt("run_tpl","in_pipe[PIPE_WRITETO] is invalid : %s",strerror(errno));
		debugt("run_tpl","in_pipe[PIPE_WRITETO]==%d", run_params_struct->in_pipe[PIPE_WRITETO]);
		debugt("run_tpl", "thread_id : %d", thread_order);
		int j=0;
		for(j=0;j<10;j++){
			debugt("run_tpl","io_pipes_in[%d][PIPE_WRITETO]==%d",j,  io_pipes_in[j][PIPE_WRITETO] );
		}
		return -3;
	}
	flags=fcntl(run_params_struct->fd_datasource,F_GETFD);
	if(flags<0){
		debugt("run_tpl","fd_datasource is invalid: %s",strerror(errno));
		return -3;
	}
	
	//
	while(!were_done){
		//pull watcher state
		if(watcher_alive){
			wpid=waitpid(watcher_pid,&iw_status,WNOHANG);
			if(wpid<0){
				debugt("run_tpl","error waiting for the inner watcher : %s", strerror(errno));
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
					debugt("run_tpl","watcher exited normally, exit code : %d",exit_code);
					watcher_alive=0;


				}else if(WIFSIGNALED(iw_status)){
					//killed by a signal
					debugt("run_tpl","watcher killed by a signal, signal: %d",WTERMSIG(iw_status));
					watcher_result=JS_UNKNOWN;
					watcher_alive=0;
				}else{
					debugt("run_tpl","unhandled event from the watcher");
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
		//write to the watcher (template clone)
		//NOTE:think about jumping over this call if we eat up all the fd_datasource.
		if(ds_sent_acc < run_params_struct->datasource_size){
			ds_sent=sendfile(
					run_params_struct->in_pipe[PIPE_WRITETO],
					run_params_struct->fd_datasource,
					NULL,_datasource_transfer_size
			);

			if(ds_sent>=0){
				ds_sent_acc+=ds_sent;
				//debugt("run_tpl", "feeden to watcher (sandbox) : %d", ds_sent);
			}
			//if error writing: same bahaviour as False Comparing
			//		if(thread_order==1 && ds_sent>=0)
			//			debugt("run_tpl","order=%d, sendfile=%d",thread_order,ds_sent);
			if(ds_sent<0 && errno!=EAGAIN){
				debugt("run_tpl","senfile() error : %s",strerror(errno));
				comp_result=JS_PIPE_ERROR;
				compout_detected=1;
				//send SIGUSR1
				if(cnfg_kill_on_compout)
					kill(watcher_pid,SIGUSR1);
			}
			//close the stdin pipe after sending all the data.
			if(ds_sent_acc >= run_params_struct->datasource_size){
				//do nothing, everything is taken care of in the Watcher
				// if( close(run_params_struct->in_pipe[PIPE_WRITETO])== -1 ){
				// 	debugt("run_tpl", "cannot close writing end of stdin pipe");
				// }
				// debugt("run_tpl", "closing the pipe of stdin");
			}
		}


		//use simultaneous read
		//ps:ublocking read
		count=read(run_params_struct->out_pipe[PIPE_READFROM],buffer,255);

				if( count>=0)
					debugt("run_tpl","order=%d, read count=%d",run_params_struct->thread_order,count);

		if(ccp->sandbox_struct->show_submission_output && count!=-1 ){
			//write(STDOUT_FILENO,"~~~~~~~~~\n",10);
			//write(STDOUT_FILENO,buffer,count);
			buffer[count]='\0';

			debugt("stdout", "%s", buffer);
			//write(STDOUT_FILENO,"~~~~~~~~~\n",10);
		}
		if(count>0){ //some bytes are read
			nequal=ccp->run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,compare_stage);
			if(nequal==0)		comp_result=JS_CORRECT;
			else if(nequal>0)	comp_result=JS_WRONG;
			else				comp_result=JS_COMP_ERROR;
			compare_stage++;
			//
			//debugt("compare","stage=%d,nequal:%d",compare_stage,nequal);
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
				debugt("rn_tpl","too much generated output");
				kill(watcher_pid,SIGUSR1);
				comp_result=JS_OUTPUTLIMIT;
			}
		}
		else if(count==0){//end of file received
			endoffile=1;
			nequal=ccp->run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,-1);
			if(nequal==0)			comp_result=JS_CORRECT;
			else if(nequal>0)		comp_result=JS_WRONG;
			else					comp_result=JS_COMP_ERROR;
			//debugt("compare","stage=%d,nequal:%d",compare_stage,nequal);
		}else if(count<0){
			if(errno==EAGAIN){
				if(	!watcher_alive ){
					//actually in case the submission is killed or suicide, we won't need to compare at all, but let's do it.
					//last call to compare_output
					nequal=run_params_struct->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,-1);
					if(nequal==0)			comp_result=JS_CORRECT;
					else if(nequal>0)		comp_result=JS_WRONG;
					else					comp_result=JS_COMP_ERROR;
					//we're done then
					were_done=1;
					//debugt("compare","stage=%d,nequal:%d",compare_stage,nequal);
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
	}

	else{
		debugt("run_tpl","spawning failed before execvp()");
		result=JS_UNKNOWN;
	}

	//result
	run_params_struct->result=result;

	/*
	 * Close all opened file descriptors
	 * Free All Malloced memory areas
	 */

	free(ccp);

	return 0;




}

/**
 * jug_sandbox_child(), aka Watcher
 *
 *
 */
//reside@[main_process], run@[template_forked_process|watcher_process]
int jug_sandbox_child(void* arg){
	int fail=0;
	

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
		debugt("sandbox_child","ERFS mode off");
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
	debugt("watcher","/proc remounted succesfully");
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
	debugt("watcher", "binary_input_pipe (%d, %d)", binary_input_pipe[0], binary_input_pipe[1]);
	//link Binary input to the reading end of the pipe.
	fail=dup2(binary_input_pipe[PIPE_READFROM], STDIN_FILENO);
	debugt("watcher", "binary_input_pipe[0] is dup2 to %d", fail);


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
	//we will read from in_pipe[0] and write to binary_input_pipe[1].
	//we will close binary_input_pipe[1] when all the in-feed is consumed.
	/*
	//if the user provides a direct fd to read from, redirect the stdin to it
	if(ccp->run_params_struct->fd_datasource_dir==0){
		dup2(ccp->run_params_struct->fd_datasource,STDIN_FILENO);
		//but, if they provide a fd where they supposed to write the data, redirect stdin to the
		// receiving end of the input transferring pipe.
	}else{
		dup2(ccp->run_params_struct->in_pipe[0],STDIN_FILENO);
	}
	*/

	
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
				}else if(splice_size>0) {
					//debugt("watcher", "transferred to Binary : %d", (int)splice_size);
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
	debugt("binary","trying to execute : %s", ccp->binary_path);
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
	sb->count_submissions=0;
	sb->count_submissions_success=0;

	//check if cgroup filesystem is properly mounted
	//old code here
	//check the sandbox root env.
	struct stat st={0};
	error=stat("/tmp",&st);
	if(error){
		debugt("sandbox_init","chroot's /tmp unfound, jug_rootfs is not ready");
		return -10;
	}


	return 0;
}

//
// jug_sandbox_init_template_process (\\[main_process\main_thread])
//
int jug_sandbox_template_init(){
	template_pid=-1;
	int i;
	//io pipes with the template clone //FIXME: not good for dependancy (Sandbox shouldn't be dependant on Queue)
	int threads_max=queue_get_workers_count();
	if(threads_max>THREADS_MAX){
		debugt("template_init","number of workers must <24");
		return -5;
	}
	debugt("template_init","workers number=%d",threads_max);
	for(i=0;i<threads_max;i++){
		if(pipe(io_pipes_in[i])==-1 || pipe(io_pipes_out[i])==-1){
			debugt("js_template_init","cannot init io pipes pool");
			return -4;
		}

	}
	//check the pipes are OK


	//non blockin
	if(pipe(template_pipe_rx)==-1){
		debugt("js_template_init","cannot init template_pipe_tx: %s",strerror(errno));
		return -3;
	}
	//non-blocking read from _rx
	int flags=fcntl(template_pipe_rx[PIPE_READFROM],F_GETFL,0);
	fcntl(template_pipe_rx[PIPE_READFROM],F_SETFL,flags|O_NONBLOCK);
	//template process communication channel
	if(pipe(template_pipe_tx)==-1){
		debugt("js_template_init","cannot init template_pipe_rx: %s",strerror(errno));
		return -3;
	}


	//non blocking
//	 flags=fcntl(template_pipe_tx[PIPE_WRITETO],F_GETFL,0);
//		fcntl(template_pipe_tx[PIPE_WRITETO],F_SETFL,flags|O_NONBLOCK);


	//init mutex

	pthread_mutex_init(&template_pipe_mutex, NULL);

	//init Shared Memory
	key_t shmKey=2210;
	int shmid;
	char* shm;
	if( (shmid=shmget(shmKey,2028,IPC_CREAT|0666))<0 ){
		debugt("template_init","cannot create shared memory");
		return -1;
	}
	if( (shm=shmat(shmid, NULL, 0))==NULL){
		debugt("template_init","cannot attach shared memory");
		return -1;
	}
	strcpy(shm,"INIT");
	template_shm=shm;
	//clone
	char* tpl_stack;
	char *tpl_stacktop;
	int STACK_SIZE=2*1000000; //2MB is enough
	tpl_stack=(char*)malloc(STACK_SIZE);
	if(!tpl_stack){
		return -1;
	}
	tpl_stacktop=tpl_stack;
	tpl_stacktop+=STACK_SIZE;
	template_pid=clone(jug_sandbox_template,tpl_stacktop,SIGCHLD,NULL);
	if(template_pid==-1){
		debugt("js_template_init","cannot clone() the template process : %s",strerror(errno));
		return -2;
	}


	for(i=0;i<threads_max;i++){
		close(io_pipes_in[i][PIPE_READFROM]);
		close(io_pipes_out[i][PIPE_WRITETO]);
	}
	/*close(template_pipe_rx[PIPE_WRITETO]);*/
	close(template_pipe_tx[PIPE_READFROM]);
	close(template_pipe_rx[PIPE_WRITETO]);
	free(tpl_stack);
	debugt("js_template_init","template process launched, pid=%d",template_pid);
	return 0;
}

//
//jug_sandbox_template (\\[template_process])
//
int jug_sandbox_template(void*arg){
	signal(SIGUSR1,jug_sandbox_template_sighandler);
	close(template_pipe_rx[PIPE_READFROM]);
	close(template_pipe_tx[PIPE_WRITETO]);

	//in order to be able to transfer EOF to the receiving process, all the writing ends of
	// the 'stdin pipe' aka Input Transferring Pipe, should be closed.
	//EOF is only transmitted after all the writing fds are closed.
	for (int i=0; i<THREADS_MAX; i++){
		close(io_pipes_in[i][PIPE_WRITETO]);
		close(io_pipes_out[i][PIPE_READFROM]); //Template doesn't need to read from those
	}
	//shared mem
	key_t key=2210;
	int shmid;
	char* shm;
	if ( (shmid=shmget(key,2028,0666))<0){
		debugt("template","cannot get the shared memory");
		debugt("template","DOWN");
		return -1;
	}
	if( (shm=shmat(shmid,NULL,0))==NULL){
		debugt("template","cannot attach the shared memory");
		debugt("template","DOWN");
		return -1;
	}
	//the template being a separated process, ti should set the template_shm to the calculated value
	template_shm=shm;
	debugt("SHM","template read: %s", shm);
	shm[0]='N'; //NOT READY;
	//

	char buf[50];
	int up=1;
	int thispid=getpid();
	//event loop
	while(1){

		if(up)
			debugt("template","UP (%d)", thispid);
		else debugt("template","DOWN (%d)",thispid);
		template_shm[0]='S';
		//sleep a little
		sleep(8	);
	}
	return 0;
}



// jug_sandbox_template_sighandler (\\[template_process:sighandler])
void jug_sandbox_template_sighandler(int sig){

	debugt("tpl_sighandler","sig received : %d",sig);
	if(sig!=SIGUSR1) return;
	//
	char pipe_buf[50];
	pid_t cloned_pid=-1;
	int rd,i, _thread_order;
	int wr;

	unsigned char* tx_buf=(unsigned char*)malloc(TEMPLATE_MAX_TX*sizeof(unsigned char) );
	//shared mem management
	template_shm[0]='R';
	while(template_shm[0]!='K');
	//read args
	char* shmStart=template_shm+1;
	int readLen=*((int*)shmStart);
	memcpy(tx_buf, shmStart+sizeof(int), readLen);


	//unserialize
	struct clone_child_params* ccp=jug_sandbox_template_unserialize(tx_buf);
	free(tx_buf);


	//
	///////testunit

	//experiment: route the in/out pipes through already threading established pipes
	//	_thread_order=ccp->run_params_struct->thread_order;
	//	for(i=0;i<2;i++){
	//		ccp->run_params_struct->out_pipe[i]=io_pipes_out[_thread_order][i];
	//		ccp->run_params_struct->in_pipe[i]=io_pipes_in[_thread_order][i];
	//	}

	//

	//TEST-UNIT


	//clone preparation
	char* cloned_stack;
	char *cloned_stacktop;
	int STACK_SIZE=10*1000000; //10MB
	cloned_stack=(char*)malloc(STACK_SIZE);
	if(!cloned_stack){
		debugt("js_tpl_sighandler","cannot malloc the stack for clone");
		sprintf(pipe_buf,"%d",-1);
		write(template_pipe_rx[PIPE_WRITETO],pipe_buf,strlen(pipe_buf)+1);
		return ;
	}
	cloned_stacktop=cloned_stack;
	cloned_stacktop+=STACK_SIZE;

	int JUG_CLONE_SANDBOX=CLONE_NEWUTS|CLONE_NEWNET|CLONE_NEWPID|CLONE_NEWNS|CLONE_NEWUSER;
	//CLONE TO THE WATCHER
	cloned_pid=clone(jug_sandbox_child,cloned_stacktop,JUG_CLONE_SANDBOX|CLONE_PARENT|SIGCHLD,(void*)ccp);
	///////////

	//free first
	free(cloned_stack);
	jug_sandbox_template_freeccp(ccp);

	//write
	int pidReturn;
	if(cloned_pid==-1) pidReturn=-2;
	else pidReturn=cloned_pid;
	int len=sizeof(int);
	memcpy(shmStart,&len,sizeof(int));
	memcpy(shmStart+sizeof(int),&pidReturn,len);
	template_shm[0]='R';
	//work's end
	while(template_shm[0]!='K'); //thread finished its work
	template_shm[0]='R'; //ready for the next request
	//exit without cloning
	return;
	//
	//
//	if(cloned_pid==-1){
//		debugt("js_tpl_sighandler","cannot clone: %s",strerror(errno));
//		sprintf(pipe_buf,"%d",-2);
//		write(template_pipe_rx[PIPE_WRITETO],pipe_buf,strlen(pipe_buf)+1);
//	}else{
//		debugt("CLONE","cloned, pid=%d",cloned_pid);
//		sprintf(pipe_buf,"%d",(int)cloned_pid);
//		write(template_pipe_rx[PIPE_WRITETO],pipe_buf,strlen(pipe_buf)+1);
//	}
//
//
//	return;

}

// jug_sandbox_template_clone (\\[main_process/some_thread])
pid_t jug_sandbox_template_clone(void*arg, int len){
	debugt("testunit","template_clone");
	if(template_pid<0){
		debugt("tpl_clone","template_pid<0");
		return (pid_t)-1;
	}
	//decl
	char pipe_buf[500];
	int is_read=1,wr=0;
	struct timeval pipe_read_timeout;
	fd_set _set;
	int slct;
	pid_t pid;
	//
	pipe_read_timeout.tv_sec=20;
	pipe_read_timeout.tv_usec=0;
	FD_ZERO(&_set);
	//FD_SET(template_pipe_rx[PIPE_READFROM],&_set);

	pthread_mutex_lock(&template_pipe_mutex);
	global_sandbox->count_submissions++;
	while(template_shm[0]!='S');
	debugt("SHM-T","Sync START received");
	//SEND KILL
	if(kill(template_pid,SIGUSR1)==-1){
		debugt("tpl_clone","cannot deliver SIGUSR1 signal");
		pthread_mutex_unlock(&template_pipe_mutex);
		return (pid_t)-1;
	}
	//SEND SUBMISSION DATA
	while(template_shm[0]!='R');
	char* shmStart=template_shm+1;
	memcpy(shmStart,&len,sizeof(int));
	memcpy(shmStart+sizeof(int),arg,len);
	template_shm[0]='K';
	//READ WATCHER PID
	while(template_shm[0]!='R');
	int readLen=*((int*)shmStart);
	char* readData=(char*)malloc(readLen*sizeof(char)+1);
	memcpy(readData, shmStart+sizeof(int), readLen);
	int readPid=*( (int*)readData);
	free(readData);
	//thread: Im ok
	template_shm[0]='K';
	global_sandbox->count_submissions_success++;
	pthread_mutex_unlock(&template_pipe_mutex);

	return (pid_t)readPid;
//	return (pid_t)88989;
	//TESTUNIT

}

//jug_sandbox_template_term //[main_process/any_thread]
void jug_sandbox_template_term() {
	if(template_pid>0)
		kill(template_pid,SIGKILL);
	template_pid=-1;
}

//jug_sandbox_template_serialize
int jug_sandbox_template_serialize(
		void** p_serial,
		struct clone_child_params* ccp){
	//run_params|sandbox|char[]binary_path|argc|argv[0]|argv[1]|....|'\0'
	int i,sz,index,argc;
	//calc size
	sz=sizeof(struct run_params)+sizeof(struct sandbox)+(strlen(ccp->binary_path)+1);
	i=0;argc=0;

	while(ccp->argv[i]!=NULL){
		argc+=1;
		sz+=strlen(ccp->argv[i])+1;
		i++;
	}

	sz+=sizeof(int); //for argc
	//malloc

	void* serial=malloc(sz);
	*p_serial=serial;

	//fill
	//run_params_struct
	index=0;
	memcpy(serial+index,ccp->run_params_struct,sizeof(struct run_params));
	index+=sizeof(struct run_params);
	//sandbox
	memcpy(serial+index,ccp->sandbox_struct,sizeof(struct sandbox));
	index+=sizeof(struct sandbox);
	//binary_path
	memcpy(serial+index,ccp->binary_path,strlen(ccp->binary_path)+1);
	index+=strlen(ccp->binary_path)+1;
	//argc
	memcpy(serial+index,&argc,sizeof(int));
	index+=sizeof(int);
	//argv
	i=0;
	while(ccp->argv[i]!=NULL){
		memcpy(serial+index,ccp->argv[i],strlen(ccp->argv[i])+1);
		index+=strlen(ccp->argv[i])+1;
		i++;
	}

	//
	return sz;

}

//unserilize
struct clone_child_params* jug_sandbox_template_unserialize(void*serial){
	//run_params|sandbox|char[]binary_path|argc|argv[0]|argv[1]|....|'\0'
	int index,i,argc;
	struct clone_child_params* ccp;
	ccp=(struct clone_child_params*)malloc(sizeof(struct clone_child_params));



	index=0;
	//run_params
	ccp->run_params_struct=(struct run_params*)malloc(sizeof(struct run_params));
	memcpy(ccp->run_params_struct,serial+index,sizeof(struct run_params));
	index+=sizeof(struct run_params);



	//sandbox
	ccp->sandbox_struct=(struct sandbox*)malloc(sizeof(struct sandbox));
	memcpy(ccp->sandbox_struct, serial+index,sizeof(struct sandbox));
	index+=sizeof(struct sandbox);
	//binar_path
	i=index;
	while(*(char*)(serial+i)!='\0') i++;
	ccp->binary_path=(char*)malloc((i-index+1)*sizeof(char));
	memcpy(ccp->binary_path,serial+index,(i-index+1));
	index=i+1;
	//argc
	argc=0;
	memcpy(&argc,serial+index,sizeof(int));
	index+=sizeof(int);
	//argv
	ccp->argv=(char**)malloc( (argc+1)*sizeof(char*));
	for(i=0;i<argc;i++){
		ccp->argv[i]=(char*)malloc( strlen(serial+index)+1 );
		memcpy(ccp->argv[i],serial+index,strlen(serial+index)+1);
		index+=strlen(serial+index)+1;
	}
	ccp->argv[i]=NULL;
	//
	return ccp;

}


//free clone_child_params //[template_process]
void jug_sandbox_template_freeccp(struct clone_child_params* ccp){
	free(ccp->run_params_struct);
	free(ccp->sandbox_struct);
	free(ccp->binary_path);
	int i=-1;
	while(ccp->argv[++i]!=NULL) free(ccp->argv[i]);
	free(ccp->argv);
	free(ccp);
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
		debugt("watcher_sig_handler","binary process %d (inside pid_ns) has been succesfully killed",binary_pid);
	}

	//SIGUSR1: sent from the parent, wrong output detectedn or pipe error
	else if(sig==SIGUSR1){
		debugt("watcher_sig_handler","SIGUSR1 received, wrong output detected");
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






uint jug_hash(char* key, int len){
	uint hash = 5381;
	uint M=5381;
	uint i;
	for( i = 0; i < len; ++i)
		hash = M * hash + key[i];
	return hash % 999999;
}






