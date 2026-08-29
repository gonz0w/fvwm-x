/* -*-c-*- */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libs/log.h"
#include "libs/safemalloc.h"
#include "module_list.h"

#define MODULE_INPUT_HEADER_SIZE (sizeof(Window) + sizeof(unsigned long))

static fmodule_list module_list;
static fmodule_list death_row;

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
	*list = new_store;
}

static fmodule *module_list_remove(fmodule *module, fmodule_list *list)
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
			free(current);
			return module;
		}
	}

	return NULL;
}

fmodule *module_create(void)
{
	fmodule *module;

	module = fxmalloc(sizeof(*module));
	MOD_SET_CMDLINE(module, 0);
	MOD_READFD(module) = -1;
	MOD_WRITEFD(module) = -1;
	fqueue_init(&MOD_PIPEQUEUE(module));
	MOD_PIPEQUEUE_BYTES(module) = 0;
	module->xinputBytes = 0;
	module->xinputExpected = MODULE_INPUT_HEADER_SIZE;
	module->xinputRefs = 0;
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
	module_list_insert(
		module_list_remove(module, &module_list), &death_row);
}

void module_cleanup(void)
{
	fmodule_store **pos;

	for (pos = &death_row; *pos != NULL;)
	{
		fmodule_store *current;

		current = *pos;
		if (current->module->xinputRefs != 0)
		{
			pos = &current->next;
			continue;
		}
		*pos = current->next;
		module_destroy(current->module);
		free(current);
	}
}

fmodule_input_status module_receive(
	fmodule *module, fmodule_input **ret_input)
{
	unsigned long size;
	unsigned long cont;
	Window win;
	ssize_t n;
	fmodule_input *input;

	*ret_input = NULL;
	while (module->xinputBytes < module->xinputExpected)
	{
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
		return module_receive(module, ret_input);
	}

	memcpy(&win, module->xinputBuffer, sizeof(win));
	memcpy(&size, module->xinputBuffer + sizeof(win), sizeof(size));
	memcpy(
		&cont, module->xinputBuffer + MODULE_INPUT_HEADER_SIZE + size,
		sizeof(cont));

	input = fxmalloc(sizeof(*input) + (size_t)size + 1);
	input->module = module;
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

void module_input_discard(fmodule_input *input)
{
	if (input == NULL)
	{
		return;
	}
	if (input->module != NULL && input->module->xinputRefs > 0)
	{
		input->module->xinputRefs--;
	}
	free(input);
}

void module_list_itr_init(fmodule_list_itr *itr)
{
	*itr = module_list;
}

fmodule *module_list_itr_next(fmodule_list_itr *itr)
{
	fmodule *module;

	if (*itr == NULL)
	{
		return NULL;
	}
	module = (*itr)->module;
	*itr = (*itr)->next;

	return module;
}
