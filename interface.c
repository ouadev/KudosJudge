#include "interface.h"


int jug_int_recv(int client_sock,int_request* request){
	int read_size, read_size_acc=0;
	while(1){
		read_size = recv(client_sock , request+read_size_acc , sizeof(int_request) , 0) ;
		read_size_acc+=read_size;
		if(read_size_acc==sizeof(int_request)) break;
	}
	return 0;
}



int jug_int_write(int client_sock,int_response* response){
	return write(client_sock , response, sizeof(int_response));
}
