#ifndef FVWM_MODULE_RECORDS_H
#define FVWM_MODULE_RECORDS_H

#include "module_list.h"
#include "libs/Module.h"
#include "libs/fqueue.h"

/* please don't use msg_masks_t and PipeMask outside of module_interface.c.
 * They are only global to allow to access the IS_MESSAGE_SELECTED macro without
 * having to call a function. */
typedef struct msg_masks_t
{
	unsigned long m1;
	unsigned long m2;
} msg_masks_t;

struct fmodule_config_packet
{
	struct fmodule_config_packet *next;
	size_t size;
	unsigned long data[];
};

/* module linked list record, only to be accessed by using the access macros
 * below */
struct fmodule
{
        struct
        {
		unsigned is_cmdline_module : 1;
		unsigned is_retired : 1;
        } xflags;
	int xreadPipe;
	int xwritePipe;
	fqueue xpipeQueue;
	size_t xpipeQueueBytes;
	unsigned char xinputBuffer[
		MAX_MODULE_INPUT_TEXT_LEN + sizeof(Window) +
		2 * sizeof(unsigned long)];
	size_t xinputBytes;
	size_t xinputExpected;
	/* Queued/active inputs and synchronous frames pin death-row records. */
	size_t xinputRefs;
	size_t xqueuedInputs;
	size_t xactiveInputs;
	fmodule_config_packet *xdeferredConfig;
	size_t xdeferredConfigCount;
	struct fmodule_input *xcommandHead;
	struct fmodule_input *xcommandTail;
	msg_masks_t xPipeMask;
	msg_masks_t xNoGrabMask;
	msg_masks_t xSyncMask;
	char *xname;
	char *xalias;
};

#define MOD_IS_CMDLINE(m) ((m)->xflags.is_cmdline_module)
#define MOD_SET_CMDLINE(m,on) ((m)->xflags.is_cmdline_module = !!(on))
#define MOD_IS_RETIRED(m) ((m)->xflags.is_retired)

struct fmodule_store
{
	fmodule *module;
	struct fmodule_store *next;
	struct fmodule_store *retired_next;
};

#define MOD_READFD(m) ((m)->xreadPipe)
#define MOD_WRITEFD(m) ((m)->xwritePipe)
#define MOD_PIPEQUEUE(m) ((m)->xpipeQueue)
#define MOD_PIPEQUEUE_BYTES(m) ((m)->xpipeQueueBytes)
#define MOD_PIPEMASK(m) ((m)->xPipeMask)
#define MOD_NAME(m) ((m)->xname)
#define MOD_ALIAS(m) ((m)->xalias)
#define MODULE_QUEUE_MAX_BYTES (8U * 1024U * 1024U)
#define MODULE_INPUT_QUEUE_MAX_COMMANDS 256U
#define MODULE_DEFERRED_CONFIG_MAX_WINDOWS 64U

/* this is a bit long winded to allow MAX_MESSAGE to be 32 and not get an
 * integer overflow with (1 << MAX_MESSAGES) and even with
 * (1<<(MAX_MESSAGES-1)) - 1 */
#define DEFAULT_MASK   (MAX_MSG_MASK & ~(M_SENDCONFIG))
#define DEFAULT_XMASK  (DEFAULT_XMSG_MASK)

/*
 * Returns zero if the msg is not selected by the mask. Takes care of normal
 * and extended messages.
 */
#define IS_MESSAGE_IN_MASK(mask, msg) \
	(((msg)&M_EXTENDED_MSG) ? ((mask)->m2 & (msg)) : ((mask)->m1 & (msg)))

/*
 * Returns non zero if one of the specified messages is selected for the module
 */
#define IS_MESSAGE_SELECTED(module, msg_mask) \
	IS_MESSAGE_IN_MASK(&(MOD_PIPEMASK(module)), (msg_mask))

/*
 * M_SENDCONFIG for   modules to tell  fvwm that  they  want to  see each
 * module configuration command as   it is entered.  Causes  modconf.c to
 * look at each active module, find  the ones that sent M_SENDCONFIG, and
 * send a copy of the command in an M_CONFIG_INFO command.
 */

/* struct to store module input data */
struct fmodule_input
{
	Window window;
	fmodule *module;
	char *command;
	Bool is_queued;
	Bool in_command_queue;
	struct fmodule_input *queue_previous;
	struct fmodule_input *queue_next;
	struct fmodule_input *producer_next;
};

#endif /* FVWM_MODULE_RECORDS_H */
