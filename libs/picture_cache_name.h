#ifndef FVWM_PICTURE_CACHE_NAME_H
#define FVWM_PICTURE_CACHE_NAME_H

#include <stdint.h>

/* A rejection key only: hash matches still require the complete name. */
static inline uint64_t picture_cache_name_hash(const char *name)
{
	const unsigned char *cursor = (const unsigned char *)name;
	uint64_t hash = UINT64_C(14695981039346656037);

	while (*cursor != '\0')
	{
		hash ^= *cursor++;
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

#endif
