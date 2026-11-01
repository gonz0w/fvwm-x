/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "file_transfer_protocol.h"
#include <string.h>

static bool
record_valid(const struct file_transfer_record *r)
{
	bool selection = r->kind == FILE_TRANSFER_BEGIN ||
		r->kind == FILE_TRANSFER_ACTION;
	bool pointer = r->kind >= FILE_TRANSFER_BEGIN &&
		r->kind <= FILE_TRANSFER_CANCEL;
	unsigned int count = 0;
	unsigned char nonce = 0;

	if (r->kind < FILE_TRANSFER_ATTACH || r->kind > FILE_TRANSFER_ACTION ||
	    !r->epoch || !r->generation || !r->frame || !r->list)
		return false;
	for (size_t i = 0; i < sizeof(r->nonce); i++)
		nonce |= r->nonce[i];
	if (!nonce || (!pointer && r->kind != FILE_TRANSFER_ACTION &&
	    (r->timestamp || r->root_x || r->root_y || r->state)))
		return false;
	if (r->kind == FILE_TRANSFER_ACTION &&
	    (r->detail < FILE_TRANSFER_ACTION_COPY ||
	    r->detail > FILE_TRANSFER_ACTION_PASTE ||
	    r->root_x || r->root_y || r->state))
		return false;
	if (r->kind == FILE_TRANSFER_FEEDBACK && r->detail > FILE_TRANSFER_IDLE)
		return false;
	if (r->kind == FILE_TRANSFER_CLIENT_MESSAGE)
	{
		if (!r->detail || r->format != 32)
			return false;
	}
	else
	{
		if (r->format || (r->detail && r->kind != FILE_TRANSFER_ACTION &&
		    r->kind != FILE_TRANSFER_FEEDBACK))
			return false;
		for (size_t i = 0; i < 5; i++)
			if (r->data[i])
				return false;
	}
	if (r->kind == FILE_TRANSFER_ACTION &&
	    r->detail == FILE_TRANSFER_ACTION_PASTE &&
	    (r->rows || r->selection_bytes))
		return false;
	if (selection)
	{
		if ((r->kind == FILE_TRANSFER_BEGIN && !r->rows) ||
		    r->rows > FILE_TRANSFER_MAX_ROWS ||
		    r->selection_bytes != (r->rows + 7) / 8)
			return false;
		for (uint32_t i = 0; i < r->rows; i++)
			count += (r->selection[i / 8] >> (i % 8)) & 1;
		if (count > FILE_TRANSFER_MAX_SELECTED ||
		    (r->rows % 8 && (r->selection[r->selection_bytes - 1] >>
		    (r->rows % 8))))
			return false;
	}
	else if (r->rows || r->selection_bytes)
		return false;
	for (size_t i = r->selection_bytes;
	     i < FILE_TRANSFER_SELECTION_BYTES; i++)
		if (r->selection[i])
			return false;
	return true;
}

bool
file_transfer_encode(const struct file_transfer_record *r,
	unsigned char out[FILE_TRANSFER_WIRE_BYTES])
{
	uint32_t words[24] = {0};

	if (!r || !out || !record_valid(r))
		return false;
	words[0] = FILE_TRANSFER_MAGIC;
	words[1] = r->kind;
	words[2] = r->epoch >> 32;
	words[3] = r->epoch;
	words[4] = r->generation;
	words[5] = r->frame;
	words[6] = r->list;
	words[7] = r->timestamp;
	words[8] = (uint32_t)r->root_x;
	words[9] = (uint32_t)r->root_y;
	words[10] = r->state;
	words[11] = r->detail;
	memcpy(words + 12, r->data, sizeof(r->data));
	words[17] = r->format;
	words[18] = r->selection_bytes;
	words[19] = r->rows;
	for (size_t i = 0; i < 20; i++)
		for (size_t j = 0; j < 4; j++)
			out[i * 4 + j] = words[i] >> (24 - j * 8);
	memcpy(out + 80, r->nonce, 16);
	memcpy(out + 96, r->selection, FILE_TRANSFER_SELECTION_BYTES);
	return true;
}

bool
file_transfer_decode(const unsigned char *in, size_t length,
	struct file_transfer_record *r)
{
	uint32_t w[20];
	struct file_transfer_record decoded = {0};

	if (r)
		memset(r, 0, sizeof(*r));
	if (!in || !r || length != FILE_TRANSFER_WIRE_BYTES)
		return false;
	for (size_t i = 0; i < 20; i++)
		w[i] = (uint32_t)in[i * 4] << 24 |
			(uint32_t)in[i * 4 + 1] << 16 |
			(uint32_t)in[i * 4 + 2] << 8 | in[i * 4 + 3];
	if (w[0] != FILE_TRANSFER_MAGIC)
		return false;
	decoded.kind = w[1];
	decoded.epoch = (uint64_t)w[2] << 32 | w[3];
	decoded.generation = w[4];
	decoded.frame = w[5];
	decoded.list = w[6];
	decoded.timestamp = w[7];
	decoded.root_x = (int32_t)w[8];
	decoded.root_y = (int32_t)w[9];
	decoded.state = w[10];
	decoded.detail = w[11];
	memcpy(decoded.data, w + 12, sizeof(decoded.data));
	decoded.format = w[17];
	decoded.selection_bytes = w[18];
	decoded.rows = w[19];
	memcpy(decoded.nonce, in + 80, 16);
	memcpy(decoded.selection, in + 96, FILE_TRANSFER_SELECTION_BYTES);
	if (!record_valid(&decoded))
		return false;
	*r = decoded;
	return true;
}
