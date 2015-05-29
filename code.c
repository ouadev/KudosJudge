#include <stdio.h>
#include <stdlib.h>


int main(int argc,char*argv[]){
	int s=0,i;
	for(i=0;i<10000;i++){
		s+=i;
		s=s%7;
	}
	
	printf("result is : %d \n",s);
	return 0;
}