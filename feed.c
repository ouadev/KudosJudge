#include "feed.h"

int jug_feed_init(){
	//create a directory in global_ramfs
	ramfs_info* ramfs=get_global_ramfs();
	char * feed_workspace=(char*) malloc(sizeof(char)*(strlen(ramfs->path)+100));
	sprintf(feed_workspace, "%s/%s",ramfs->path,"feed-workspace");
	mode_t oldWorkspaceMask=umask(022);
	if(mkdir(feed_workspace, 0777)!=0 && errno!=EEXIST){
		debugt("lang"," cannot create feed workspace in ramfs directory");
		free(feed_workspace);
		umask(oldWorkspaceMask);
		return -1;
	}
	umask(oldWorkspaceMask);
	g_feed_workspace=feed_workspace;
	return 0;
}


char* jug_feed_new_file(char* content, int worker_id, const char* tag){

	
	//
	char* filename=(char*)malloc(sizeof(char)*strlen(g_feed_workspace)+100);
	time_t now;
	time(&now);
	sprintf(filename,"%s/%d_%d_%s.data", g_feed_workspace,worker_id, (int)now, tag);
	//create the file
	umask(S_IWGRP|S_IRGRP|S_IWOTH|S_IROTH);
	FILE* feedfile=fopen(filename,"w+");
	if(feedfile==NULL){
		debugt("feed","cannot create a feed file",strerror(errno));
		free(filename);
		return NULL;
	}
	int pp=fputs(content,feedfile);
	if(pp<=0){
		debugt("feed","cannot write sourcecode to text_filename");
		free(filename);
		fclose(feedfile);
		return NULL;
	}
	fclose(feedfile);



	return filename;


}




void jug_feed_remove_by_name(char* filename) {
	int error=unlink(filename);
	if(error<0){
		debugt("feed", "cannot remove %s, error : %s", filename, strerror(errno));
	}
}