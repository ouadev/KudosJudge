#ifndef H_JUG_INTERFACE
#define H_JUG_INTERFACE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
 #include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "buffer/buffer.h"
#include "log.h"
#include "def.h"

#define RECEIVE_SIZE_MAX 5000
#define REQUEST_SIZE_MAX 30000000 //30MB
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
	struct sockaddr_in* client_sockaddr_in;
}jug_connection;

void jug_int_free_connection(jug_connection* connection);
/**
 * struct int_request
 * an example of structure to use to communicate with the daemon
 * (note: use a serialization framework to send these structures over network;
 * + should recompile client.c if something changed in the definition of this structure
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

int jug_int_init();

unsigned int  g_socketbuffer_max_size;
int g_socketbuffer_max_size_calculated;

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




/**
 * methods
 */

const char* jug_int_verdict_to_string(jug_verdict_enum verdict);
int jug_int_send_verdict(int client_sock,jug_verdict_enum verdict);


#endif
