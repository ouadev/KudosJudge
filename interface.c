#include "interface.h"







//decode the raw data we receive from the client.
//format : (req_type)\n(input_type)\n(input_size)\n(input)\n(output_type)\n(output_size)\n(output)\n(source_size)\n(sourcecode)
//0: success
int jug_int_decode(buffer_t* buffer, int_request* request){
	int ssflag;

	int type;
	int in_type;
	int out_type;
	int in_size;
	int out_size;
	int in_reference;
	int out_reference;
	int sourcecode_size;

	const char *  string_of_buffer=buffer_get_string(buffer);
	char* string=(char*)string_of_buffer;

	type=atoi(string);
	while(string[0]!='\n') string++;string++;
	request->type=type;
	
	//parse input

	in_type=atoi(string);
	while(string[0]!='\n') string++;string++;
	request->in_type=in_type;

	
	if(in_type==INT_REQ_FEEDTYPE_DATA){
		in_size=atoi(string);
		while(string[0]!='\n') string++;string++;
		request->input=(char*)malloc(sizeof(char)*in_size+1);

		if(strlen(string)<in_size){
			free(request->input);
			return -2; //unexpected end
		}
		memcpy(request->input, string, in_size);
		request->input[in_size]='\0';
		string += in_size+1; //+1 for the \n separator.
	

	}else if(in_type==INT_REQ_FEEDTYPE_REFERENCE){
		in_reference=atoi(string);
		while(string[0]!='\n') string++;string++;
		request->in_reference=in_reference;
	}else{
		return -3; //unkown type of input
	}
	//parse output
	out_type=atoi(string);
	while(string[0]!='\n') string++;string++;
	request->out_type=out_type;
	
	if(out_type==INT_REQ_FEEDTYPE_DATA){
		out_size=atoi(string);
		while(string[0]!='\n') string++;string++;
		
		request->output=(char*)malloc(sizeof(char)*out_size+1);
		if(strlen(string)<out_size){
			free(request->output);
			return -2; //unexpected end
		}
		memcpy(request->output, string, out_size);
		request->output[out_size]='\0';
		string += out_size+1; //+1 for the \n separator.

	}else if(out_type==INT_REQ_FEEDTYPE_REFERENCE){
		out_reference=atoi(string);
		while(string[0]!='\n') string++;string++;
		request->out_reference=out_reference;
	}else{
		return -3; //unkown type of input
	}


	//keep parsing
	if(type==INT_REQ_TYPE_JUDGE){
		sourcecode_size=atoi(string);
		while(string[0]!='\n') string++;string++;
		request->sourcecode=(char*)malloc(sizeof(char)*sourcecode_size+1);
		if(strlen(string)<sourcecode_size){
			free(request->sourcecode);
			return -2;//unexpected end
		}
		memcpy(request->sourcecode , string, sourcecode_size);
		request->sourcecode[sourcecode_size]='\0';
		string += sourcecode_size+1 ; //+1 for the \n separator.

	}else if(type==INT_REQ_TYPE_SETFEED){
		//no source code expected
		if(strlen(string)>0) return -4; //expected end
	}else{
		return -3;//unknown type of input.
	}

	


	return 0;

}


void jug_int_free_request(int_request* request){
	free(request->sourcecode);
	free(request->input);
	free(request->output);
}

void jug_int_free_submission(jug_submission* submission){
	free(submission->input_filename);
	free(submission->output_filename);
	free(submission->source);
	free(submission->bin_path);
	free(submission->bin_cmd);
}


void jug_int_free_connection(jug_connection* connection){
	free(connection);
}


//receive athe message from the client
buffer_t* jug_int_receive(int client_sock){
	buffer_t* request_buffer=buffer_new();
	char tmp_buffer[RECEIVE_SIZE_MAX];
	int read_size, read_size_acc=0;
	int metadata_read=0;
	int request_size=0;
	int body_start_index=0;
	while(1){
		read_size = recv(client_sock , tmp_buffer , sizeof(tmp_buffer) , 0) ;
		if(read_size<0){
			//error
			buffer_free(request_buffer);
			return NULL;
		}
				//
		read_size_acc+=read_size;
		buffer_append_n(request_buffer, tmp_buffer, read_size);
		if( !metadata_read && (body_start_index=buffer_indexof(request_buffer, "\n"))>0){
			metadata_read=1;
			request_size= body_start_index + 1 + atoi(buffer_get_string(request_buffer));
		}
		if(read_size==0 || (metadata_read && read_size_acc>=request_size)) break;
	}
	buffer_t* pure_request_buffer=buffer_slice(request_buffer, body_start_index+1, buffer_size(request_buffer)-1);
	buffer_free(request_buffer);
	return pure_request_buffer;
}
//send verdict
int jug_int_send_verdict(int client_sock,jug_verdict_enum verdict){
	char response[100];
	char hr_vedict[50];
	sprintf(hr_vedict, "%s", jug_int_verdict_to_string(verdict));
	sprintf(response, "%d\n%s", (int)strlen(hr_vedict), hr_vedict);
	//debugt("interface", "response text : \n %s", response);
	//end

	int length=strlen(response);
	int bytesSent=0;
	char* tmp=response;
	while(length > 0) {
	    bytesSent = send(client_sock, tmp, length, 0);
	    if (bytesSent == 0)
	        break; //socket probably closed
	    else if (bytesSent < 0)
	        break; //handle errors appropriately
	    tmp += bytesSent;
	    length -= bytesSent;
	}

	return bytesSent;


}




const char* jug_int_verdict_to_string(jug_verdict_enum verdict){

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
