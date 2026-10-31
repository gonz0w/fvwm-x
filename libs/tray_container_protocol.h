#ifndef FVWM_TRAY_CONTAINER_PROTOCOL_H
#define FVWM_TRAY_CONTAINER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define FVWM_TRAY_CONTAINER_VERSION 1U
#define FVWM_TRAY_CONTAINER_CAP "_FVWM_TRAY_CONTAINER_V1"
#define FVWM_TRAY_CONTAINER_SLOT "_FVWM_TRAY_SLOT_V1"
#define FVWM_TRAY_CONTAINER_OFFER "_FVWM_TRAY_OFFER_V1"
#define FVWM_TRAY_CONTAINER_ATTACH "_FVWM_TRAY_ATTACH_V1"
#define FVWM_TRAY_CONTAINER_ACK "_FVWM_TRAY_ATTACHED_V1"
#define FVWM_TRAY_CONTAINER_DETACH "_FVWM_TRAY_DETACH_V1"
#define FVWM_TRAY_CONTAINER_FOCUS "_FVWM_TRAY_FOCUS_V1"
#define FVWM_TRAY_CONTAINER_FOCUS_REQUEST "_FVWM_TRAY_FOCUS_REQUEST_V1"
#define FVWM_TRAY_CONTAINER_FOCUS_ACK "_FVWM_TRAY_FOCUS_ACK_V1"
#define FVWM_TRAY_CONTAINER_RETURN "_FVWM_TRAY_RETURN_V1"

/* A pending handshake wakes the existing poll loop at its deadline. */
static inline int tray_container_pending_timeout(bool pending,
	double deadline, double now)
{
	if (!pending)
		return -1;
	if (!(now >= 0) || !(deadline > now))
		return 0;
	double remaining = deadline - now;
	return remaining >= 2.0 ? 2000 : (int)(remaining * 1000.0) + 1;
}

/* Four distinct live XIDs bind one attempt. Atom names carry the version;
 * the fifth ClientMessage word is version, acknowledgement or direction. */
static inline bool tray_container_identity_valid(const uint32_t words[5])
{
	if (!words)
		return false;
	for (unsigned int i = 0; i < 4; i++)
	{
		if (!words[i])
			return false;
		for (unsigned int j = 0; j < i; j++)
			if (words[i] == words[j])
				return false;
	}
	return true;
}

static inline bool tray_container_identity_equal(const uint32_t left[5],
	const uint32_t right[5])
{
	if (!tray_container_identity_valid(left) ||
	    !tray_container_identity_valid(right))
		return false;
	for (unsigned int i = 0; i < 4; i++)
		if (left[i] != right[i])
			return false;
	return true;
}

#endif
