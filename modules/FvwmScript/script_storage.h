#ifndef FVWMSCRIPT_SCRIPT_STORAGE_H
#define FVWMSCRIPT_SCRIPT_STORAGE_H

#include <limits.h>
#include <stddef.h>

#define SCRIPT_STORAGE_DEFAULT_VALUE (1024U * 1024U)
#define SCRIPT_STORAGE_DEFAULT_TOTAL (32U * 1024U * 1024U)
#define SCRIPT_STORAGE_MAX_BYTES INT_MAX

typedef enum
{
	SCRIPT_STORAGE_OK,
	SCRIPT_STORAGE_VALUE_LIMIT,
	SCRIPT_STORAGE_TOTAL_LIMIT
} script_storage_result;

typedef struct
{
	size_t value_limit;
	size_t total_limit;
	size_t used;
} script_storage;

/* Single module instance; parser and interpreter pass its state explicitly. */
script_storage *script_storage_runtime(void);
script_storage_result script_storage_check_value(const script_storage *storage,
	size_t length);
/* old_bytes includes its terminator; zero denotes a newly added table entry.
 * Length excludes the new terminator. Refusal leaves accounting unchanged. */
script_storage_result script_storage_check_replace(const script_storage *storage,
	size_t old_bytes, size_t length);
script_storage_result script_storage_replace(script_storage *storage,
	size_t old_bytes, size_t length);

#endif
