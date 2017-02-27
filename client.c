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


//low-level network function
int kudosjudge_send(int client_sock, char* s);
char* kudosjudge_receive(int client_sock);

//business primitives
char* kudosdjudge_forge_dd(char* source_filename, char* input_filename, char* output_filename);
void print_hr_duration(unsigned long useconds);

/**
*
* ./client 
*/
int main(int argc , char *argv[])
{
	//parse arguments

	if(argc<4){printf("need more args\n");return 0;}
	char* source_filename=argv[1];
	char* input_filename=argv[2];
	char* output_filename=argv[3];

	//TEST
	struct timeval tv_start, tv_end;
	unsigned long useconds;
	int sock;
	struct sockaddr_in server;
	int sent;
	///////
	//connection///
	//////////
	
	//char  server_reply[2000];

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

	puts("KudosClient: Connected");


	//request
	char* request=NULL;

	request=kudosdjudge_forge_dd(source_filename, input_filename,output_filename);

	if(request==NULL){
		printf("Client : forging the request failed\n");
		return 1;
	}
	//record start time
	gettimeofday(&tv_start,NULL);
	//send data
	sent= kudosjudge_send(sock, request);
	
	//receive the verdict
	char* server_reply=kudosjudge_receive(sock);

	printf("Verdict : %s\n", server_reply);
	
	//record end time
	gettimeofday(&tv_end,NULL);
	useconds= (1000000*tv_end.tv_sec + tv_end.tv_usec) - (1000000*tv_start.tv_sec + tv_start.tv_usec);
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

char* kudosjudge_receive(int client_sock){
	char init_buffer[100];
	int rcv_count=0, rcv_count_acc=0, size_expected=0, metadata_got=0;
	while(1){
		rcv_count=recv(client_sock,init_buffer, sizeof(init_buffer),0 );
		if(rcv_count<0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
			puts("KudosClient: recv failed,");
			printf("KudosClient : error : %s\n", strerror(errno));
			return NULL;
		}
		init_buffer[rcv_count]='\0';
		rcv_count_acc+=rcv_count;
		char* pp;
		if( (pp=strchr(init_buffer, '\n') )!=NULL){
			size_expected=(pp-init_buffer)+1+atoi(init_buffer);
			break;
		}
	}
	//continue getting
	char* buffer=(char*)malloc(sizeof(char) * size_expected+10);
	memcpy(buffer, init_buffer, rcv_count_acc);
	char* tmp=buffer+rcv_count_acc;
	while(1){
		rcv_count=recv(client_sock, tmp, size_expected, 0);
		if(rcv_count<0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
			puts("KudosClient: recv failed,");
			printf("KudosClient : error : %s\n", strerror(errno));
			return NULL;
		}
		rcv_count_acc+=rcv_count;
		if(rcv_count_acc>= size_expected) break;
	}
	*(tmp)='\0';
	//remove haeder (size of message)
	int body_len=0;
	char verdict_s[50];
	body_len=atoi(buffer);
	int i=0;
	while(buffer[i]!='\n') i++;i++;
	char* final_buffer=(char*)malloc(sizeof(char)*strlen(buffer));
	strcpy(final_buffer, buffer+i);
	free(buffer);
	return final_buffer;
}


void print_hr_duration(unsigned long useconds){
	
		printf("%lds ", useconds/1000000);
		useconds=useconds%1000000;

		printf("%ldms ", useconds/1000);
		useconds=useconds%1000;
		
		printf("%ldus\n", useconds);
			
}


char* kudosdjudge_forge_dd(char* source_filename, char* input_filename, char* output_filename){
	

	FILE* source_file=fopen(source_filename,"r");
	FILE* input_file=fopen(input_filename, "r");
	FILE* output_file=fopen(output_filename, "r");

	char source[1000];source[0]='\0';

	fseek(input_file, 0L, SEEK_END);
	ssize_t input_file_size=ftell(input_file);
	rewind(input_file);

	char* input =(char*)malloc(sizeof(char)* (input_file_size + 1));
	input[0]='\0';
	
	char output[4000];output[0]='\0';

	char block_buffer[10000];


	 int read=0;
	while(!feof(source_file)){
		read=fread(block_buffer,sizeof(char), sizeof(block_buffer), source_file);
		block_buffer[read]='\0';
		strcat(source, block_buffer);
	}

	while(!feof(input_file)){
		read=fread(block_buffer,sizeof(char), sizeof(block_buffer), input_file);
		block_buffer[read]='\0';
		strcat(input, block_buffer);	
	}

	while(!feof(output_file)){
		read=fread(block_buffer,sizeof(char), sizeof(block_buffer), output_file);
		block_buffer[read]='\0';
		strcat(output, block_buffer);
	}

	//printf("source:\n%s\n", source);
	//printf("input:\n%s\n", input );
	//printf("output:\n%s\n", output );
	
	///build request string.
	int type=0;
	int in_type=0;//data
	int out_type=0;//data

	char* body=(char*)malloc(sizeof(char)*(strlen(source)+strlen(input)+strlen(output)+400));
	if(body==NULL) {printf("cannot allocate space for body\n"); return NULL;}
	char* request=(char*)malloc(sizeof(char)*(strlen(source)+strlen(input)+strlen(output)+400));
	if(request==NULL) {printf("cannot allocate space for request\n"); return NULL;}
	sprintf(body,"%d\n%d\n%ld\n%s\n%d\n%ld\n%s\n%ld\n%s", 
		type, in_type,strlen(input), input,out_type, strlen(output), output, strlen(source), source );
	int bodylen=strlen(body);
	sprintf(request, "%d\n%s",bodylen, body );
	free(body);


	return request;


	
	
	

}