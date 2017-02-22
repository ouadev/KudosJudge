#ifndef H_FEED
#define H_FEED
/**
* Manage feed: input and correct output files. manage storage, reference, and cleaning.
*/
#include <time.h>
#include "ramfs.h"

int jug_feed_init();

char* jug_feed_new_file(char* content, int worker_id, const char* tag);

void jug_feed_remove_by_name(char* filename);
//global
char* g_feed_workspace;
#endif