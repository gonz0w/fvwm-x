#ifndef FVWMSCRIPT_SCRIPT_PACKET_H
#define FVWMSCRIPT_SCRIPT_PACKET_H

#include <stdbool.h>
#include <stddef.h>

/* Validate only the body fields consumed by Script and Swallow widgets. */
bool script_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t body_words);

#endif
