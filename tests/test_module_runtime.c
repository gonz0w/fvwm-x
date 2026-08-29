#include "fvwm/module_list.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

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

int main(void)
{
	test_fragmented_frame();
	test_inputs_pin_dead_module();
	test_quit_frame_is_pinned();
	test_invalid_frames_kill_module();
	test_output_queue_accounting();

	return 0;
}
