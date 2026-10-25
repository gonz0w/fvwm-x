#include "config.h"
#include "fvwm/module_records.h"
#include "fvwm/appearance_preview.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static Bool refuse_malloc;
static unsigned allocation_attempts;
static Bool observe_owner_notifications;
static fmodule *expected_preview_owner;
static unsigned owner_notifications;
static Bool notified_owner_retired;

/* The preview subsystem is omitted from this fixture. Observe only the
 * runtime's owner notification and the owner's lifetime during the call. */
void appearance_preview_owner_gone(struct fmodule *owner)
{
	if (observe_owner_notifications)
	{
		assert(owner == expected_preview_owner);
		owner_notifications++;
		notified_owner_retired = MOD_IS_RETIRED(owner);
	}
}

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
	if (refuse_malloc)
	{
		allocation_attempts++;
		return NULL;
	}
	return __real_malloc(size);
}

static fmodule *create_pipe_module(int *read_fd, int *write_fd)
{
	fmodule *module;
	int pipe_fds[2];
	int flags;

	assert(pipe(pipe_fds) == 0);
	flags = fcntl(pipe_fds[0], F_GETFL);
	assert(flags >= 0);
	assert(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

	module = module_create();
	MOD_READFD(module) = pipe_fds[0];
	module_register(module);
	*read_fd = pipe_fds[0];
	*write_fd = pipe_fds[1];

	return module;
}

static size_t make_frame(
	unsigned char *frame, Window window, const char *command,
	unsigned long cont)
{
	unsigned long size;
	size_t offset;

	size = strlen(command);
	offset = 0;
	memcpy(frame + offset, &window, sizeof(window));
	offset += sizeof(window);
	memcpy(frame + offset, &size, sizeof(size));
	offset += sizeof(size);
	memcpy(frame + offset, command, size);
	offset += size;
	memcpy(frame + offset, &cont, sizeof(cont));
	offset += sizeof(cont);

	return offset;
}

static void write_all(int fd, const void *data, size_t size)
{
	const unsigned char *bytes;
	size_t written;

	bytes = data;
	for (written = 0; written < size;)
	{
		ssize_t n;

		n = write(fd, bytes + written, size - written);
		if (n < 0 && errno == EINTR)
		{
			continue;
		}
		assert(n > 0);
		written += (size_t)n;
	}
}

static void assert_fd_open(int fd)
{
	assert(fcntl(fd, F_GETFD) >= 0);
}

static void assert_fd_closed(int fd)
{
	errno = 0;
	assert(fcntl(fd, F_GETFD) == -1);
	assert(errno == EBADF);
}

static void test_owner_notifications(void)
{
	fmodule *module = module_create();

	module_register(module);
	module->xinputRefs = 1;
	expected_preview_owner = module;
	owner_notifications = 0;
	observe_owner_notifications = True;
	module_kill(module);
	assert(owner_notifications == 1 && !notified_owner_retired);
	assert(MOD_IS_RETIRED(module));
	module_kill(module);
	assert(owner_notifications == 1);
	module_cleanup();
	assert(owner_notifications == 1);
	module->xinputRefs = 0;
	module_cleanup();
	/* Final destruction notifies again; preview recovery handles duplicates. */
	assert(owner_notifications == 2 && notified_owner_retired);
	observe_owner_notifications = False;
	expected_preview_owner = NULL;

	module = module_create();
	expected_preview_owner = module;
	owner_notifications = 0;
	observe_owner_notifications = True;
	module_destroy(module);
	assert(owner_notifications == 1 && !notified_owner_retired);
	observe_owner_notifications = False;
	expected_preview_owner = NULL;
}

static void test_fragmented_frame(void)
{
	unsigned char frame[128];
	fmodule_input *input;
	fmodule *module;
	size_t frame_size;
	size_t i;
	int read_fd;
	int write_fd;
	Window window;

	window = (Window)0x1234;
	module = create_pipe_module(&read_fd, &write_fd);
	assert(module_receive(module, &input) == FMODULE_INPUT_INCOMPLETE);
	assert(input == NULL);

	frame_size = make_frame(frame, window, "Echo fragmented", 1);
	for (i = 0; i < frame_size; i++)
	{
		write_all(write_fd, frame + i, 1);
		if (i + 1 == frame_size)
		{
			assert(module_receive(module, &input) == FMODULE_INPUT_READY);
		}
		else
		{
			assert(
				module_receive(module, &input) ==
				FMODULE_INPUT_INCOMPLETE);
			assert(input == NULL);
		}
	}
	assert(input->window == window);
	assert(strcmp(input->command, "Echo fragmented") == 0);
	assert(module->xinputRefs == 1);

	module_input_discard(input);
	module_kill(module);
	module_cleanup();
	assert_fd_closed(read_fd);
	close(write_fd);
}

static void test_inputs_pin_dead_module(void)
{
	unsigned char frames[256];
	fmodule_input *first;
	fmodule_input *second;
	fmodule *module;
	size_t first_size;
	size_t second_size;
	int read_fd;
	int write_fd;

	module = create_pipe_module(&read_fd, &write_fd);
	first_size = make_frame(frames, (Window)1, "Echo first", 1);
	second_size = make_frame(
		frames + first_size, (Window)2, "Echo second", 1);
	write_all(write_fd, frames, first_size + second_size);

	assert(module_receive(module, &first) == FMODULE_INPUT_READY);
	assert(module_receive(module, &second) == FMODULE_INPUT_READY);
	assert(module->xinputRefs == 2);
	module_kill(module);
	module_cleanup();
	assert_fd_open(read_fd);

	module_input_discard(first);
	module_cleanup();
	assert_fd_open(read_fd);
	assert(module->xinputRefs == 1);

	module_input_discard(second);
	module_cleanup();
	assert_fd_closed(read_fd);
	close(write_fd);
}

static void test_quit_frame_is_pinned(void)
{
	unsigned char frame[128];
	fmodule_input *input;
	fmodule *module;
	size_t frame_size;
	int read_fd;
	int write_fd;

	module = create_pipe_module(&read_fd, &write_fd);
	frame_size = make_frame(frame, None, "Nop", 0);
	write_all(write_fd, frame, frame_size);
	assert(module_receive(module, &input) == FMODULE_INPUT_READY);

	module_cleanup();
	assert_fd_open(read_fd);
	module_input_discard(input);
	module_cleanup();
	assert_fd_closed(read_fd);
	close(write_fd);
}

static void test_invalid_frames_kill_module(void)
{
	unsigned char header[sizeof(Window) + sizeof(unsigned long)];
	fmodule_input *input;
	fmodule *module;
	unsigned long size;
	Window window;
	int read_fd;
	int write_fd;

	window = None;
	size = MAX_MODULE_INPUT_TEXT_LEN + 1UL;
	memcpy(header, &window, sizeof(window));
	memcpy(header + sizeof(window), &size, sizeof(size));

	module = create_pipe_module(&read_fd, &write_fd);
	write_all(write_fd, header, sizeof(header));
	assert(module_receive(module, &input) == FMODULE_INPUT_ERROR);
	assert(input == NULL);
	module_cleanup();
	assert_fd_closed(read_fd);
	close(write_fd);

	module = create_pipe_module(&read_fd, &write_fd);
	close(write_fd);
	assert(module_receive(module, &input) == FMODULE_INPUT_ERROR);
	module_cleanup();
	assert_fd_closed(read_fd);
}

static void test_output_queue_accounting(void)
{
	fmodule *module;

	module = module_create();
	assert(module_output_queue_reserve(module, MODULE_QUEUE_MAX_BYTES));
	assert(MOD_PIPEQUEUE_BYTES(module) == MODULE_QUEUE_MAX_BYTES);
	assert(!module_output_queue_reserve(module, 1));
	assert(MOD_PIPEQUEUE_BYTES(module) == MODULE_QUEUE_MAX_BYTES);

	module_output_queue_release(module, MODULE_QUEUE_MAX_BYTES - 1);
	assert(MOD_PIPEQUEUE_BYTES(module) == 1);
	module_output_queue_release(module, 2);
	assert(MOD_PIPEQUEUE_BYTES(module) == 0);
	assert(!module_output_queue_reserve(
		module, (size_t)MODULE_QUEUE_MAX_BYTES + 1));
	module_destroy(module);
}

static void test_input_queue_backpressure(void)
{
	fmodule_input *queued[MODULE_INPUT_QUEUE_MAX_COMMANDS];
	fmodule_input *input;
	unsigned char frame[128];
	int read_fd, write_fd, other_read, other_write;
	fmodule *module = create_pipe_module(&read_fd, &write_fd);
	fmodule *other = create_pipe_module(&other_read, &other_write);
	size_t size;

	for (unsigned int i = 0; i < MODULE_INPUT_QUEUE_MAX_COMMANDS; i++)
	{
		size = make_frame(frame, (Window)i, "Nop", 1);
		write_all(write_fd, frame, size);
		assert(module_receive_queued(module, &queued[i]) ==
			FMODULE_INPUT_READY);
		assert(queued[i]->window == (Window)i && queued[i]->is_queued);
	}
	assert(module->xqueuedInputs == MODULE_INPUT_QUEUE_MAX_COMMANDS);
	assert(!module_input_queue_ready(module));
	size = make_frame(frame, (Window)999, "Echo retained", 1);
	write_all(write_fd, frame, size);
	assert(module_receive_queued(module, &input) == FMODULE_INPUT_INCOMPLETE);
	assert(input == NULL && module->xinputBytes == 0);
	/* A saturated producer neither consumes bytes nor pauses other modules. */
	write_all(other_write, frame, size);
	assert(module_receive_queued(other, &input) == FMODULE_INPUT_READY);
	assert(strcmp(input->command, "Echo retained") == 0);
	module_input_discard(input);
	assert(other->xqueuedInputs == 0);
	/* Synchronous handshakes retain their direct receive path at capacity. */
	assert(module_receive(module, &input) == FMODULE_INPUT_READY);
	assert(!input->is_queued && input->window == (Window)999);
	module_input_discard(input);
	assert(module->xqueuedInputs == MODULE_INPUT_QUEUE_MAX_COMMANDS);
	size = make_frame(frame, None, ModuleUnlockResponse, 1);
	write_all(write_fd, frame, size);
	assert(module_receive(module, &input) == FMODULE_INPUT_READY);
	assert(strcmp(input->command, ModuleUnlockResponse) == 0);
	module_input_discard(input);
	assert(module->xqueuedInputs == MODULE_INPUT_QUEUE_MAX_COMMANDS);
	size = make_frame(frame, (Window)999, "Echo retained", 1);
	write_all(write_fd, frame, size);
	module_input_dequeue(queued[0]);
	module_input_dequeue(queued[0]);
	assert(module_input_queue_ready(module));
	assert(module->xqueuedInputs == MODULE_INPUT_QUEUE_MAX_COMMANDS - 1);
	assert(module_receive_queued(module, &input) == FMODULE_INPUT_READY);
	assert(strcmp(input->command, "Echo retained") == 0);
	assert(module->xqueuedInputs == MODULE_INPUT_QUEUE_MAX_COMMANDS);
	module_input_discard(input);
	module_kill(module);
	module_cleanup();
	assert_fd_open(read_fd);
	for (unsigned int i = 0; i < MODULE_INPUT_QUEUE_MAX_COMMANDS; i++)
	{
		module_input_discard(queued[i]);
	}
	assert(module->xqueuedInputs == 0 && module->xinputRefs == 0);
	module_cleanup();
	assert_fd_closed(read_fd);
	module_kill(other);
	module_cleanup();
	assert_fd_closed(other_read);
	close(write_fd);
	close(other_write);
}

static void test_deferred_config_snapshots(void)
{
	unsigned long packet[] = { START_FLAG, M_CONFIGURE_WINDOW, 6, 0, 11, 1 };
	fmodule *module = module_create();
	fmodule_config_packet *snapshot;
	size_t allocation = sizeof(*snapshot) + sizeof(packet);

	module_register(module);
	assert(!module_config_defer(module, packet, sizeof(packet)));
	module_config_interaction_begin();
	module_config_interaction_begin();
	for (unsigned int i = 0; i < 4096; i++)
	{
		packet[5] = i;
		assert(module_config_defer(module, packet, sizeof(packet)));
		assert(module->xdeferredConfigCount == 1);
		assert(MOD_PIPEQUEUE_BYTES(module) == allocation);
	}
	packet[4] = 22;
	assert(module_config_defer(module, packet, sizeof(packet)));
	assert(module->xdeferredConfigCount == 2);
	assert(!module_config_pending());
	module_config_interaction_end();
	assert(!module_config_pending());
	module_config_interaction_end();
	assert(!module_config_pending());
	module_config_dispatch_ready();
	assert(module_config_pending());
	/* Final authoritative updates still replace unsent drag snapshots. */
	packet[4] = 11;
	packet[5] = 9000;
	assert(module_config_defer(module, packet, sizeof(packet)));
	packet[5] = 0;
	snapshot = module_config_take(module);
	assert(snapshot->next == NULL && snapshot->data[4] == 11);
	assert(snapshot->data[5] == 9000);
	free(snapshot);
	module_config_discard(module, 99);
	assert(module->xdeferredConfigCount == 1);
	module_config_discard(module, 22);
	assert(module_config_take(module) == NULL);
	assert(MOD_PIPEQUEUE_BYTES(module) == 0);
	assert(!module_config_pending());
	module_kill(module);
	module_cleanup();
}

static void test_deferred_config_limits(void)
{
	unsigned long packet[] = { START_FLAG, M_CONFIGURE_WINDOW, 5, 0, 1 };
	fmodule *module = module_create();

	module_register(module);
	module->xinputRefs = 1;
	module_config_interaction_begin();
	for (unsigned int i = 0; i < MODULE_DEFERRED_CONFIG_MAX_WINDOWS; i++)
	{
		packet[4] = i;
		assert(module_config_defer(module, packet, sizeof(packet)));
	}
	packet[4] = MODULE_DEFERRED_CONFIG_MAX_WINDOWS;
	assert(module_config_defer(module, packet, sizeof(packet)));
	assert(module->xdeferredConfigCount == MODULE_DEFERRED_CONFIG_MAX_WINDOWS);
	module_config_interaction_end();
	module_config_dispatch_ready();
	assert(!module_config_pending()); /* Over-limit module retired. */
	module_cleanup(); /* Lease prevents freeing the retired producer. */
	module->xinputRefs = 0;
	module_cleanup();

	module = module_create();
	module_register(module);
	module->xinputRefs = 1;
	module_config_interaction_begin();
	assert(module_output_queue_reserve(module, MODULE_QUEUE_MAX_BYTES));
	assert(module_config_defer(module, packet, sizeof(packet)));
	assert(module->xdeferredConfigCount == 0);
	module_config_interaction_end();
	module_config_dispatch_ready();
	assert(!module_config_pending());
	module->xinputRefs = 0;
	module_cleanup();
}

static void test_deferred_config_invalid_frames(void)
{
	unsigned long packet[] = { START_FLAG, M_CONFIGURE_WINDOW, 5, 0, 1 };

	module_config_interaction_begin();
	for (unsigned int i = 0; i < 4; i++)
	{
		fmodule *module = module_create();
		module_register(module);
		module->xinputRefs = 1;
		packet[2] = i == 3 ? 99 : 5;
		assert(module_config_defer(module, i == 0 ? NULL : packet,
			i == 1 ? sizeof(unsigned long) : i == 2 ?
			FvwmPacketMaxSize_byte + 1 : sizeof(packet)));
		assert(module->xdeferredConfigCount == 0);
		module->xinputRefs = 0;
		module_cleanup();
	}
	module_config_interaction_end();
	module_config_dispatch_ready();
	assert(!module_config_pending());
}

static Bool receive_check(void *context)
{
	unsigned *remaining = context;

	if (*remaining == 0)
	{
		return False;
	}
	(*remaining)--;
	return True;
}

static void test_checked_receive_and_retirement(void)
{
	int read_fd, write_fd;
	fmodule *module = create_pipe_module(&read_fd, &write_fd);
	unsigned char frame[128];
	size_t size = make_frame(frame, 0, "NOP", 1);
	fmodule_input *input;
	unsigned remaining = 1;

	assert(write(write_fd, frame, size) == (ssize_t)size);
	assert(module_receive_checked(module, &input, receive_check,
		&remaining) == FMODULE_INPUT_INCOMPLETE);
	assert(input == NULL && module->xinputBytes > 0);
	assert(module->xinputRefs == 0);
	assert(module_receive(module, &input) == FMODULE_INPUT_READY);
	assert(strcmp(input->command, "NOP") == 0);
	module_kill(module);
	assert(MOD_IS_RETIRED(module));
	module_kill(module); /* Retirement is idempotent while input pins it. */
	module_cleanup();
	assert(fcntl(read_fd, F_GETFD) >= 0);
	module_input_discard(input);
	module_cleanup();
	assert_fd_closed(read_fd);
	close(write_fd);
}

static void test_complete_frame_declined_handoff(void)
{
	for (unsigned long continuation = 0; continuation <= 1; continuation++)
	{
		int read_fd, write_fd;
		fmodule *module = create_pipe_module(&read_fd, &write_fd);
		unsigned char frame[128];
		size_t size = make_frame(frame, 0, "NOP", continuation);
		fmodule_input *input;
		unsigned remaining = 3;

		assert(write(write_fd, frame, size) == (ssize_t)size);
		assert(module_receive_checked(module, &input, receive_check,
			&remaining) == FMODULE_INPUT_INCOMPLETE);
		assert(input == NULL && module->xinputBytes == size);
		assert(module->xinputExpected == size && module->xinputRefs == 0);
		assert(!MOD_IS_RETIRED(module));
		assert(module_receive_queued(module, &input) == FMODULE_INPUT_READY);
		assert(strcmp(input->command, "NOP") == 0);
		assert(module->xqueuedInputs == 1 && module->xinputRefs == 1);
		assert(MOD_IS_RETIRED(module) == (continuation == 0));
		module_input_discard(input);
		assert(module->xqueuedInputs == 0 && module->xinputRefs == 0);
		module_kill(module);
		module_cleanup();
		assert_fd_closed(read_fd);
		close(write_fd);
	}
}

static void test_retirement_without_allocation(void)
{
	int read_fd, write_fd;
	fmodule *module = create_pipe_module(&read_fd, &write_fd);
	module->xinputRefs++;
	allocation_attempts = 0;
	refuse_malloc = True;
	module_kill(module);
	module_kill(module);
	module_cleanup();
	refuse_malloc = False;
	assert(allocation_attempts == 0);
	assert(MOD_IS_RETIRED(module));
	assert_fd_open(read_fd);
	module->xinputRefs--;
	module_cleanup();
	assert_fd_closed(read_fd);
	close(write_fd);
}

static void test_command_storage_refusal(void)
{
	for (unsigned held = 0; held <= 1; held++)
	{
		int read_fd, write_fd;
		fmodule *module = create_pipe_module(&read_fd, &write_fd);
		unsigned char frame[128];
		size_t size = make_frame(frame, 42, "Echo owned", 1);
		fmodule_input *previous = NULL;
		fmodule_input *input = (fmodule_input *)module;

		if (held)
		{
			write_all(write_fd, frame, size);
			assert(module_receive_queued(module, &previous) ==
				FMODULE_INPUT_READY);
		}
		write_all(write_fd, frame, size);
		allocation_attempts = 0;
		refuse_malloc = True;
		assert(module_receive_queued(module, &input) == FMODULE_INPUT_ERROR);
		refuse_malloc = False;
		assert(input == NULL && allocation_attempts == 1);
		assert(MOD_IS_RETIRED(module));
		assert(module->xinputRefs == held && module->xqueuedInputs == held);
		module_cleanup();
		if (held)
		{
			assert_fd_open(read_fd);
			assert(previous->window == 42);
			assert(strcmp(previous->command, "Echo owned") == 0);
			module_input_discard(previous);
			module_cleanup();
		}
		assert_fd_closed(read_fd);
		close(write_fd);
	}
	/* A new producer still admits a complete command after the refusal. */
	test_fragmented_frame();
}

static void test_iterator_skips_retired_successor(void)
{
	int readers[3], writers[3];
	fmodule *modules[3];
	fmodule_list_itr iterator;

	for (unsigned i = 0; i < 3; i++)
	{
		modules[i] = create_pipe_module(&readers[i], &writers[i]);
	}
	module_list_itr_init(&iterator);
	assert(module_list_itr_next(&iterator) == modules[2]);
	module_kill(modules[1]);
	module_cleanup();
	assert_fd_open(readers[1]);
	assert(module_list_itr_next(&iterator) == modules[0]);
	assert(module_list_itr_next(&iterator) == NULL);
	assert(module_list_itr_next(&iterator) == NULL);
	module_cleanup();
	assert_fd_closed(readers[1]);
	module_kill_all();
	for (unsigned i = 0; i < 3; i++)
	{
		assert_fd_closed(readers[i]);
		close(writers[i]);
	}
}

static void test_nested_iterator_cleanup_and_finish(void)
{
	int readers[2], writers[2];
	fmodule *modules[2];
	fmodule_list_itr outer, inner;

	for (unsigned i = 0; i < 2; i++)
	{
		modules[i] = create_pipe_module(&readers[i], &writers[i]);
	}
	module_list_itr_init(&outer);
	assert(module_list_itr_next(&outer) == modules[1]);
	module_list_itr_init(&inner);
	assert(module_list_itr_next(&inner) == modules[1]);
	module_kill_all();
	assert_fd_open(readers[0]);
	assert_fd_open(readers[1]);
	assert(module_list_itr_next(&inner) == NULL);
	module_cleanup();
	assert_fd_open(readers[0]);
	assert_fd_open(readers[1]);
	module_list_itr_finish(&outer);
	module_list_itr_finish(&outer);
	module_cleanup();
	for (unsigned i = 0; i < 2; i++)
	{
		assert_fd_closed(readers[i]);
		close(writers[i]);
	}
}

static void test_iterator_defers_new_registration(void)
{
	int readers[3], writers[3];
	fmodule *modules[3];
	fmodule_list_itr iterator;

	modules[0] = create_pipe_module(&readers[0], &writers[0]);
	modules[1] = create_pipe_module(&readers[1], &writers[1]);
	module_list_itr_init(&iterator);
	assert(module_list_itr_next(&iterator) == modules[1]);
	modules[2] = create_pipe_module(&readers[2], &writers[2]);
	assert(module_list_itr_next(&iterator) == modules[0]);
	assert(module_list_itr_next(&iterator) == NULL);
	module_list_itr_init(&iterator);
	for (unsigned i = 3; i > 0; i--)
	{
		assert(module_list_itr_next(&iterator) == modules[i - 1]);
	}
	assert(module_list_itr_next(&iterator) == NULL);
	module_kill_all();
	for (unsigned i = 0; i < 3; i++)
	{
		assert_fd_closed(readers[i]);
		close(writers[i]);
	}
	module_list_itr_init(&iterator);
	assert(module_list_itr_next(&iterator) == NULL);
}

int main(void)
{
	test_owner_notifications();
	test_iterator_skips_retired_successor();
	test_nested_iterator_cleanup_and_finish();
	test_iterator_defers_new_registration();
	test_retirement_without_allocation();
	test_command_storage_refusal();
	test_complete_frame_declined_handoff();
	test_checked_receive_and_retirement();
	test_deferred_config_snapshots();
	test_deferred_config_limits();
	test_deferred_config_invalid_frames();
	test_fragmented_frame();
	test_inputs_pin_dead_module();
	test_quit_frame_is_pinned();
	test_invalid_frames_kill_module();
	test_output_queue_accounting();
	test_input_queue_backpressure();

	return 0;
}
