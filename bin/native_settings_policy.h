#ifndef FVWM_NATIVE_SETTINGS_POLICY_H
#define FVWM_NATIVE_SETTINGS_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NS_LINE_MAX 2048U
#define NS_TEXT_MAX 128U
#define NS_RESOURCE_MAX 65536U
#define NS_BLOCK_MAX 512U
#define NS_KEY_COUNT 5U

typedef struct
{
	char desktop[33], run[33];
	uint64_t sequence;
	unsigned int motion, cursor_size;
	char font[129], icon[129], cursor[129];
} ns_request;

typedef enum
{
	NS_ACCEPT, NS_DUPLICATE, NS_STALE
} ns_admission;

typedef struct
{
	bool initialized;
	ns_request current;
	uint32_t serial, changed[NS_KEY_COUNT];
} ns_model;

typedef struct
{
	size_t start, length;
	char run[33];
	uint32_t owner;
	bool separator;
	ns_request values;
} ns_block;

typedef enum
{
	NS_RESOURCE_OK, NS_RESOURCE_UNSAFE, NS_RESOURCE_LIMIT
} ns_resource_result;

bool ns_parse(const char *line, size_t length, ns_request *request);
bool ns_values_equal(const ns_request *a, const ns_request *b);
ns_admission ns_admit(const ns_model *model, const ns_request *request);
/* Commit a previously admitted request. Return whether X bytes changed. */
bool ns_commit(ns_model *model, const ns_request *request);
size_t ns_encode(const ns_model *model, unsigned char *output, size_t capacity);
/* No NUL, ambiguous markers or unsafe final logical-line continuation. */
ns_resource_result ns_resource_scan(const char *data, size_t length,
	ns_block *block, bool *found);
size_t ns_resource_block(const ns_request *request, uint32_t owner,
	bool separator, char *output, size_t capacity);
/* Updates demand an exact suffix; cleanup permits an unchanged block in
 * the middle, preserving every external byte on either side. */
bool ns_resource_owned(const char *data, size_t length, const char *owned,
	size_t owned_length, bool cleanup, size_t *start);

#endif
