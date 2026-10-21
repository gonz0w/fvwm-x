/* -*-c-*- */

#ifndef FVWM_MODULE_LIST_H
#define FVWM_MODULE_LIST_H


/* for F_CMD_ARGS and exec_context_t */
#include "fvwm/execcontext.h"

/* Complete transport/input records live in module_records.h. */
typedef struct fmodule fmodule;
typedef struct fmodule_input fmodule_input;
typedef struct fmodule_config_packet fmodule_config_packet;
typedef struct fmodule_store fmodule_store;
typedef fmodule_store *fmodule_list;

/* Exhaustion or explicit finish releases the iteration's cleanup hold. */
typedef struct fmodule_list_itr
{
	fmodule_store *next;
	Bool active;
} fmodule_list_itr;

typedef enum
{
	FMODULE_INPUT_ERROR = -1,
	FMODULE_INPUT_INCOMPLETE = 0,
	FMODULE_INPUT_READY = 1
} fmodule_input_status;


/*
 *	Basic Module Handling Functions
 */

/* kill all modules */
void module_kill_all(void);

/* create, register, or destroy a module record */
fmodule *module_create(void);
void module_register(fmodule *module);
void module_destroy(fmodule *module);
Bool module_output_queue_reserve(fmodule *module, size_t entry_size);
void module_output_queue_release(fmodule *module, size_t entry_size);

/* Copied configure snapshots share the output byte cap and coalesce by X ID.
 * Delivery waits for the outer command stack, never retaining window pointers. */
void module_config_interaction_begin(void);
void module_config_interaction_end(void);
Bool module_config_defer(fmodule *module, const unsigned long *packet,
	size_t size);
Bool module_config_has_window(const fmodule *module, Window window);
void module_config_discard(fmodule *module, Window window);
fmodule_config_packet *module_config_take(fmodule *module);
/* Outer event loop only: the enclosing command stack has unwound. */
void module_config_dispatch_ready(void);
Bool module_config_pending(void);
void module_config_flush_one(void);

/* kill a module */
void module_kill(fmodule *module);

/* execute module wrapper, desperate mode */
fmodule *executeModuleDesperate(F_CMD_ARGS);


/*
 *	Basic Module Communication Functions
 */

/* send "raw" data to the module */
/* module_send(fmodule *module, unsigned long *ptr, int size); */
void PositiveWrite(fmodule *module, unsigned long *ptr, int size);
/* Queue copied bytes through the normal masks/limits, without a sync wait. */
void module_output_enqueue(fmodule *module, unsigned long *packet, int size);

/* Incrementally receives one command from a nonblocking module pipe. */
fmodule_input_status module_receive(
	fmodule *module, fmodule_input **ret_input);
/* A synchronous owner can check its deadline before each nonblocking read,
 * including interrupted retries. A false check leaves partial input intact. */
fmodule_input_status module_receive_checked(fmodule *module,
	fmodule_input **ret_input, Bool (*check)(void *), void *context);

/* Queue admission is separate from synchronous handshakes, which must still
 * receive unlock responses when the producer's command queue is full. */
Bool module_input_queue_ready(const fmodule *module);
fmodule_input_status module_receive_queued(
	fmodule *module, fmodule_input **ret_input);
/* Release the admission slot before executing an input. Idempotent; discard
 * also releases it if a queued input is abandoned without executing. */
void module_input_dequeue(fmodule_input *input);

/* frees an input data struct */
void module_input_discard(fmodule_input *input);

/* returns true if received the "expect" string, false otherwise */
Bool module_input_expect(fmodule_input *input, char *expect);


/*
 *	Utility Functions
 */

/* initializes the given iterator */
void module_list_itr_init(fmodule_list_itr *itr);
void module_list_itr_finish(fmodule_list_itr *itr);
/* gets the next module on the list */
fmodule *module_list_itr_next(fmodule_list_itr *itr);

/* free modules in the deathrow */
void module_cleanup(void);



/*
 *	Message Queue Handling Functions
 */

/* message queues */
void FlushAllMessageQueues(void);
void FlushMessageQueue(fmodule *module);


/*
 *	Misc Functions (should they be here?)
 */

/*
 * exposed to be used by modconf.c
 */
char *skipModuleAliasToken(const char *string);


/* dead pipe signal handler - empty */
RETSIGTYPE DeadPipe(int nonsense);

#endif /* MODULE_LIST_H */
