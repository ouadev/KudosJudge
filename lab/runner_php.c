#include "../sandbox.h"
#include "../ramfs.h"
#include <stdlib.h>
#define MAX_CODE_SIZE 50000  //50 KB
#define MAX_BIN_OUTPUT 500000; //500 KB is more than enough

/*
 * @usage: runner_php [c_source_file]
 *
 * + create a ramfs directory somewhere in $JUG_CHROOT.
 * + create a unique.c and compile it.
 * + give it to the Runner. it will change the working dir, and change filename in argv.
 * @note: binaries in /ramfs are being deleted. and sources are kept bur inaccessible.
 */

//output holder
char* output_holder;
int 	output_count=0;

/*
 * php_compare_output
 */
int php_compare_output(int fd_out_ref,char* rx,int size,int end);

/*
 * main
 */
int main(int argc,char*argv[]){
	char* jug_chroot;
	char line[10000];
	char cmd[1000];
	char output_file[1000];
	char erfs_bin_path[500];
	char ramfs_root[400];
	pid_t upid;
	FILE* ufile;
	int fail=0;
	//check user
	if(geteuid()!=0){
		debugt("runner_php","must be run as root, uid:%d ",geteuid());
		printf("JUDGE INTERNAL ERROR, SORRY\n");
		return -10;
	}
	//bash pipelining ?
	if(argc==2){
		printf("[test] argv[1]=%s\n",argv[1]);
		if(freopen(argv[1],"r",stdin)==NULL){
			debugt("runner_php","cannot freopen stdin to the source : %s, error:%s\n",argv[1],strerror(errno));
			printf("JUDGE INTERNAL ERROR, SORRY\n");
			return -8;
		}
	}


	//get env
	if (  (jug_chroot=getenv("JUG_CHROOT"))==NULL){
		debugt("runner_php","JUG_CHROOT env not found \n");
		printf("JUDGE INTERNAL ERROR, SORRY\n");
		return -2;
	}

	//prepare ramfs
	ramfs_info* ramfsinfo=init_ramfs(jug_chroot,"ramfs",10,15);
	debugt("runner_pgp","jug_chroot:%s",jug_chroot);
	debugt("runner_php","path : %s",ramfsinfo->path);

	//	umount_ramfs(ramfsinfo);
	//	rm_ramfs_dir(ramfsinfo);
	//	return 0;

	fail=create_ramfs(ramfsinfo);
	if(fail && fail!=-2){//-2: already exists, good
		debugt("runner_php","cannot initialize ramfs directory");
		printf("JUDGE INTERNAL ERROR, SORRY\n");
		return -9;
	}

	//create unique.c in /tmp
	upid=getpid();
	char upath[50];
	sprintf(upath,"%s/submission_%d.c",ramfsinfo->path,(int)upid);

	umask(S_IWGRP|S_IRGRP|S_IWOTH|S_IROTH);

	ufile=fopen(upath,"w+");
	if(ufile==NULL){
		printf("cannot create a tmp C file, error:%s\n",strerror(errno));
		printf("path : %s\n",upath);
		return -3;
	}


	//read C code
	while(fgets(line,10000,stdin)!=NULL){
		fwrite(line,1,strlen(line),ufile);
	}
	fclose(ufile);



	//Compile The Code
	sprintf(output_file,"%s/submission_%d",ramfsinfo->path,(int)upid);
	debugt("runner_php","source path: %s",upath);
	sprintf(cmd,"gcc-4.7 %s -o %s",upath,output_file);
	system(cmd);

	struct stat st;
	if(stat(output_file,&st) != 0){
		printf("COMPILER ERROR\n");
		return -4;
	}


	//run the compiled binary
	struct sandbox sb;
	jug_sandbox_result result;
	struct run_params runp;
	int ret,infd,rightfd;
	ret=jug_sandbox_init(&sb);
	if(ret){
		printf("JUDGE INTERNAL ERROR, SORRY\n");
		return -5;
	}

	infd=open("/home/odev/jug/tests/problems/twins/twins.in",O_RDWR);
	rightfd=open("/home/odev/jug/tests/problems/twins/twins.out",O_RDWR);
	//allocate memory for the output holder
	output_holder=(char*)malloc(500000);
	if(output_holder==NULL){
		debugt("runner_php","cannot allocate memory for the output");
		printf("JUDGE INTERNAL ERROR, SORRY\n");
		return -7;
	}
	//change the config.ini default parameters with your own stuff
	//-1 means : take the default values from the config file
	runp.mem_limit_mb=-1;
	runp.time_limit_ms=-1;
	runp.fd_datasource=infd;
	runp.fd_datasource_dir=0;
	runp.compare_output=php_compare_output;
	runp.fd_output_ref=rightfd;

	//run the binary inside the sandbox.
	sprintf(erfs_bin_path,"/%s/submission_%d",ramfsinfo->dirname,(int)upid);
	debugt("runner_php","erfs_bin: %s",erfs_bin_path);
	char *bin_args[2]={erfs_bin_path,NULL};
	ret=jug_sandbox_run(&runp,&sb,erfs_bin_path,bin_args);
	//
	if(ret>=0){
		result=runp.result;
		//format the output
		output_holder[output_count]='\0';
		printf("%s\n",jug_sandbox_result_str(result));
		//printf("output_holder size:%d, output_count:%d\n",strlen(output_holder),output_count);
		printf("%s",output_holder);
	}else{
		printf("JUDGE INTERNAL ERROR, SORRY\n");
		debugt("runner_php","cannot run the binary, sandbox_run returns %d\n",ret);
		return -6;
	}
	close(infd);
	close(rightfd);
	//
	//remove(upath);
	remove(output_file);
	remove(upath);

	return 0;
}



//compare()
int php_compare_output(int fd_out_ref,char* rx,int size,int end){
	int d=0,i,j,rd=0,rd2=0,read_plus=0,goback=0,dec=0;
	char* tmprx=rx;
	char bufftest[1];
	//save rx: specific to Runner_php
	if(size>0){
		debugt("php_compare_output","%d octets received",size);
		int max_bin_out=MAX_BIN_OUTPUT;
		if(output_count>max_bin_out){
			debugt("runner_php","max output size reached");
			return -66;
		}
		memcpy(output_holder+output_count,rx,size);
		output_count+=size;

	}

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

