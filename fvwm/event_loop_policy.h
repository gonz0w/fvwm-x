/* -*-c-*- */

#ifndef FVWM_EVENT_LOOP_POLICY_H
#define FVWM_EVENT_LOOP_POLICY_H

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include "libs/fqueue.h"

/* Yield often enough that resize floods cannot monopolize module/timer work. */
#define FVWM_X_EVENT_BUDGET 16
#define FVWM_MODULE_COMMAND_BUDGET 64
#define FVWM_SCHEDULE_COMMAND_BUDGET 64
#define FVWM_INTERACTION_COMMAND_BUDGET 8
#define FVWM_MODULE_OUTPUT_BYTE_BUDGET (64U * 1024U)
#define FVWM_MODULE_OUTPUT_PACKET_BUDGET 32U
#define FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET 128U

typedef struct
{
	size_t bytes;
	unsigned int packets;
	unsigned int attempts;
} fvwm_output_budget;

void fvwm_output_budget_init(fvwm_output_budget *budget);
/* Reserve one write attempt, including one that returns EINTR/EAGAIN.
 * A zero request ends the pass. Successful bytes and packets count separately. */
size_t fvwm_output_budget_request(fvwm_output_budget *budget, size_t pending);
void fvwm_output_budget_written(fvwm_output_budget *budget, size_t bytes);
void fvwm_output_budget_completed(fvwm_output_budget *budget);

typedef struct
{
	uint64_t previous_ns;
	uint64_t expires_ns;
} fvwm_module_deadline;

typedef enum
{
	FVWM_MODULE_DEADLINE_INVALID = -1,
	FVWM_MODULE_DEADLINE_EXPIRED = 0,
	FVWM_MODULE_DEADLINE_READY = 1
} fvwm_module_deadline_status;

/* UINT64_MAX denotes a failed clock read. Invalid/regressing clocks fail
 * closed; readiness and nested commands never extend this operation's limit. */
int fvwm_module_deadline_init(fvwm_module_deadline *deadline,
	uint64_t now_ns, int seconds);
fvwm_module_deadline_status fvwm_module_deadline_remaining(
	fvwm_module_deadline *deadline, uint64_t now_ns,
	struct timeval *remaining);

typedef void (*fvwm_queue_consumer_t)(void *object, void *context);
typedef int (*fvwm_queue_ready_t)(void *object, void *context);

typedef struct
{
	unsigned int depth;
	unsigned int remaining;
} fvwm_queue_drain_state;

#define FVWM_QUEUE_DRAIN_STATE_INIT { 0, 0 }

typedef enum
{
	FVWM_X_EVENT_NONE,
	FVWM_X_EVENT_DELIVER,
	FVWM_X_EVENT_POLL
} fvwm_x_event_action;

int fvwm_bounded_queue_drain(
	fqueue *queue, fvwm_queue_drain_state *state, unsigned int budget,
	fvwm_queue_consumer_t consume, void *context);
void fvwm_queue_drain_after_poll(
	fvwm_queue_drain_state *state, unsigned int budget);
/* The caller replenishes remaining only after polling descriptors. Remove
 * each ready head before invoking callbacks, which may recurse or cancel jobs.
 * The queue must not be locked across this call; ready is a read-only
 * predicate. Consumers own detached objects and may mutate the queue. */
void fvwm_ready_queue_drain(
	fqueue *queue, unsigned int *remaining, fvwm_queue_ready_t ready,
	fvwm_queue_consumer_t consume, void *context);
fvwm_x_event_action fvwm_x_event_policy(
	int event_pending, unsigned int *consecutive_events);

#endif
