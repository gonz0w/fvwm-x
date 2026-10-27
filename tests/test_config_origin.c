/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/config_origin.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	fvwm_config_origin *root, *child, *held, *derived, *bad;
	char large[FVWM_CONFIG_SOURCE_MAX_BYTES + 2];
	unsigned int i;

	root = fvwm_config_origin_file("/root.cfg", 2, 4, NULL);
	child = fvwm_config_origin_file("/child.cfg", 8, 9, root);
	held = fvwm_config_origin_retain(child);
	fvwm_config_origin_release(root);
	fvwm_config_origin_release(child);
	assert(strstr(fvwm_config_origin_json(held),
		"\"include\":[{\"path\":\"/root.cfg\",\"first\":2,\"last\":4}]") != NULL);
	derived = fvwm_config_origin_derive(held, "invocation", "unknown definition");
	fvwm_config_origin_release(held);
	assert(strstr(fvwm_config_origin_json(derived),
		"\"kind\":\"invocation\"") != NULL);
	assert(strstr(fvwm_config_origin_json(derived),
		"\"path\":\"/child.cfg\"") != NULL);
	child = fvwm_config_origin_file("/from-function", 1, 1, derived);
	assert(strstr(fvwm_config_origin_json(child), "unavailable") != NULL);
	fvwm_config_origin_release(child);
	fvwm_config_origin_release(derived);

	bad = fvwm_config_origin_file("relative", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	child = fvwm_config_origin_file("/child", 1, 1, bad);
	assert(strstr(fvwm_config_origin_json(child), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	fvwm_config_origin_release(child);
	bad = fvwm_config_origin_file("/bad\xc0\xaf", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	bad = fvwm_config_origin_file("/bad\npath", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	bad = fvwm_config_origin_file("/bad\xc2\x85path", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	bad = fvwm_config_origin_file("/range", 1, (size_t)UINT32_MAX + 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	memset(large, 'x', sizeof(large));
	large[0] = '/';
	large[sizeof(large) - 1] = 0;
	bad = fvwm_config_origin_file(large, 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	/* An individually bounded path can still exceed the complete JSON wire
	 * bound; refusal must not publish a truncated exact source. */
	large[FVWM_CONFIG_SOURCE_MAX_BYTES - 32] = 0;
	bad = fvwm_config_origin_file(large, 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);

	root = NULL;
	for (i = 0; i < FVWM_CONFIG_SOURCE_MAX_INCLUDE + 1; i++)
	{
		child = fvwm_config_origin_file("/x", 1, 1, root);
		fvwm_config_origin_release(root);
		root = child;
		assert(strstr(fvwm_config_origin_json(root), "\"kind\":\"file\"") != NULL);
	}
	child = fvwm_config_origin_file("/overflow", 1, 1, root);
	assert(strstr(fvwm_config_origin_json(child), "unavailable") != NULL);
	fvwm_config_origin_release(root);
	fvwm_config_origin_release(child);
	root = fvwm_config_origin_file("/quote\"slash\\é", 1, 2, NULL);
	assert(strstr(fvwm_config_origin_json(root), "\\u0022slash\\u005c") != NULL);
	assert(strlen(fvwm_config_origin_json(root)) <= FVWM_CONFIG_SOURCE_MAX_BYTES);
	fvwm_config_origin_release(root);
	return 0;
}
