#ifndef H_JUG_INTERFACE
#define H_JUG_INTERFACE

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

/**
 * struct int_request
 * an example of structure to use to communicate with the daemon
 * (note: use a serialization framework to send these structures over network;
 */
typedef struct int_request{
	int type; //< type of request :
			//initContestMode (to load testcases and stuff), judgeSubmission, getState (used also to get the verdict)
	int synchronous; //<wether the client wishes to wait for the verdict or not, otherwise he will use getState afterward.
	//for debugging
	char path[50]; ///< path to execute (inside ERFS if activated)
	char tc_in_path[100]; ///< path outside ERFS
	char tc_out_path[100]; ///< path outside ERFS
	char echo[15]; ///< simple char array to return back to test connection



}int_request;


/**
 * struct int_response
 * the structure to be used multiple times during the same connection
 */
typedef struct int_response{
	char echo[15]; //< should be filled with the int_request.echo
	int verdict; //< a number that represents different verdicts
	char verdict_s[60]; //< the verdict in string format.
} int_response;

/**
 * jug_int_recv
 * @desc	receive a request from client
 * @param	request a pointer to the request buffer to fill
 * @param	client_sock socket
 * @return  0 if success,
 * @note	should be elaborated more to deal with transmission surprises
 */
int jug_int_recv(int client_sock,int_request* request);

/**
 * jug_int_write
 * @desc	write a response structure to scoket
 * @param	client_sock socket
 * @param	response int_response object
 */
int jug_int_write(int client_sock,int_response* response);

#endif
