#ifndef FVWM_BUTTONS_PACKET_H
#define FVWM_BUTTONS_PACKET_H

#include <stdbool.h>
#include <stddef.h>

/* Declared-body admission for the fields consumed by this module. */
bool buttons_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t body_words);

#endif
