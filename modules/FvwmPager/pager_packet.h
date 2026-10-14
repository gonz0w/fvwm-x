#ifndef FVWM_PAGER_PACKET_H
#define FVWM_PAGER_PACKET_H

#include <stdbool.h>
#include <stddef.h>

/* Validate only the fields consumed by Pager; unused types remain ignorable. */
bool pager_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t body_words);

#endif
