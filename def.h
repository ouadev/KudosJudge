#ifndef H_DEF
#define H_DEF

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





#endif