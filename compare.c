#include "compare.h"


//compare()
int compare_output(int fd_out_ref,char* rx,int size){
	//checking
	if(!fd_out_ref_checked){
		fd_out_ref_checked=1;
		int fd_flags=fcntl(fd_out_ref,F_GETFD);
		if(fd_flags < 0){
			debugt("compare_output","the fd_out_ref in invalid");
			return -10;
		}
		fd_flags=fcntl(fd_out_ref,F_GETFL);
		if( !(fd_flags&O_RDWR) && !(fd_flags&O_RDONLY)){
			debugt("compare_output","the fd_out_ref is not readible");
			return -20;
		}
	}
	//comparing
	if(size==0) return 0;
	int d=0;
	char* tmprx=rx;

	while(size>0){
		int rd;
		rd=read(fd_out_ref,buffer,READ_SIZE>size?size:READ_SIZE);
		if(rd<=0) return -30;
		if( (d=memcmp(buffer,tmprx,rd)) !=0 ) {
			return d;
		}
		debugt("alert","im here : %d",rd);
		size-=rd;
		tmprx=rx+rd;
	}

	return 0;
}
