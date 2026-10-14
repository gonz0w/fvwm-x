#ifndef FVWM_ICONMAN_PACKET_H
#define FVWM_ICONMAN_PACKET_H

#include <stdbool.h>
#include <stddef.h>

/* Declared-body admission for the fields consumed by this module. */
bool iconman_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t body_words);

/* Decimal native property text denotes an X11 32-bit resource ID. */
bool iconman_parse_window_id(const char *text, unsigned long *result);

#endif
