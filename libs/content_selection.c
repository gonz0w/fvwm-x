/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_selection.h"

struct content_selection_transfer {
	struct content_payload_owner *owner;
	struct content_ref ref;
	uint32_t representation;
	struct content_selection_options options;
	struct content_selection_result *result;
	enum content_selection_status status;
	size_t offset;
};

static bool source_current(FSelectionSource source, void *context)
{
	struct content_selection_transfer *transfer = context;

	return transfer->options.current(source, transfer->options.context);
}

static bool service(void *context)
{
	struct content_selection_transfer *transfer = context;

	return !transfer->options.service ||
		transfer->options.service(transfer->options.context);
}

static bool consume(const unsigned char *bytes, size_t offset, size_t length,
	void *context)
{
	struct content_selection_transfer *transfer = context;
	uint64_t now_ms = 0;

	if (!transfer->options.clock(transfer->options.context, &now_ms))
	{
		transfer->status = CONTENT_SELECTION_CLOCK;
		return false;
	}
	if (offset != transfer->offset)
	{
		transfer->status = CONTENT_SELECTION_INVALID;
		return false;
	}
	transfer->result->payload = content_payload_write(transfer->owner,
		transfer->ref, transfer->representation,
		CONTENT_PAYLOAD_ENCODED,
		offset, bytes, length, now_ms, &transfer->result->error);
	if (transfer->result->payload != CONTENT_PAYLOAD_OK)
	{
		transfer->status = CONTENT_SELECTION_PAYLOAD;
		return false;
	}
	transfer->offset += length;
	return true;
}

/* Read-only inspection after the owner's guarded zero-write validates ref and
 * prepared storage. Never mutate/acknowledge the embedded policy separately. */
static bool empty_single_producer(const struct content_payload_owner *owner,
	struct content_ref ref, uint32_t representation,
	enum content_hint *hint, size_t *maximum)
{
	for (size_t i = 0; i < owner->policy.limits.items; i++)
	{
		const struct content_payload_slot *slot = &owner->slots[i];

		if (slot->ref.epoch == ref.epoch &&
			slot->ref.item == ref.item &&
			slot->ref.generation == ref.generation)
		{
			const struct content_payload_region *region =
				&slot->regions[0];

			*hint = slot->item->descriptor.hint;
			*maximum = region->capacity.encoded;
			return slot->count == 1 &&
				region->representation == representation &&
				!region->capacity.decoded &&
				!region->capacity.derived &&
				!region->written.encoded;
		}
	}
	return false;
}

enum content_selection_status content_selection_receive(
	struct content_payload_owner *owner, struct content_ref ref,
	uint32_t representation,
	const struct content_selection_options *options,
	struct content_selection_result *result)
{
	struct content_selection_options request = options ? *options :
		(struct content_selection_options){0};
	struct content_selection_transfer transfer = {
		.owner = owner, .ref = ref, .representation = representation,
		.options = request, .result = result,
		.status = CONTENT_SELECTION_OK
	};
	FSelectionReceipt receipt = {0};
	uint64_t now_ms = 0;
	enum content_hint hint = CONTENT_HINT_ABSENT;
	size_t maximum = 0;

	if (!result)
		return CONTENT_SELECTION_INVALID;
	*result = (struct content_selection_result){
		.transfer = FSELECTION_INVALID,
		.payload = CONTENT_PAYLOAD_INVALID
	};
	if (!owner || !options || !representation || !request.display ||
		request.selection == None || request.target == None ||
		request.property == None || request.source.owner == None ||
		!request.source.generation || !request.current ||
		!request.clock ||
		request.timeout_ms <= 0 ||
		request.timeout_ms > FSELECTION_TIMEOUT_MS)
		return CONTENT_SELECTION_INVALID;
	if (!request.clock(request.context, &now_ms))
		return CONTENT_SELECTION_CLOCK;
	result->payload = content_payload_write(owner, ref, representation,
		CONTENT_PAYLOAD_ENCODED, 0, NULL, 0, now_ms, &result->error);
	if (result->payload != CONTENT_PAYLOAD_OK)
		return CONTENT_SELECTION_PAYLOAD;
	if (!empty_single_producer(owner, ref, representation, &hint, &maximum))
		return CONTENT_SELECTION_INVALID;
	if (maximum > FSELECTION_TEXT_MAX_BYTES)
		maximum = FSELECTION_TEXT_MAX_BYTES;
	FSelectionSink sink = {.source = request.source,
		.maximum = maximum,
		.current = source_current, .consume = consume,
		.context = &transfer};

	/* From this point, every failure owns exact-ref retirement. */
	result->producer_taken = true;
	if (hint != request.hint)
	{
		result->payload = CONTENT_PAYLOAD_REFUSED;
		result->error.policy = CONTENT_EXCLUDED;
		transfer.status = CONTENT_SELECTION_PAYLOAD;
		goto retire;
	}
	result->transfer = FSelectionRequestExactTo(request.display,
		request.selection, request.target,
		request.property, request.time,
		request.timeout_ms, request.service ? service : NULL, &transfer,
		&sink, &receipt);
	if (result->transfer != FSELECTION_OK)
	{
		if (transfer.status == CONTENT_SELECTION_OK)
			transfer.status = CONTENT_SELECTION_TRANSFER;
		goto retire;
	}
	if (XGetSelectionOwner(request.display, request.selection) !=
		request.source.owner || !request.current(request.source,
			request.context))
	{
		result->transfer = FSELECTION_STALE_SOURCE;
		transfer.status = CONTENT_SELECTION_TRANSFER;
		goto retire;
	}
	/* Source observation may take time. Sample expiry only after the last
	 * external observer, then enter the native gate without callbacks. */
	if (!request.clock(request.context, &now_ms))
	{
		transfer.status = CONTENT_SELECTION_CLOCK;
		goto retire;
	}
	if (receipt.length != transfer.offset)
	{
		transfer.status = CONTENT_SELECTION_INVALID;
		goto retire;
	}
	/* Recheck the exact prefix after all external callbacks. No entered
	 * callback separates this guarded check and native publication. */
	result->payload = content_payload_write(owner, ref, representation,
		CONTENT_PAYLOAD_ENCODED, receipt.length, NULL, 0, now_ms,
		&result->error);
	if (result->payload == CONTENT_PAYLOAD_OK)
		result->payload = content_payload_publish(owner, ref,
			request.hint,
			now_ms, &result->error);
	if (result->payload != CONTENT_PAYLOAD_OK)
	{
		transfer.status = CONTENT_SELECTION_PAYLOAD;
		goto retire;
	}
	result->length = receipt.length;
	result->type = receipt.type;
	return CONTENT_SELECTION_OK;
retire:
	result->retirement = content_payload_retire(owner, ref,
		&result->retirement_error);
	result->error.cleanup_pending = result->error.cleanup_pending ||
		result->retirement_error.cleanup_pending;
	return transfer.status;
}
