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

int main(int argc , char *argv[])
{
	int sock;
	struct sockaddr_in server;
	char message[1000] , server_reply[2000];

	//Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1)
	{
		printf("Could not create socket\n");
		exit(5);
	}
	puts("Socket created");

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( 22101 );

	//Connect to remote server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}

	puts("Connected\n");

	//keep communicating with server
	int rcv_count=0;
	while(1)
	{
		printf("Enter message : ");
		scanf("%s" , message);
		printf("my message : %s, strlen=%d\n",message,strlen(message));
		//Send some data
		if( send(sock , message , strlen(message) , 0) < 0)
		{
			puts("Send failed");
			return 1;
		}

		//Receive a reply from the server
		if( (rcv_count=recv(sock , server_reply , 2000 , 0)) < 0)
		{
			puts("recv failed");
			break;
		}
		server_reply[rcv_count]='\0';
		printf("Server reply : %d\n",rcv_count);
		puts(server_reply);
	}

	close(sock);
	return 0;
}
