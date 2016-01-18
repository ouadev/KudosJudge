/*
 * kudosd : Kudos Judge Daemon.
 * main entry of the judge.
 */
//TODO: implement cleaning-up function for each of components, this functions to be called when exiting from the daemon
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#include "log.h"
typedef enum {DAEMON_ACTION_START,DAEMON_ACTION_STOP,DAEMON_ACTION_RESTART,DAEMON_ACTION_STATE} daemon_action_enum;

int kjd_daemonize();
void kjd_sighandler(int sig);
void kjd_exit(int status, char* syslog_msg);
void kjd_log(char* msg,...);

int main(int argc, char* argv[]){
	char* usage_str="kudosd [start|stop|restart|state]";
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
	//parse arguments
	printf("KudosJudge 0.1\n");
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
		else{
			printf("%s : Unknown action\n",action_str);
			exit(0);
		}
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
	printf("started\n");
	printf("use\033[1;30m $./kudosd state\033[0m to check if the daemon is running \n");
	/**
	 *
	 * start the daemon
	 */
	//open syslog
	openlog("kudosd", LOG_PID|LOG_CONS, LOG_USER);
	//daemonize
	error=kjd_daemonize();

	//setup tcp server

	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		kjd_log("Could not create socket");
	}
	kjd_log("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 22101 );

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		kjd_log("bind failed. Error");
		return 1;
	}
	kjd_log("bind done");

	kjd_log("KudosJudge Daemon Up ...");
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
	//looop
	while(1){
		//Listen
		listen(socket_desc , 3);

		//Accept and incoming connection
		kjd_log("Waiting for incoming connections...");
		c = sizeof(struct sockaddr_in);

		//accept connection from an incoming client
		client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
		if (client_sock < 0)
		{
			kjd_log("accept failed");
			return 1;
		}
		kjd_log("Connection accepted");

		//Receive a message from client
		while( (read_size = recv(client_sock , client_message , 2000 , 0)) > 0 )
		{

			//Send the message back to client
			write(client_sock , client_message , read_size);
		}

		if(read_size == 0)
		{
			kjd_log("Client disconnected");
			fflush(stdout);
		}
		else if(read_size == -1)
		{
			kjd_log("recv failed");
		}
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
	signal(SIGCHLD, SIG_IGN);
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
		//request to shutdown the daemon
		//here should implement the cleaning process
		//clean the ramfs, kill the template process, remove all tmp files, and exit
		unlink("/tmp/kudosd-pid");
		//exit
		kjd_log("SIGINT received,  T'hlla :) ");
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

/**
 * void kjd_log(char* msg)
 */
void kjd_log( char* message,...){

	char* dbg_msg=(char*)malloc(strlen(message)+120);
	va_list args;
	va_start(args,message);
	vsprintf(dbg_msg,message,args);
	va_end(args);
	//

	syslog(LOG_INFO, "%s\n",dbg_msg);
}
