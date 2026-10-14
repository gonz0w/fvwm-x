/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include "libs/fvwmlib.h"
#include "libs/System.h"

static Bool query_active;
static long query_value;
static unsigned query_calls;
#if HAVE_SYSCONF
long __real_sysconf(int key);
long __wrap_sysconf(int key)
{
	if (query_active && key == _SC_OPEN_MAX)
	{
		query_calls++;
		return query_value;
	}
	return __real_sysconf(key);
}
#else
int __real_getdtablesize(void);
int __wrap_getdtablesize(void)
{
	if (query_active)
	{
		query_calls++;
		return (int)query_value;
	}
	return __real_getdtablesize();
}
#endif

static void test_query_bounds(void)
{
	static const long limits[] = { -1, 0, 1, FD_SETSIZE - 1,
		FD_SETSIZE, FD_SETSIZE + 1, INT_MAX,
#if HAVE_SYSCONF
		LONG_MAX,
#endif
	};

	query_active = True;
	for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); i++)
	{
		query_value = limits[i];
		query_calls = 0;
		fd_set_size_t expected = query_value <= 0 ||
			query_value > FD_SETSIZE ? FD_SETSIZE : query_value;

		assert(GetFdWidth() == expected && query_calls == 1);
		query_calls = 0;
		fvwmlib_init_max_fd();
		assert(fvwmlib_max_fd == expected && query_calls == 1);
	}
	query_active = False;
}

static void test_native_query(void)
{
	fd_set_size_t width = GetFdWidth();

	assert(width > 0 && width <= FD_SETSIZE);
	fvwmlib_init_max_fd();
	assert(fvwmlib_max_fd > 0 && fvwmlib_max_fd <= FD_SETSIZE);
}

int main(void)
{
	test_query_bounds();
	test_native_query();
	puts("descriptor width: single query, finite fallback and native limit pass");
	return 0;
}
