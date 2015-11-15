#include "compare.h"


//compare()
int compare_output(int fd_out_ref,char* rx,int size,int end){
	int d=0,i,j,rd=0,rd2=0,read_plus=0,goback=0,dec=0;
	char* tmprx=rx;
	char bufftest[1];
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

	//end call: check if there still be data in reference output
	if(end==1){
		while(1){
			rd=read(fd_out_ref,buffer,READ_SIZE);
			if(rd==0)	return 0;
			if(rd<0)	return -55;
			for(i=0;i<rd;i++){
				if(buffer[i]!='\n' && buffer[i]!='\r') return 10;
			}
		}
		return 0;
	}

	//comparing
	while(size>0){

		rd=read(fd_out_ref,buffer,READ_SIZE>size?size:READ_SIZE);
		if(rd<0) return -30;
		if(rd==0){
			i=size;
			while(i--)
				if(tmprx[i]!='\r' && tmprx[i]!='\n')
					return 20;
			return 0;
		}
		i=j=0;
		while(j<rd && i<size){
			if(buffer[j]=='\r'){
				read_plus++;
				j++;
				continue;
			}
			if(tmprx[i]=='\r'){
				goback++;
				i++;
				continue;
			}

			if(tmprx[i]!=buffer[j]) return i+1;
			i++;
			j++;
		}
		dec=goback-read_plus;
		if(dec>0)
			lseek(fd_out_ref, lseek(fd_out_ref,0,SEEK_CUR)-dec, SEEK_SET  );

		tmprx=tmprx+i;
		size-=i;

	}

	return 0;
}
