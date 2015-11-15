/***
* logo.h: debugging & logging functions
*/

#ifndef H_LOG
#define H_LOG


//this functions are designed to debug different parts of KudosJudge.
// for the time being, it only prints messages. but as the project advances, 
// one can do more stuff with the debug information, like storing them in a file. and not showing 
// then on the user terminal.

/** simple message */
void debug(char* message,...);
/** debuglinux: debug linux error (errno) */
void debugl();
/** append linux error errno to a user message*/
void debugll(char*message,...);
/*debug by a tagged message*/
void debugt(char*tag,char*message,...);
/*fprintf a fixed length of bytes.*/
void print_bytes(char*buffer,int len);
#endif
