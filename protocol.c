#include "protocol.h"




const char* protocol_verdict_to_string(jug_verdict_enum verdict){

	const char* strings[8]={
			"ACCEPTED",
			"WRONG ANSWER",
			"COMPILING ERROR",
			"TIME LIMIT EXCEEDED",
			"MEMORY LIMIT EXCEEDED",
			"OUTPUT LIMIT EXCEEDED",
			"RUNTIME ERROR",
			"INTERNAL ERROR"
	};

	return strings[(int)verdict];
}
