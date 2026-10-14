#ifndef FVWMLIB_IMAGE_PROCESS_H
#define FVWMLIB_IMAGE_PROCESS_H

#include "image_decompress.h"
#include <stdint.h>
#include <poll.h>

typedef struct image_process image_process;
typedef enum
{
	IMAGE_PROCESS_RUNNING,
	IMAGE_PROCESS_SUCCEEDED,
	IMAGE_PROCESS_FAILED
} image_process_status;

typedef bool (*image_process_sink_fn)(const unsigned char *bytes,
	size_t length, void *context);

/* start borrows source and sink context through completion. step never waits:
 * it performs at most one 64 KiB read and write and a WNOHANG child check.
 * poll descriptors/deadline are exposed for the owner's ordinary event loop.
 * SIGCHLD must be waitable (default disposition or fvwmReapChildren), or stay
 * blocked until completion. At most one child may use the FVWM reaper watch.
 * Cancellation closes transport and signals only the still-owned PID/group;
 * the state remains alive until a later step observes its reap. */
image_process *image_process_start(const image_input *source,
	const char *program, char *const argv[], size_t output_limit,
	int timeout_ms, uint64_t memory_limit, image_process_sink_fn sink,
	void *sink_context);
void image_process_descriptors(const image_process *process,
	struct pollfd descriptors[2]);
int image_process_timeout_ms(const image_process *process);
image_process_status image_process_step(image_process *process);
void image_process_cancel(image_process *process);
int image_process_error(const image_process *process);
/* Refuses to free a state whose child has not yet been reaped. */
bool image_process_destroy(image_process *process);
/* Shutdown only: cancel transport and transfer any final reap to the installed
 * FVWM reaper, then free state without waiting. Refuses other dispositions.
 * On success the borrowed source/sink may be released immediately. */
bool image_process_handoff(image_process *process);

/* Checked literal-argv process transport. The sink validates protocol headers
 * before allocating pixels; its context remains owned by the caller. A nonzero
 * memory limit precedes exec and preserves stricter inherited limits.
 * Parent callbacks must not recursively dispatch commands or X lifecycle work.
 * Synchronous adapter for utilities/modules. It owns the temporary SIGCHLD
 * mask/disposition and uses the same start/step transport. */
bool image_process_run(const image_input *source, const char *program,
	char *const argv[], size_t output_limit, int timeout_ms,
	uint64_t memory_limit, image_process_sink_fn sink, void *sink_context,
	image_decompress_cancel_fn cancel, void *cancel_context);

#endif
