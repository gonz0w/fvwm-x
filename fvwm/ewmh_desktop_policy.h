#ifndef FVWM_EWMH_DESKTOP_POLICY_H
#define FVWM_EWMH_DESKTOP_POLICY_H

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>

/* Bound root-property preparation, not configured FVWM desktop numbers. */
#define FVWM_EWMH_DESKTOP_MAX_COUNT 1024U

static inline bool fvwm_ewmh_desktop_count(unsigned configured, int current,
	int highest_client, unsigned maximum, unsigned long request_words,
	unsigned *result)
{
	uint64_t count = configured ? configured : 1;

	if (result == NULL)
		return false;
	*result = 0;
	if (highest_client >= 0 &&
	    (maximum == 0 || (unsigned)highest_client < maximum) &&
	    (uint64_t)(unsigned)highest_client + 1 > count)
		count = (uint64_t)(unsigned)highest_client + 1;
	if (current >= 0 && (maximum == 0 || (unsigned)current < maximum) &&
	    (uint64_t)(unsigned)current + 1 > count)
		count = (uint64_t)(unsigned)current + 1;
	/* XChangeProperty has six header words; work areas need four per desk. */
	if (request_words < 6 || count > FVWM_EWMH_DESKTOP_MAX_COUNT ||
	    count > (request_words - 6) / 4)
		return false;
	*result = (unsigned)count;
	return true;
}

static inline bool fvwm_ewmh_desktop_request(long value, int current,
	int highest_client, unsigned maximum, unsigned long request_words,
	unsigned *result)
{
	unsigned count;

	if (result == NULL)
		return false;
	*result = 0;
	if (value <= 0 || value > INT_MAX ||
	    (current >= 0 && (unsigned long)current >= (unsigned long)value) ||
	    (highest_client >= 0 &&
	     (unsigned long)highest_client >= (unsigned long)value) ||
	    (maximum != 0 && (unsigned long)value > maximum) ||
	    !fvwm_ewmh_desktop_count((unsigned)value, current, highest_client,
		maximum, request_words, &count) || count != (unsigned long)value)
		return false;
	/* Refuse a populated/current-desktop shrink rather than relocate clients. */
	*result = count;
	return true;
}

static inline bool fvwm_ewmh_desktop_index(long value, unsigned count)
{
	return value >= 0 && (unsigned long)value < count;
}

#endif
