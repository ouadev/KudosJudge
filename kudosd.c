/*
 * kudosd : Kudos Judge Daemon.
 * main entry of the judge.
 */
//TODO: implement cleaning-up function for each of components, this functions to be called when exiting from the daemon
//TODO: do the same for restart command. must do a proper stop
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include<sys/socket.h>
#include<arpa/inet.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#include "log.h"
#include "interface.h"
#include "queue.h"
#include "protocol.h"
#include "sandbox.h"
#include "ramfs.h"
#include "lang.h"



typedef enum {DAEMON_ACTION_START,DAEMON_ACTION_STOP,DAEMON_ACTION_RESTART,DAEMON_ACTION_STATE} daemon_action_enum;

int kjd_daemonize();
void kjd_sighandler(int sig);
void kjd_exit(int status, char* syslog_msg);


int main(int argc, char* argv[]){
	char* usage_str="kudosd [start|stop|restart|state|-v]";
	int error;
	int socket_desc , client_sock , c , read_size;
	struct sockaddr_in server , client;
	char client_message[2000];
	FILE* tmpfile_pid=NULL;
	FILE* tmpfile_pid_new=NULL;
	char action_str[50], tmp_path[50];
	daemon_action_enum action;
	pid_t thispid, daemon_pid;
	struct stat tmp_stat;
	struct stat daemon_proc_stat;
	int tmpfile_exists=0,daemon_running=0, tmp_read=0;
	char tmppid_str[10], proc_daemon[50];
	int complete_to_daemon=0;
	//set needed environment variable
	error=setenv(  "JUG_ROOT" , "/opt/kudosJudge", 0);
	if(error<0){
		printf("Cannot set the environment variable JUG_ROOT");
		return -52;
	}
	//parse arguments
	if(argc==1){
		printf("Usage : %s\n",usage_str);
		exit(0);
	}else{
		strcpy(action_str,argv[1]);
		if(strcmp(action_str,"start")==0)
			action= DAEMON_ACTION_START;
		else if(strcmp(action_str,"restart")==0)
			action= DAEMON_ACTION_RESTART;
		else if(strcmp(action_str,"stop")==0)
			action= DAEMON_ACTION_STOP;
		else if(strcmp(action_str,"state")==0)
			action=DAEMON_ACTION_STATE;
		else if(strcmp(action_str,"-v")==0){
			printf("KudosJudge 0.1\n");
			exit(0);
		}
		else{
			printf("%s : Unknown action\n",action_str);
			exit(0);
		}
	}
	//check if run as root
	if( (action==DAEMON_ACTION_START || action==DAEMON_ACTION_STOP ||
			action== DAEMON_ACTION_RESTART) && geteuid() != 0)
	{
		printf("You may need to run kudosJudge as root (sudo)\n");
	}
	//execute action
	//gather information about daemon state
	sprintf(tmp_path,"/tmp/kudosd-pid");
	error=stat(tmp_path,&tmp_stat);
	if(error==-1){
		if(errno==ENOENT)
			tmpfile_exists=0;
		else{
			printf("Internal Error : 1\n");
			exit(1);
		}
	}else{
		tmpfile_exists=1;
	}
	//
	if(tmpfile_exists){
		tmpfile_pid=fopen(tmp_path,"r");
		if(tmpfile_pid==NULL){
			printf("Internal Error : 2\n");
			exit(2);
		}
		if( (tmp_read=fread(tmppid_str,1,50,tmpfile_pid) )==-1){
			printf("Internal Error : 3\n");
			exit(3);
		}
		tmppid_str[10]='\0';
		daemon_pid=(pid_t)atoi(tmppid_str);
		//check this pid
		sprintf(proc_daemon,"/proc/%d",daemon_pid);
		error=stat(proc_daemon,&daemon_proc_stat);
		if(error==-1){
			if (errno!=ENOENT){
				printf("Internal Error : 5\n");
				exit(5);
			}
			daemon_running=0;
		}else{
			//daemon is running.
			daemon_running=1;
		}
		fclose(tmpfile_pid);
	}else{
		//if tmpfile is unfound, we suppose the daemon is not up
		daemon_running=0;
	}

	//action?
	if(action==DAEMON_ACTION_STATE){
		if(daemon_running)
			printf("the KudosJudge Daemon is\033[0;32m running\033[0m (pid=%d)\n",daemon_pid);
		else
			printf("the KudosJudge Daemon is\033[0;31m not running\n");
		printf("\033[0m");
		exit(0);
	}
	else if(action==DAEMON_ACTION_START){
		if(daemon_running){
			printf("the daemon is already running : pid=%d\n",daemon_pid);
			exit(0);
		}else{
			if(tmpfile_exists && unlink(tmp_path)==-1){
				//remove the tmp file
				printf("Internal Error : 4\n");
				exit(4);

			}
			//daemon is not running then start it
			complete_to_daemon=1;
		}
	}else if(action==DAEMON_ACTION_STOP){
		if(!daemon_running){
			printf("the daemon isn't running\n");
			exit(0);
		}
		//daemon running, send to it a signal to stop.
		if(kill(daemon_pid,SIGINT)==-1){
			printf("error during shutdown operation, error %d",6);
			exit(6);
		}
		if( unlink(tmp_path)==-1){
			//remove the tmp file
			printf("Internal Error : 7\n");
			exit(7);

		}
		printf("the daemon (pid=%d) is stopped\n", daemon_pid);
		exit(0);
	}else if(action==DAEMON_ACTION_RESTART){
		//shutdown
		if(!daemon_running){
			printf("the daemon isn't running\n");
		}else{
			printf("stopping...\n");
			if(kill(daemon_pid,SIGINT)==-1){
				printf("error during shutdown operation, error %d",8);
				exit(8);
			}
			if( unlink(tmp_path)==-1){
				//remove the tmp file
				printf("Internal Error : 9\n");
				exit(9);
			}
		}
		//start
		complete_to_daemon=1;
	}


	printf("starting ...\n");
	printf("\033[0;32mstarted\033[0m\n");
	printf("use\033[1;30m $./kudosd state\033[0m to check if the daemon is running \n");
	/**
	 *
	 * start the daemon
	 */
	//open syslog
	openlog("kudosd", LOG_PID|LOG_CONS, LOG_USER);
	//daemonize
	error=kjd_daemonize();
	//redirect stderr debug to
	char stderr_tmpfile[40];
	char stdout_tmpfile[40];
	sprintf(stderr_tmpfile,"/tmp/kudosd-stderr-%d",getpid());
	sprintf(stdout_tmpfile,"/tmp/kudosd-stdout-%d",getpid());
	freopen(stderr_tmpfile,"w+",stderr);
	//freopen(stdout_tmpfile,"w+",stdout);

	//////////////////////////
	//// init ramfs /////////
	////////////////////////
	//get env
	char* jug_root;
	char* ramfs_root= (char*)malloc(sizeof(char)* 400);
	if (  (jug_root=getenv("JUG_ROOT"))==NULL){
		kjd_log("JUG_ROOT environment variable is not found \n");
		return -558;
	}
	//check if the /var/opt/kudosdJudge exists
	struct stat varOptKudosdJudge;
	if( stat("/var/opt/kudosJudge", &varOptKudosdJudge)!=0 ||
			!S_ISDIR(varOptKudosdJudge.st_mode)){
		kjd_log("/var/opt/kudosJudge don't exist : creating it ...");
		mode_t oldUmask=umask(022);
		error=mkdir("/var/opt/kudosJudge",0777);
		umask(oldUmask);
		if(error !=0){
			kjd_log("failed creating /var/opt/kudosJudge : %d", strerror(errno));
			return -558;
		}
	}
	//put ramfs inside /var/opt/kudosJudge
	ramfs_info* ramfsinfo=init_ramfs("/var/opt/kudosJudge","ramfs",10,15);
	error=create_ramfs(ramfsinfo);
	if(error && error!=-2){//-2: already exists, good
		kjd_log("Ramfs directory cannot be initialized : %s/%s",jug_root,"ramfs");
		return -559;
	}else if(error==-2){
		kjd_log("Ramfs is already mounted : %s", ramfsinfo->path);
	}else{
		kjd_log("Ramfs is mounted : %s", ramfsinfo->path);
	}
	set_global_ramfs(ramfsinfo);
	///////////////////
	//init queue /////
	/////////////////
	error=queue_start();
	if(error){
		kjd_log("Queue not started,  error %d",error);
		return -100;
	}
	kjd_log("Queue and workers are ready");
	////////////////////////
	///////init sandbox////
	//////////////////////
	error=jug_sandbox_start();
	if(error<0){
		kjd_log("Sandbox not started");
		return -556;
	}
	kjd_log("Sandbox started");
	///////////////////////////
	///// init languages /////
	/////////////////////////
	error=lang_init();
	if(error!=0){
		kjd_log("Problem initializing languages");
		return -557;
	}
	kjd_log("Languages are ready");
	//lang_print();

	/////////////////////
	//setup tcp server//
	//////////////////
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		kjd_log("Could not create socket");
	}


	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 22101 );

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		kjd_log("bind failed. Error: %s",strerror(errno));
		return 1;
	}



	//here create a tmpfile_new
	tmpfile_pid_new=fopen(tmp_path,"w+");
	if(tmpfile_pid_new==NULL){
		kjd_log("cannot create tmp file, exiting...");
		return 2;
	}
	sprintf(tmppid_str,"%d",(int)getpid() );
	error=fwrite(tmppid_str,1,strlen(tmppid_str)+1,tmpfile_pid_new);
	if(error< (strlen(tmppid_str)+1) ){
		kjd_log("cannot write daemon pid to the tmp file, exiting...");
		return 3;
	}
	fclose(tmpfile_pid_new);
	////////////////////
	///////looop////////
	///////////////////
	int_request request;
	int_response response;
	int n_got=0;
	while(1){
		//Listen
		listen(socket_desc , 3);

		//Accept and incoming connection
		kjd_log("Listening ....");
		c = sizeof(struct sockaddr_in);

		//accept connection from an incoming client
		client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
		if (client_sock < 0)
		{
			kjd_log("accept failed");
			return 1;
		}
		kjd_log("Connection accepted");

		//push the handle in the queue
		jug_connection* new_connection=(jug_connection*)malloc(sizeof(jug_connection));
		new_connection->client_socket=client_sock;

		queue_push_connection(new_connection);
		kjd_log("Client_sock inserted in Queue");

	}
	//main loop
	kjd_log("KudosJudge shutdown ....");


	//clean up
	closelog();
	return 0;

}

/**
 * daemonize()
 */

int kjd_daemonize(){
	pid_t pid;
	pid=fork();
	if(pid<0){
		return -1;
	}
	if(pid>0){
		exit(0);
	}
	//the child
	if(setsid()<0){
		kjd_log("cannot create the session");
		exit(-1);
	}
	//signal stuff
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, kjd_sighandler); //used to shutdown the daemon

	//for second time to prevent from getting a controlling terminal
	pid=fork();
	if(pid<0){
		kjd_log("daemonize: cannot fork second time");
		exit(-2);
	}
	if(pid>0)
		exit(0);
	//the final working child
	umask(0);
	chdir("/");
	//close all open files but in/out/err
	int fd;
	for(fd=sysconf(_SC_OPEN_MAX);fd>2;fd--){
		close(fd);
	}


	return 0;
}

/**
 * kjd_sighandler()
 * should clean before exiting.
 */

void kjd_sighandler(int sig){
	if(sig==SIGINT){
		kjd_log("SIGINT received, exiting ... ");
		//clean the ramfs, kill the template process, remove all tmp files, and exit
		//kill threads.
		kjd_log("stopping Workers...");
		if(queue_stop_workers()){
			kjd_log("failed to stop Queue's workers");
			return;
		}
		//stopping sandbox (template process)
		kjd_log("stopping Sandbox");
		if(jug_sandbox_stop()){
			kjd_log("failed to stop Sandbox");
			return ;
		}
		//remove tmp file
		unlink("/tmp/kudosd-pid");
		//exi
		exit(0);

	}
}
/**
 * void kjd_exit()
 */
void kjd_exit(int status,char* syslog_msg){
	syslog(LOG_INFO, "%s",syslog_msg);
	exit(status);
}


