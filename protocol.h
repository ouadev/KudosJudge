#ifndef H_JUG_PROTOCOL
#define H_JUG_PROTOCOL
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
    //debug only
    char* bin_path;
} jug_submission;

/**
 * global stuff
 */



/**
 * methods
 */

const char* protocol_verdict_to_string(jug_verdict_enum verdict);

#endif
