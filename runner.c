#include "runner.h"





/**
 * jug_sandbox_start
 */
int jug_runner_start(int num_workers){
	int ret;
	//set number of workers
	g_runner_number_workers= num_workers;

	//start the runner 
	ret=jug_runner_init();
	if(ret){
		debugt("runner_start","cannot init the Runner");
		return -1;
	}

	//init sandbox
	ret=jug_sandbox_init();
	if(ret){
		debugt("runner_start","cannot init the sandbox");
		return -1;
	}
	//init template
	ret=jug_runner_template_init();
	if(ret!=0){
		debugt("runner_start","cannot init template process");
		return -2;
	}
	//give them time to get ready
	sleep(1.5);

	return 0;
}


/**
 * jug_sanbdox_stop
 */
int jug_runner_stop(){
	jug_runner_template_term();
	return 0;
}


int jug_runner_number_workers(){
	return g_runner_number_workers;
}
/**
* jug_run_init
*/
int jug_runner_init(){
	//fill the sandbox structure
	g_runner=NULL;

	int error;
	Runner* runner=(Runner*)malloc(sizeof(Runner));
	char* value;
	
	//stack size
	if( ( value=jug_get_config("Executer","stack_size_mb") )==NULL ){
		debugt("sandbox","cannot read time_limit_ms config");
		free(runner);
		return -1;
	}
	runner->stack_size_mb=atoi(value);
	
	//kill on wrong
	if( (value=jug_get_config("Executer","kill_on_compout") )==NULL ){
		debugt("sandbox","cannot read kill_on_compout config");
		free(runner);
		return -1;
	}
	runner->kill_on_compout= atoi(value);
	//show submission's output on the screen
	//show_submission_output
	if( (value=jug_get_config("Executer","show_submission_output") )==NULL ){
		debugt("sandbox","cannot read show_submission_output config");
		free(runner);
		return -1;
	}
	runner->show_submission_output= atoi(value);
	//output size limit
	//output_size_limit_mb_default
	if( ( value=jug_get_config("Executer","output_size_limit_mb") )==NULL ){
		debugt("sandbox","cannot read output_size_limit_mb config");
		return -1;
	}
	runner->output_size_limit_mb_default=atoi(value);
	//fill sandbox's state
	runner->nbr_instances=0;
	runner->count_submissions=0;
	runner->count_submissions_success=0;
	//set comparing function
	runner->compare_output=compare_output;
	//set global Runnner
	g_runner=runner;

	return 0;

}


/**
 * entry function, may changed in the future after add compiling layer
 * (code for experiment only, many hardcoded stuff)
 */
jug_verdict_enum jug_runner_judge(jug_submission* submission){


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
	ret=jug_runner_run(&runp,args[0],args,submission->thread_id);
	debugt("judge","ALL:%d,SUCC:%d", g_runner->count_submissions, g_runner->count_submissions_success);
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
int jug_runner_run(
		struct run_params* run_params_struct,
		char* binary_path,
		char*argv[],
		int thread_order){
	//locals
	int i,fail,fd_datasource_flags,fd_datasource_status;
	int watcher_pid;
	int received_bytes=0;
	int compare_stage=0;
	//get sandbox
	struct sandbox* sandbox_struct=jug_sandbox_global();
	//some checking
	//if the caller didn't want to specify some limits, replace them with the Config's defaults.
	if(run_params_struct->mem_limit_mb<0)
		run_params_struct->mem_limit_mb=sandbox_struct->mem_limit_mb_default;
	if(run_params_struct->time_limit_ms<0)
		run_params_struct->time_limit_ms=sandbox_struct->time_limit_ms_default;

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
	int serial_len=jug_runner_template_serialize(&serial,ccp);
	//clone request
	//ASK TEMPLATE TO CLONE
	watcher_pid=jug_runner_template_clone(serial,serial_len);
	////////
	run_params_struct->watcher_pid=watcher_pid;
	//free memory
	free(serial);
	if(watcher_pid==-1){
		debugt("run_tpl","cannot clone() the template");
		return -2;
	}


	/*
	 *
	 * read the output from the submission (Jug_sandbox_run_tpl)
	 *
	 */

	pid_t wpid;
	int count=0,endoffile=0,compout_detected=0,iw_status,nequal,spawn_succeeded=1;
	int watcher_alive=1,were_done=0,flags;;
	char buffer[256];
	short cnfg_kill_on_compout= g_runner->kill_on_compout;
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
					kill(watcher_pid,SIGUSR2);
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
		
		count=read(run_params_struct->out_pipe[PIPE_READFROM],buffer,255);

		if( count>=0)
			debugt("run_tpl","order=%d, read count=%d",run_params_struct->thread_order,count);

		if( g_runner->show_submission_output && count!=-1 ){
			//write(STDOUT_FILENO,"~~~~~~~~~\n",10);
			//write(STDOUT_FILENO,buffer,count);
			buffer[count]='\0';
			debugt("stdout", "%s", buffer);
			//write(STDOUT_FILENO,"~~~~~~~~~\n",10);
		}
		if(count>0){ //some bytes are read
			nequal= g_runner->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,compare_stage);
			if(nequal==0)		comp_result=JS_CORRECT;
			else if(nequal>0)	comp_result=JS_WRONG;
			else				comp_result=JS_COMP_ERROR;
			compare_stage++;

			
			//
			//debugt("compare","stage=%d,nequal:%d",compare_stage,nequal);
			//if an inequality is reported by the comparing function, and simultaneous mode is on, kill.
			if(nequal != 0){
				compout_detected=1;
				debugt("runner.run","sending compout kill signal to watcher");
				//communicate with the child via SIGUSR1 signal to inform them we detected false output
				if(cnfg_kill_on_compout){
					fail=kill(watcher_pid,SIGUSR2);
					if(fail==-1){
						debugt("runner.run","sending compout signal to watcher FAILED");
					}
				}
				
			}
			//detect too much of output (not test feature)
			//SIGUSR1 handler will kill the binary and make sure the watcher reports a good execution
			// with exit(JS_CORRECT). to report the output overflow to sandbox user, we will change the
			// value of comp_result flag to JS_OUTPUTLIMIT
			received_bytes+=count;
			if( (received_bytes/1000000)> g_runner->output_size_limit_mb_default){
				debugt("rn_tpl","too much generated output");
				kill(watcher_pid,SIGUSR2);
				comp_result=JS_OUTPUTLIMIT;
			}
		}
		else if(count==0){//end of file received
			endoffile=1;
			nequal= g_runner->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,-1);
			if(nequal==0)			comp_result=JS_CORRECT;
			else if(nequal>0)		comp_result=JS_WRONG;
			else					comp_result=JS_COMP_ERROR;
			//debugt("compare","stage=%d,nequal:%d",compare_stage,nequal);

		}else if(count<0){
			if(errno==EAGAIN){
				if(	!watcher_alive ){
					//actually in case the submission is killed or suicide, we won't need to compare at all, but let's do it.
					//last call to compare_output
					nequal= g_runner->compare_output(ccp->run_params_struct->fd_output_ref,buffer,count,-1);
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
					kill(watcher_pid,SIGUSR2);
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
	//flush out pipe
	debugt("Runner", "(%d) Total transferred to Watcher : %d", run_params_struct->thread_order, ds_sent_acc);
	
	//there are more than one writer, and we cannot close them all (the template one). and so the read will 
	// block forever. what we will do, since the binary is closed, is to just read for a while
	int blocking_count=0;
	int flush_out_acc=0;
	while ( (count=read(run_params_struct->out_pipe[PIPE_READFROM],buffer,255)) != 0   ){
		if(count==-1 && errno!=EAGAIN){
			debugt("Runner", "error emptying pipe_out %s", strerror(errno));
			run_params_struct->result=JS_UNKNOWN;
			return 0;
		}

		if(count<0 && errno==EAGAIN ) blocking_count++;
		if(count >=0){blocking_count=0; flush_out_acc+=count;}
		if(blocking_count>10000) break;
	};
	debugt("Runner", "(%d) out_pipe is supposed flushed : %d", run_params_struct->thread_order, flush_out_acc);
	
	// debugt("runner", "out_pipe flushed ");
	//write the rest of data to in_pipe
	int flush_acc=0;
	int flush_written=0;
	while(1){
		flush_written=sendfile(
					run_params_struct->in_pipe[PIPE_WRITETO],
					run_params_struct->fd_datasource,
					NULL,_datasource_transfer_size
			);
		if(flush_written>=0){
			flush_acc += flush_written;
		}
		if(flush_written<0 && errno != EAGAIN){
			debugt("Runner", "error writing the rest of data to Watcher, %s", strerror(errno));
			run_params_struct->result= JS_UNKNOWN;
			return 0;
		}
		if( flush_acc + ds_sent_acc >= run_params_struct->datasource_size ){
			break;
		}
	}
	debugt("Runner", "(%d) Total thrown to Watcher %d", run_params_struct->thread_order, flush_acc);
	
	free(ccp);

	return 0;




}


// jug_sandbox_init_template_process (\\[main_process\main_thread])
//
int jug_runner_template_init(){
	template_pid=-1;
	int error;
	int i;
	//io pipes with the template clone
	int threads_max=jug_runner_number_workers();
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

	error=pthread_mutex_init(&template_pipe_mutex, NULL);
	if(error!=0){
		debugt("template_init", "cannot init Template mutex, %s", strerror(errno));
		return -3;
	}

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
	template_pid=clone(jug_runner_template,tpl_stacktop,SIGCHLD,NULL);
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
int jug_runner_template(void*arg){
	signal(SIGUSR1,jug_runner_template_sighandler);
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
void jug_runner_template_sighandler(int sig){

	//debugt("tpl_sighandler","sig received : %d",sig);
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
	struct clone_child_params* ccp=jug_runner_template_unserialize(tx_buf);
	free(tx_buf);



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
	cloned_pid=clone(jug_sandbox_watcher,cloned_stacktop,JUG_CLONE_SANDBOX|CLONE_PARENT|SIGCHLD,(void*)ccp);
	///////////

	//free first
	free(cloned_stack);
	jug_runner_template_freeccp(ccp);

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


}

// jug_sandbox_template_clone (\\[main_process/some_thread])
pid_t jug_runner_template_clone(void*arg, int len){
	
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
	int error;
	//
	pipe_read_timeout.tv_sec=20;
	pipe_read_timeout.tv_usec=0;
	FD_ZERO(&_set);
	//FD_SET(template_pipe_rx[PIPE_READFROM],&_set);
	pthread_mutex_lock(&template_pipe_mutex);

	g_runner->count_submissions++;

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
	g_runner->count_submissions_success++;
	pthread_mutex_unlock(&template_pipe_mutex);
	return (pid_t)readPid;
//	return (pid_t)88989;
	//TESTUNIT

}

//jug_sandbox_template_term //[main_process/any_thread]
void jug_runner_template_term() {
	if(template_pid>0)
		kill(template_pid,SIGKILL);
	template_pid=-1;
}

//jug_sandbox_template_serialize
int jug_runner_template_serialize(
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
struct clone_child_params* jug_runner_template_unserialize(void*serial){
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
void jug_runner_template_freeccp(struct clone_child_params* ccp){
	free(ccp->run_params_struct);
	free(ccp->sandbox_struct);
	free(ccp->binary_path);
	int i=-1;
	while(ccp->argv[++i]!=NULL) free(ccp->argv[i]);
	free(ccp->argv);
	free(ccp);
}



void jug_runner_free_submission(jug_submission* submission){
	free(submission->input_filename);
	free(submission->output_filename);
	free(submission->source);
	free(submission->bin_path);
	free(submission->bin_cmd);
}