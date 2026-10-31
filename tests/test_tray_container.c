/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "libs/tray_container_protocol.h"
#include <assert.h>
#include <string.h>

int main(void)
{
	uint32_t attempt[5] = {10, 20, 30, 40, 1};
	uint32_t reply[5];
	memcpy(reply, attempt, sizeof(reply));
	assert(tray_container_identity_valid(attempt));
	assert(tray_container_identity_equal(attempt, reply));
	/* Status/direction belongs to the versioned message type, not the
	 * lifetime tuple; the identical tuple is used for release and
	 * return. */
	reply[4] = UINT32_MAX;
	assert(tray_container_identity_equal(attempt, reply));
	for (unsigned int i = 0; i < 4; i++)
	{
		memcpy(reply, attempt, sizeof(reply));
		reply[i] = 0;
		assert(!tray_container_identity_valid(reply));
		assert(!tray_container_identity_equal(attempt, reply));
		memcpy(reply, attempt, sizeof(reply));
		reply[i] = 100 + i;
		assert(!tray_container_identity_equal(attempt, reply));
	}
	for (unsigned int i = 0; i < 4; i++)
		for (unsigned int j = 0; j < i; j++)
		{
			memcpy(reply, attempt, sizeof(reply));
			reply[i] = reply[j];
			assert(!tray_container_identity_valid(reply));
		}
	assert(!tray_container_identity_valid(NULL));
	assert(!tray_container_identity_equal(attempt, NULL));
	assert(tray_container_pending_timeout(false, 12.0, 10.0) == -1);
	assert(tray_container_pending_timeout(true, 12.0, 10.0) == 2000);
	assert(tray_container_pending_timeout(true, 12.0, 11.5) == 501);
	assert(tray_container_pending_timeout(true, 12.0, 12.0) == 0);
	assert(tray_container_pending_timeout(true, 12.0, 13.0) == 0);
	assert(tray_container_pending_timeout(true, 0.0, 10.0) == 0);
	assert(tray_container_pending_timeout(true, 12.0, -1.0) == 0);
	return 0;
}
