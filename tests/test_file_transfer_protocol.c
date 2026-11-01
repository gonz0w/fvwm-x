/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "libs/file_transfer_protocol.h"
#include <assert.h>
#include <string.h>

int
main(void)
{
	struct file_transfer_record in = {
		.kind = FILE_TRANSFER_ATTACH, .epoch = UINT64_C(0x123456789abcdef0),
		.generation = UINT32_MAX, .frame = 42, .list = 43, .nonce = {1}};
	struct file_transfer_record out;
	unsigned char wire[FILE_TRANSFER_WIRE_BYTES];

	assert(file_transfer_encode(&in, wire));
	assert(wire[0] == 'F' && wire[1] == 'V' && wire[2] == 'T');
	assert(wire[8] == 0x12 && wire[15] == 0xf0 && wire[80] == 1);
	assert(file_transfer_decode(wire, sizeof(wire), &out));
	assert(out.epoch == in.epoch && out.generation == UINT32_MAX);
	assert(!file_transfer_decode(wire, sizeof(wire) - 1, &out));
	assert(out.epoch == 0);
	wire[607] = 1;
	assert(!file_transfer_decode(wire, sizeof(wire), &out));
	in.kind = FILE_TRANSFER_BEGIN;
	in.timestamp = UINT32_MAX;
	in.root_x = -32000;
	in.rows = 9;
	in.selection_bytes = 2;
	in.selection[0] = 1;
	in.selection[1] = 1;
	assert(file_transfer_encode(&in, wire));
	assert(file_transfer_decode(wire, sizeof(wire), &out));
	assert(out.root_x == -32000 && out.selection[1] == 1);
	in.selection[1] = 2;
	assert(!file_transfer_encode(&in, wire));
	in.rows = FILE_TRANSFER_MAX_ROWS;
	in.selection_bytes = FILE_TRANSFER_SELECTION_BYTES;
	memset(in.selection, 255, sizeof(in.selection));
	assert(!file_transfer_encode(&in, wire));
	memset(in.selection, 0, sizeof(in.selection));
	memset(in.selection, 255, 32);
	assert(file_transfer_encode(&in, wire));
	in.selection[32] = 1;
	assert(!file_transfer_encode(&in, wire));
	memset(in.selection, 0, sizeof(in.selection));
	in.rows = in.selection_bytes = 0;
	in.root_x = 0;
	in.kind = FILE_TRANSFER_ACTION;
	in.detail = FILE_TRANSFER_ACTION_PASTE;
	assert(file_transfer_encode(&in, wire));
	assert(file_transfer_decode(wire, sizeof(wire), &out));
	in.kind = FILE_TRANSFER_CLIENT_MESSAGE;
	in.timestamp = 0;
	in.detail = 123;
	in.format = 32;
	in.data[0] = UINT32_MAX;
	assert(file_transfer_encode(&in, wire));
	assert(file_transfer_decode(wire, sizeof(wire), &out));
	assert(out.data[0] == UINT32_MAX);
	in.format = 8;
	assert(!file_transfer_encode(&in, wire));
	in.kind = FILE_TRANSFER_FEEDBACK;
	in.detail = FILE_TRANSFER_IDLE;
	in.format = 0;
	in.data[0] = 0;
	assert(file_transfer_encode(&in, wire));
	in.detail++;
	assert(!file_transfer_encode(&in, wire));
	return 0;
}
