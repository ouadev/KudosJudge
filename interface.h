#ifndef H_JUG_INTERFACE
#define H_JUG_INTERFACE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
 #include <sys/socket.h>
#include <unistd.h>
#include "buffer/buffer.h"
#include "log.h"

#define RECEIVE_SIZE_MAX 500
/**
 * interface.h
 * the communicatio primitives that the daemon (and eventually a client) uses to communicate.
 * the API, in theory should be the same for different communication mechanismes (sockets, pipes, ...)
 * in order to write the protocol.h file once and use it several times with different eventual commmunication
 * mediums.
 * (this file is to be included by both the daemon and the client)
 * //NOTE: a thread should be allocated only to respond to getState requests
 */

/**
 * struture that represents a connection
 * may contain more data
 * may use a union to support multiple types of communication
 */

typedef struct jug_connection{
	int client_socket; //client socket
}jug_connection;

void jug_int_free_connection(jug_connection* connection);
/**
 * struct int_request
 * an example of structure to use to communicate with the daemon
 * (note: use a serialization framework to send these structures over network;
 * + should recompile client.c if something changed in the definition of this structure
 */
/*
typedef struct int_request{
	int type; //< type of request :
			//initContestMode (to load testcases and stuff), judgeSubmission, getState (used also to get the verdict)
	int synchronous; //<wether the client wishes to wait for the verdict or not, otherwise he will use getState afterward.

	int id; ///< this ID is provided by the client to be able to follow the state of a submission, (useful when synchronous is off)

	char tc_in_path[100]; ///< path outside ERFS
	char tc_out_path[100]; ///< path outside ERFS
	char sourcecode_path[100];
	char sourcecode[500]; //the source code of the submission
	//for debugging
	char path[50]; ///< path to execute (inside ERFS if activated) // in case the client give directly the binary (only in debug mode)
	char echo[15]; ///< simple char array to return back to test connection
	//



}int_request;
*/

typedef enum {INT_REQ_TYPE_JUDGE, INT_REQ_TYPE_SETFEED} int_request_type;
typedef enum {INT_REQ_FEEDTYPE_DATA, INT_REQ_FEEDTYPE_REFERENCE} int_request_feed_type;
typedef struct int_request{
	int_request_type type;
	int_request_feed_type in_type;
	int_request_feed_type out_type;
	char* sourcecode;

	int in_reference;
	char* input;

	int out_reference;
	char* output;
} int_request;

/**
 * struct int_response
 * the structure to be used multiple times during the same connection
 */
typedef struct int_response{
	char echo[15]; //< should be filled with the int_request.echo
	int verdict; //< a number that represents different verdicts
	char verdict_s[60]; //< the verdict in string format.
} int_response;


buffer_t* jug_int_receive(int client_sock);
int jug_int_decode(buffer_t* buffer, int_request* request);
void jug_int_free_request(int_request* request);



/* Protocol Part*/
/**
 * protocol.h
 * definition of the submission information, languages , ...
 * (execution mode, language, test cases, time limit, mem limit, ...)
 * In other words the protocol of communication is defined here.
 * this functions are intented to be called by the worker threads after they retreive a submission from
 * the queue.
 * @note: this file is like a layer between the (interface/queue), (eventual compiling_layer) and (sandbox)
 */


/**
 * supported languages
 */

typedef enum {LANG_C, LANG_CPP,  LANG_JAVA, LANG_PHP, LANG_PYTHON} jug_lang_enum;
/**
 * possible judge verdicts
 */
typedef enum { 	VERDICT_ACCEPTED,
				VERDICT_WRONG,
				VERDICT_COMPILE_ERROR,
				VERDICT_TIMELIMIT,
				VERDICT_MEMLIMIT,
				VERDICT_OUTPUTLIMIT,
				VERDICT_RUNTIME,
				VERDICT_INTERNAL //internal error (judge's mistake)
				} jug_verdict_enum;


/**
 * the structure that represents the submission. made during communication.
 */

typedef struct jug_submission
{
    char* source;
    char* input_filename;
    char* output_filename;
    int   language;
    int   time_limit;
    int   mem_limit;
    int   thread_id; //propagated to the other parts of the judge (debug only)
    //exec stuff
    int   interpreted;
    char* bin_cmd; //the command to execute
    char* bin_path; //the path to the file to be executed/interpreted
} jug_submission;

/**
 * global stuff
 */



/**
 * methods
 */

const char* jug_int_verdict_to_string(jug_verdict_enum verdict);
int jug_int_send_verdict(int client_sock,jug_verdict_enum verdict);
void jug_int_free_submission(jug_submission* submission);

#endif
