/***
* logo.h: debugging & logging functions
*/

#ifndef H_LOG
#define H_LOG



//this functions are designed to debug different parts of KudosJudge.
// for the time being, it only prints messages. but as the project advances, 
// one can do more stuff with the debug information, like storing them in a file.

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

/*debug_focus
 * @param tags null-terminated array of strings designating the tags
 * to focus on while debuging. give NULL to disbale focusing on tags
 * @note maximum tags : 8
 * */
void debug_focus(char* tags[]);

#endif
