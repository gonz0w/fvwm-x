/* -*-c-*- */

#include "config.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libs/log.h"
#include "libs/safemalloc.h"
#include "module_list.h"

#define MODULE_INPUT_HEADER_SIZE (sizeof(Window) + sizeof(unsigned long))

static fmodule_list module_list;
static fmodule_list death_row;
static unsigned int module_iteration_depth;
static unsigned int config_interaction_depth;
static Bool config_command_pending;

static void module_list_insert(fmodule *module, fmodule_list *list)
{
	fmodule_store *new_store;

	if (module == NULL)
	{
		return;
	}
	new_store = fxmalloc(sizeof(*new_store));
	new_store->module = module;
	new_store->next = *list;
	new_store->retired_next = NULL;
	*list = new_store;
}

static fmodule_store *module_list_take(fmodule *module, fmodule_list *list)
{
	fmodule_store **pos;

	if (module == NULL)
	{
		return NULL;
	}
	for (pos = list; *pos != NULL; pos = &(*pos)->next)
	{
		if ((*pos)->module == module)
		{
			fmodule_store *current;

			current = *pos;
			*pos = current->next;
			return current;
		}
	}

	return NULL;
}

fmodule *module_create(void)
{
	fmodule *module;

	module = fxmalloc(sizeof(*module));
	MOD_SET_CMDLINE(module, 0);
	module->xflags.is_retired = 0;
	MOD_READFD(module) = -1;
	MOD_WRITEFD(module) = -1;
	fqueue_init(&MOD_PIPEQUEUE(module));
	MOD_PIPEQUEUE_BYTES(module) = 0;
	module->xinputBytes = 0;
	module->xinputExpected = MODULE_INPUT_HEADER_SIZE;
	module->xinputRefs = 0;
	module->xqueuedInputs = 0;
	module->xactiveInputs = 0;
	module->xdeferredConfig = NULL;
	module->xdeferredConfigCount = 0;
	module->xcommandHead = NULL;
	module->xcommandTail = NULL;
	MOD_PIPEMASK(module).m1 = DEFAULT_MASK;
	MOD_PIPEMASK(module).m2 = DEFAULT_MASK;
	module->xNoGrabMask.m1 = 0;
	module->xNoGrabMask.m2 = 0;
	module->xSyncMask.m1 = 0;
	module->xSyncMask.m2 = 0;
	MOD_NAME(module) = NULL;
	MOD_ALIAS(module) = NULL;

	return module;
}

void module_destroy(fmodule *module)
{
	if (module == NULL)
	{
		return;
	}
	if (MOD_WRITEFD(module) >= 0)
	{
		close(MOD_WRITEFD(module));
	}
	if (MOD_READFD(module) >= 0)
	{
		close(MOD_READFD(module));
	}
	while (module->xdeferredConfig != NULL)
	{
		free(module_config_take(module));
	}
	free(MOD_NAME(module));
	free(MOD_ALIAS(module));
	fqueue_remove_or_operate_all(
		&MOD_PIPEQUEUE(module), NULL, NULL, free, NULL);
	free(module);
}

void module_register(fmodule *module)
{
	module_list_insert(module, &module_list);
}

Bool module_output_queue_reserve(fmodule *module, size_t entry_size)
{
	if (entry_size > MODULE_QUEUE_MAX_BYTES ||
	    MOD_PIPEQUEUE_BYTES(module) > MODULE_QUEUE_MAX_BYTES - entry_size)
	{
		return False;
	}
	MOD_PIPEQUEUE_BYTES(module) += entry_size;

	return True;
}

void module_output_queue_release(fmodule *module, size_t entry_size)
{
	if (MOD_PIPEQUEUE_BYTES(module) >= entry_size)
	{
		MOD_PIPEQUEUE_BYTES(module) -= entry_size;
	}
	else
	{
		MOD_PIPEQUEUE_BYTES(module) = 0;
	}
}

void module_config_interaction_begin(void)
{
	config_interaction_depth++;
	config_command_pending = True;
}

void module_config_interaction_end(void)
{
	assert(config_interaction_depth > 0);
	config_interaction_depth--;
}

Bool module_config_defer(fmodule *module, const unsigned long *packet,
	size_t size)
{
	fmodule_config_packet **position, *snapshot;
	size_t allocation;

	if (!config_command_pending && module->xdeferredConfig == NULL)
	{
		return False;
	}
	/* No arithmetic, key access or copy precedes complete frame validation. */
	if (size < (FvwmPacketHeaderSize + 1) * sizeof(unsigned long) ||
	    size > FvwmPacketMaxSize_byte || size % sizeof(unsigned long) != 0 ||
	    packet == NULL || packet[0] != START_FLAG ||
	    packet[1] != M_CONFIGURE_WINDOW ||
	    packet[2] != size / sizeof(unsigned long))
	{
		fvwm_debug(__func__, "invalid deferred configure packet");
		module_kill(module);
		return True;
	}
	allocation = sizeof(*snapshot) + size;
	for (position = &module->xdeferredConfig; *position != NULL;
	     position = &(*position)->next)
	{
		if ((*position)->data[FvwmPacketHeaderSize] ==
		    packet[FvwmPacketHeaderSize])
		{
			break;
		}
	}
	snapshot = *position;
	if (snapshot != NULL && snapshot->size == size)
	{
		memcpy(snapshot->data, packet, size);
		return True;
	}
	if (snapshot != NULL)
	{
		*position = snapshot->next;
		module_output_queue_release(module, sizeof(*snapshot) + snapshot->size);
		module->xdeferredConfigCount--;
		free(snapshot);
	}
	if (module->xdeferredConfigCount >= MODULE_DEFERRED_CONFIG_MAX_WINDOWS ||
	    !module_output_queue_reserve(module, allocation))
	{
		fvwm_debug(__func__, "deferred configure queue limit reached");
		module_kill(module);
		return True;
	}
	snapshot = malloc(allocation);
	if (snapshot == NULL)
	{
		module_output_queue_release(module, allocation);
		fvwm_debug(__func__,
			"Configure storage unavailable; retiring module");
		module_kill(module);
		return True;
	}
	snapshot->next = *position;
	snapshot->size = size;
	memcpy(snapshot->data, packet, size);
	*position = snapshot;
	module->xdeferredConfigCount++;
	return True;
}

Bool module_config_has_window(const fmodule *module, Window window)
{
	const fmodule_config_packet *snapshot;

	for (snapshot = module->xdeferredConfig; snapshot != NULL;
	     snapshot = snapshot->next)
	{
		if (snapshot->data[FvwmPacketHeaderSize] == window)
		{
			return True;
		}
	}
	return False;
}

void module_config_discard(fmodule *module, Window window)
{
	fmodule_config_packet **position;

	for (position = &module->xdeferredConfig; *position != NULL;
	     position = &(*position)->next)
	{
		fmodule_config_packet *snapshot = *position;
		if (snapshot->data[FvwmPacketHeaderSize] == window)
		{
			*position = snapshot->next;
			module_output_queue_release(module,
				sizeof(*snapshot) + snapshot->size);
			module->xdeferredConfigCount--;
			free(snapshot);
			return;
		}
	}
}

fmodule_config_packet *module_config_take(fmodule *module)
{
	fmodule_config_packet *snapshot = module->xdeferredConfig;

	if (snapshot != NULL)
	{
		module->xdeferredConfig = snapshot->next;
		module->xdeferredConfigCount--;
		module_output_queue_release(module, sizeof(*snapshot) + snapshot->size);
		snapshot->next = NULL;
	}
	return snapshot;
}

void module_config_dispatch_ready(void)
{
	assert(config_interaction_depth == 0);
	config_command_pending = False;
}

Bool module_config_pending(void)
{
	fmodule_list_itr iterator;
	fmodule *module;

	if (config_command_pending)
	{
		return False;
	}
	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		if (module->xdeferredConfig != NULL)
		{
			module_list_itr_finish(&iterator);
			return True;
		}
	}
	return False;
}

void module_kill_all(void)
{
	while (module_list != NULL)
	{
		module_kill(module_list->module);
	}
	module_cleanup();
}

void module_kill(fmodule *module)
{
	fmodule_store *store;

	if (module == NULL || MOD_IS_RETIRED(module))
	{
		return;
	}
	module->xflags.is_retired = 1;
	/* Retirement must not need new storage while handling refusal. */
	store = module_list_take(module, &module_list);
	if (store != NULL)
	{
		/* Keep the old successor for iterators paused in callbacks. */
		store->retired_next = death_row;
		death_row = store;
	}
}

void module_cleanup(void)
{
	fmodule_store **pos;

	/* A synchronous callback can retire any upcoming list member. */
	if (module_iteration_depth != 0)
	{
		return;
	}
	for (pos = &death_row; *pos != NULL;)
	{
		fmodule_store *current;

		current = *pos;
		if (current->module->xinputRefs != 0)
		{
			pos = &current->retired_next;
			continue;
		}
		*pos = current->retired_next;
		module_destroy(current->module);
		free(current);
	}
}

fmodule_input_status module_receive(
	fmodule *module, fmodule_input **ret_input)
{
	return module_receive_checked(module, ret_input, NULL, NULL);
}

fmodule_input_status module_receive_checked(fmodule *module,
	fmodule_input **ret_input, Bool (*check)(void *), void *context)
{
	unsigned long size;
	unsigned long cont;
	Window win;
	ssize_t n;
	fmodule_input *input;

	*ret_input = NULL;
	while (module->xinputBytes < module->xinputExpected)
	{
		if (check != NULL && !check(context))
		{
			return FMODULE_INPUT_INCOMPLETE;
		}
		n = read(
			MOD_READFD(module),
			module->xinputBuffer + module->xinputBytes,
			module->xinputExpected - module->xinputBytes);
		if (n > 0)
		{
			module->xinputBytes += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
		{
			continue;
		}
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			return FMODULE_INPUT_INCOMPLETE;
		}
		goto err;
	}

	/* A last read can consume the remaining deadline. Keep even a complete
	 * frame in the transport buffer until its owner permits handoff, so a
	 * startup timeout can resume asynchronously without losing a command. */
	if (check != NULL && !check(context))
	{
		return FMODULE_INPUT_INCOMPLETE;
	}

	if (module->xinputExpected == MODULE_INPUT_HEADER_SIZE)
	{
		memcpy(&size, module->xinputBuffer + sizeof(Window), sizeof(size));
		if (size > MAX_MODULE_INPUT_TEXT_LEN)
		{
			fvwm_debug(__func__,
				"Module(%p) command is too big (%lu), limit is %d",
				module, size, MAX_MODULE_INPUT_TEXT_LEN);
			goto err;
		}
		module->xinputExpected =
			MODULE_INPUT_HEADER_SIZE + (size_t)size + sizeof(cont);
		return module_receive_checked(module, ret_input, check, context);
	}

	memcpy(&win, module->xinputBuffer, sizeof(win));
	memcpy(&size, module->xinputBuffer + sizeof(win), sizeof(size));
	memcpy(
		&cont, module->xinputBuffer + MODULE_INPUT_HEADER_SIZE + size,
		sizeof(cont));

	input = malloc(sizeof(*input) + (size_t)size + 1);
	if (input == NULL)
	{
		fvwm_debug(__func__,
			"Command storage unavailable; retiring module");
		goto err;
	}
	input->module = module;
	input->is_queued = False;
	input->in_command_queue = False;
	input->queue_previous = NULL;
	input->queue_next = NULL;
	input->producer_next = NULL;
	module->xinputRefs++;
	input->window = win;
	input->command = (char *)input + sizeof(*input);
	memcpy(
		input->command,
		module->xinputBuffer + MODULE_INPUT_HEADER_SIZE, (size_t)size);
	input->command[size] = '\0';
	module->xinputBytes = 0;
	module->xinputExpected = MODULE_INPUT_HEADER_SIZE;

	if (cont == 0)
	{
		module_kill(module);
	}

	*ret_input = input;
	return FMODULE_INPUT_READY;

err:
	module_kill(module);
	return FMODULE_INPUT_ERROR;
}

Bool module_input_queue_ready(const fmodule *module)
{
	return module->xqueuedInputs < MODULE_INPUT_QUEUE_MAX_COMMANDS;
}

fmodule_input_status module_receive_queued(
	fmodule *module, fmodule_input **ret_input)
{
	fmodule_input_status status;

	*ret_input = NULL;
	if (!module_input_queue_ready(module))
	{
		return FMODULE_INPUT_INCOMPLETE;
	}
	status = module_receive(module, ret_input);
	if (status == FMODULE_INPUT_READY)
	{
		(*ret_input)->is_queued = True;
		module->xqueuedInputs++;
	}

	return status;
}

void module_input_dequeue(fmodule_input *input)
{
	if (input == NULL || !input->is_queued)
	{
		return;
	}
	if (input->module != NULL && input->module->xqueuedInputs > 0)
	{
		input->module->xqueuedInputs--;
	}
	input->is_queued = False;
}

void module_input_discard(fmodule_input *input)
{
	if (input == NULL)
	{
		return;
	}
	assert(!input->in_command_queue);
	module_input_dequeue(input);
	if (input->module != NULL && input->module->xinputRefs > 0)
	{
		input->module->xinputRefs--;
	}
	free(input);
}

void module_list_itr_init(fmodule_list_itr *itr)
{
	itr->next = module_list;
	itr->active = True;
	module_iteration_depth++;
}

void module_list_itr_finish(fmodule_list_itr *itr)
{
	if (itr->active)
	{
		assert(module_iteration_depth > 0);
		module_iteration_depth--;
		itr->active = False;
	}
	itr->next = NULL;
}

fmodule *module_list_itr_next(fmodule_list_itr *itr)
{
	fmodule *module;

	while (itr->next != NULL)
	{
		module = itr->next->module;
		itr->next = itr->next->next;
		if (!MOD_IS_RETIRED(module))
		{
			return module;
		}
	}
	module_list_itr_finish(itr);
	return NULL;
}
