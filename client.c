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
#include <errno.h>
#include <sys/time.h>


/**
 * struct int_response
 * the structure to be used multiple times during the same connection
 */
typedef struct int_response{
	char echo[15]; //< should be filled with the int_request.echo
	int verdict; //< a number that represents different verdicts
	char verdict_s[60]; //< the verdict in string format.
} int_response;

int kudosjudge_send(int client_sock, char* s);
void print_hr_duration(unsigned long useconds);


int main(int argc , char *argv[])
{
	///////
	//connection///
	//////////
	int sock;
	struct sockaddr_in server;
	char  server_reply[2000];

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
	/////////////////
	//// Request/////
	////////////////
	////record start time
	struct timeval tv_start;
	gettimeofday(&tv_start,NULL);
	///build request string.
	int type=0;
	int in_type=0;//data
	int out_type=0;//data
	char* sourcecode="#include <stdio.h>\nvoid main(){\nwhile(1){};printf(\"Hello KudosJudge\");\n}";
	char* input		="nothing\nspecial";
	char* output	="Hello KudosJudge";
	char* body=(char*)malloc(sizeof(char)*(strlen(sourcecode)+strlen(input)+strlen(output)+400));
	char* request=(char*)malloc(sizeof(char)*(strlen(sourcecode)+strlen(input)+strlen(output)+400));
	sprintf(body,"%d\n%d\n%ld\n%s\n%d\n%ld\n%s\n%ld\n%s", 
		type, in_type,strlen(input), input,out_type, strlen(output), output, strlen(sourcecode), sourcecode );
	int bodylen=strlen(body);
	sprintf(request, "%d\n%s",bodylen, body );
	free(body);
	//send data
	//int wrt_count=write(sock, request, strlen(request));

	//send data
	int sent= kudosjudge_send(sock, request);
	//receive Judge's response
	 int_response response;
	 int rcv_count=0;


	
	///////////////
	 //// receive/////
	 //////////////////
	//receive the verdict
	char* tmp=server_reply;
	int size_expected=0;
	int metadata_got=0;
	int rcv_count_acc=0;
	while ( 1){
		rcv_count=recv(sock,tmp, sizeof(server_reply),0 );
		if(rcv_count<0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
			puts("KudosClient: recv failed,");
			printf("KudosClient : error : %s\n", strerror(errno));
			return 54;
		}
		tmp[rcv_count]='\0';
		if( !metadata_got){
			char* pp;
			if( (pp=strchr(tmp, '\n') )!=NULL){
				metadata_got=1;
				size_expected=(pp-tmp)+1+atoi(tmp);
			}
		}
		rcv_count_acc+=rcv_count;
		tmp+=rcv_count;
		if(rcv_count_acc>=size_expected) break;
		

	} 
	*(tmp)='\0';
	int body_len=0;
	char verdict_s[50];
	body_len=atoi(server_reply);
	int i=0;
	while(server_reply[i]!='\n') i++;i++;
	strcpy(verdict_s, &server_reply[i]);
	printf("Verdict : %s\n", verdict_s);
	
	/////record end time
	struct timeval tv_end;
	gettimeofday(&tv_end,NULL);
	unsigned long useconds= (1000000*tv_end.tv_sec + tv_end.tv_usec) - (1000000*tv_start.tv_sec + tv_start.tv_usec);
	print_hr_duration(useconds);

//clean up
	close(sock);
	return 0;

}



int kudosjudge_send(int client_sock, char* s){
	int length=strlen(s);
	int bytesSent=0;
	while(length > 0) {
	    bytesSent = send(client_sock, s, length, 0);
	    if (bytesSent == 0)
	        break; //socket probably closed
	    else if (bytesSent < 0)
	        break; //handle errors appropriately
	    s += bytesSent;
	    length -= bytesSent;
	}

	return 1;

}



void print_hr_duration(unsigned long useconds){

		printf("%ld ms %ld us\n", useconds/1000, useconds%1000);
	
}