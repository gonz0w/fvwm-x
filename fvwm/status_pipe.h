#ifndef FVWM_STATUS_PIPE_H
#define FVWM_STATUS_PIPE_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define FVWM_STATUS_MAX_RECORD (1024 * 1024)

typedef struct
{
	int fd;
	char *path;
	dev_t device;
	ino_t inode;
	char *pending;
	size_t length;
	size_t offset;
} fvwm_status_pipe;

#define FVWM_STATUS_PIPE_INIT { .fd = -1 }

/* Own only a newly created FIFO. Existing paths are never replaced. */
bool status_pipe_open(fvwm_status_pipe *pipe, const char *directory);
void status_pipe_close(fvwm_status_pipe *pipe);
/* One bounded record, including its newline. Busy queues refuse new records.
 * Flush returns 1 when empty, 0 for deferred work, -1 for a permanent error.
 * The caller polls pending output and closes on a permanent error. */
bool status_pipe_queue(fvwm_status_pipe *pipe, const char *text, size_t length);
int status_pipe_flush(fvwm_status_pipe *pipe);

#endif
