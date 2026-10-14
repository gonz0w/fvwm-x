/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libs/cJSON.h"
#include "libs/json_print_policy.h"

static size_t allocation_attempts, fail_at, live_allocations;

static void *tracked_allocate(size_t size)
{
	if (allocation_attempts++ == fail_at)
	{
		return NULL;
	}
	void *memory = malloc(size);
	assert(memory != NULL);
	live_allocations++;
	return memory;
}

static void tracked_free(void *memory)
{
	if (memory != NULL)
	{
		assert(live_allocations > 0);
		live_allocations--;
		free(memory);
	}
}

static cJSON *make_record(void)
{
	cJSON *record = cJSON_CreateObject();
	cJSON *geometry = cJSON_CreateObject();

	if (record == NULL || geometry == NULL ||
	    cJSON_AddStringToObject(record, "name", "quote\"\\\n\t\001 caf\303\251")
		== NULL ||
	    cJSON_AddRawToObject(record, "epoch", "18446744073709551615")
		== NULL ||
	    cJSON_AddNumberToObject(geometry, "x", -32768) == NULL ||
	    cJSON_AddNumberToObject(geometry, "width", 65535) == NULL ||
	    !cJSON_AddItemToObject(record, "geometry", geometry))
	{
		cJSON_Delete(geometry);
		cJSON_Delete(record);
		return NULL;
	}
	return record;
}

static void test_escaping_and_wire_numbers(void)
{
	cJSON *record = make_record();
	assert(record != NULL);
	char *encoded = cJSON_PrintUnformatted(record);
	assert(encoded != NULL);
	assert(strcmp(encoded,
		"{\"name\":\"quote\\\"\\\\\\n\\t\\u0001 caf\303\251\","
		"\"epoch\":18446744073709551615,"
		"\"geometry\":{\"x\":-32768,\"width\":65535}}") == 0);
	puts(encoded);
	cJSON_free(encoded);
	cJSON_Delete(record);
	const double values[] = { 0.0, -0.0, INT_MIN, INT_MAX, 0.25,
		DBL_MIN, DBL_MAX, -DBL_MAX, INFINITY, NAN };
	for (size_t i = 0; i < sizeof(values) / sizeof(*values); i++)
	{
		cJSON *number = cJSON_CreateNumber(values[i]);
		assert(number != NULL);
		if (isnan(values[i]))
		{
			assert(number->valueint == 0);
			cJSON_SetNumberValue(number, NAN);
			assert(number->valueint == 0 && isnan(number->valuedouble));
		}
		encoded = cJSON_PrintUnformatted(number);
		assert(encoded != NULL);
		if (!isfinite(values[i]))
		{
			assert(strcmp(encoded, "null") == 0);
		}
		else
		{
			double parsed = strtod(encoded, NULL);
			assert(isfinite(parsed));
			assert(fabs(parsed - values[i]) <=
				fabs(values[i]) * DBL_EPSILON * 4);
		}
		puts(encoded);
		cJSON_free(encoded);
		cJSON_Delete(number);
	}
}

static void test_large_escaped_string_and_fixed_buffer(void)
{
	char input[4097];
	for (size_t i = 0; i < sizeof(input) - 1; i++)
	{
		input[i] = (char)(i % 31 + 1);
	}
	input[sizeof(input) - 1] = '\0';
	cJSON *string = cJSON_CreateString(input);
	assert(string != NULL);
	char *encoded = cJSON_PrintUnformatted(string);
	assert(encoded != NULL && strlen(encoded) > strlen(input));
	cJSON *round_trip = cJSON_Parse(encoded);
	assert(round_trip && strcmp(round_trip->valuestring, input) == 0);
	/* Parsing here checks only our generated output, not a runtime input. */
	char tiny[8];
	memset(tiny, 0x5a, sizeof(tiny));
	assert(!cJSON_PrintPreallocated(string, tiny, 4, 0));
	assert(tiny[4] == 0x5a && tiny[7] == 0x5a);
	cJSON_Delete(round_trip);
	cJSON_free(encoded);
	cJSON_Delete(string);
	puts("bounded-escaped-round-trip: passed");
}

static void test_utf8_boundary(void)
{
	const struct { const char *input, *encoded; } cases[] = {
		{ "ascii", "ascii" },
		{ "\302\200\337\277\340\240\200\354\277\277"
		  "\356\200\200\357\277\277\360\220\200\200"
		  "\364\217\277\277",
		  "\302\200\337\277\340\240\200\354\277\277"
		  "\356\200\200\357\277\277\360\220\200\200"
		  "\364\217\277\277" },
		{ "\200\277\300\301\365\377",
		  "\\ufffd\\ufffd\\ufffd\\ufffd\\ufffd\\ufffd" },
		{ "\340\237\277", "\\ufffd\\ufffd\\ufffd" },
		{ "\355\240\200", "\\ufffd\\ufffd\\ufffd" },
		{ "\360\217\277\277", "\\ufffd\\ufffd\\ufffd\\ufffd" },
		{ "\364\220\200\200", "\\ufffd\\ufffd\\ufffd\\ufffd" },
		{ "\302", "\\ufffd" },
		{ "\342\202", "\\ufffd\\ufffd" },
		{ "\360\237\230", "\\ufffd\\ufffd\\ufffd" },
		{ "\342x\202", "\\ufffdx\\ufffd" },
		{ "\377caf\303\251\"\n",
		  "\\ufffdcaf\303\251\\\"\\n" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++)
	{
		/* Exact allocations expose lookahead past truncated terminators. */
		size_t length = strlen(cases[i].input);
		char *input = malloc(length + 1);
		assert(input != NULL);
		memcpy(input, cases[i].input, length + 1);
		cJSON *object = cJSON_CreateObject();
		assert(object && cJSON_AddStringToObject(object, input, input));
		char expected[512];
		int written = snprintf(expected, sizeof(expected),
			"{\"%s\":\"%s\"}", cases[i].encoded, cases[i].encoded);
		assert(written >= 0 && (size_t)written < sizeof(expected));
		char *encoded = cJSON_PrintUnformatted(object);
		assert(encoded && strcmp(encoded, expected) == 0);
		assert(strcmp(object->child->string, input) == 0);
		assert(strcmp(object->child->valuestring, input) == 0);
		cJSON *parsed = cJSON_Parse(encoded);
		assert(parsed != NULL);
		cJSON_Delete(parsed);
		cJSON_free(encoded);
		encoded = cJSON_Print(object);
		assert(encoded != NULL);
		parsed = cJSON_Parse(encoded);
		assert(parsed != NULL);
		cJSON_Delete(parsed);
		cJSON_free(encoded);
		char fixed[512];
		assert(cJSON_PrintPreallocated(object, fixed, sizeof(fixed), 0));
		assert(strcmp(fixed, expected) == 0);
		cJSON_Delete(object);
		free(input);
	}
	for (unsigned int byte = 1; byte <= 255; byte++)
	{
		char input[] = { (char)byte, 0 };
		cJSON *string = cJSON_CreateString(input);
		assert(string != NULL);
		char *encoded = cJSON_PrintBuffered(string, 1, 0);
		assert(encoded != NULL);
		cJSON *parsed = cJSON_Parse(encoded);
		assert(parsed != NULL);
		assert(strcmp(parsed->valuestring,
			byte < 128 ? input : "\357\277\275") == 0);
		assert(strcmp(string->valuestring, input) == 0);
		cJSON_Delete(parsed);
		cJSON_free(encoded);
		cJSON_Delete(string);
	}
	puts("utf8-keys-values-preserve-storage: passed");
}

static void test_checked_print_sizes(void)
{
	size_t result = 17;
	assert(json_print_size_add(INT_MAX - 6, 6, &result));
	assert(result == INT_MAX);
	assert(!json_print_size_add(INT_MAX, 1, &result));
	assert(result == INT_MAX);
	assert(!json_print_size_add(SIZE_MAX, 0, &result));
	assert(!json_print_size_add(1, SIZE_MAX, &result));
	assert(json_print_size_add(0, 0, &result) && result == 0);
	puts("checked-expansion-offset-sizes: passed");
}

static cJSON *make_legacy_record(void)
{
	cJSON *record = cJSON_CreateObject();
	if (record == NULL ||
	    cJSON_AddStringToObject(record, "key\377", "cut\342\202") == NULL)
	{
		cJSON_Delete(record);
		return NULL;
	}
	return record;
}

static void test_allocation_failures(void)
{
	cJSON_Hooks hooks = { tracked_allocate, tracked_free };
	for (unsigned int kind = 0; kind < 2; kind++)
	{
		bool succeeded = false;
		for (fail_at = 0; fail_at < 64; fail_at++)
		{
			allocation_attempts = live_allocations = 0;
			cJSON_InitHooks(&hooks);
			cJSON *record = kind ? make_legacy_record() : make_record();
			char *encoded = record ? cJSON_PrintUnformatted(record) : NULL;
			succeeded = encoded != NULL;
			cJSON_free(encoded);
			cJSON_Delete(record);
			assert(live_allocations == 0);
			cJSON_InitHooks(NULL);
			if (succeeded)
			{
				break;
			}
		}
		assert(succeeded && fail_at > 3);
		printf("allocation-failure-cleanup[%u]: %zu positions passed\n",
			kind, fail_at);
	}
}

int main(void)
{
	test_escaping_and_wire_numbers();
	test_large_escaped_string_and_fixed_buffer();
	test_allocation_failures();
	test_utf8_boundary();
	test_checked_print_sizes();
	return 0;
}
