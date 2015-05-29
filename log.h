/***
* logo.h: debugging & logging functions
*/

#ifndef H_LOG
#define H_LOG


/** simple message */
void debug(char* message);
/** debuglinux: debug linux error (errno) */
void debugl();
/** append linux error errno to a user message*/
void debugll(char*message);
/*debug by a tagged message*/
void debugt(char*tag,char*message);

#endif