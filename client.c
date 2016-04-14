/**
 * client.c
 * an experimental client for kudos daemon.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "interface.h"

int main(int argc , char *argv[])
{
	int sock;
	struct sockaddr_in server;
	char message[1000] , server_reply[2000];

	//Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1)
	{
		printf("KudosClient: Could not create socket\n");
		exit(5);
	}
	puts("KudosClient: Socket created");

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( 22101 );

	//Connect to remote server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("KudosClient: connect failed. Error");
		return 1;
	}

	puts("KudosClient: Connected\n");

	//keep communicating with server
	int rcv_count=0;
	int_request request;
	int_response response;
	//one time request
	//printf("KudosClient: enter path to execute : ");
	//scanf("%s" , request.path);
	strcpy(request.path,"/opt/twins");
	strcpy(request.echo, request.path);
	strcpy(request.tc_in_path,"/home/odev/jug/tests/problems/twins/twins.in");
	strcpy(request.tc_out_path,"/home/odev/jug/tests/problems/twins/twins.out");
	//using sourcecode causes trouble, the size is very long and the network reading is not ready for that
//	strcpy(request.sourcecode, "#include <stdio.h>\n int main(){ printf(\"Hello World\"); return 0;}");
	//so instead we use a name for a file that contains the source just for debugging purposes
	request.sourcecode[0]='\0'; //mandatory
	strcpy(request.sourcecode_path,"/home/odev/jug/tests/problems/twins/twins.c");

	request.id=999;

	//Send some data
	if( send(sock , &request , sizeof(int_request) , 0) < 0)
	{
		puts("KudosClient: Send failed");
		return 1;
	}
//
//	//Receive the ack
//	if( (rcv_count=recv(sock , &response , sizeof(int_response) , 0)) < 0)
//	{
//		puts("KudosClient: recv failed");
//		return 50;
//	}
//	//repsonse retreived
//	printf("echo :%s\n",response.echo);

	//
	struct timeval tv;
	tv.tv_sec = 6;  /* 30 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
	//receive the verdict
	if( ( rcv_count=recv(sock,&response, sizeof(int_response),0 ))<0){
		puts("KudosClient: recv failed");
		return 54;
	}
	printf("verdict: %s\nrcv_count=%d, strlen_resp=%d\n", response.verdict_s,rcv_count,sizeof(int_response));

	close(sock);
	return 0;
}
