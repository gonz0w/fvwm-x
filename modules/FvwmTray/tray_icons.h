#ifndef FVWM_TRAY_ICONS_H
#define FVWM_TRAY_ICONS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define TRAY_ICON_BATCH_MAX 8U
#define TRAY_ICON_NAME_MAX 256U
#define TRAY_ICON_THEME_MAX 128U
#define TRAY_ICON_ROOT_MAX 1024U
#define TRAY_ICON_PATH_MAX 2048U

typedef struct tray_icons tray_icons;

typedef struct
{
	uint64_t id, generation;
	unsigned int size, quarters; /* Logical8..128; scale quarters4..16. */
	const char *name, *theme, *theme_path;
} tray_icon_request;

typedef struct
{
	uint64_t id, generation;
	char path[TRAY_ICON_PATH_MAX + 1];
} tray_icon_result;

/* A NULL executable selects the installed private worker, with the existing
 * absolute FVWM_DESKTOP_WORKER test override. One batch/child owns its private
 * immutable files until release. Empty paths mean named-icon fallback. */
tray_icons *tray_icons_create(const char *executable);
bool tray_icons_begin(tray_icons *icons, const tray_icon_request *requests,
	unsigned int count, double now);
/* File/access observation only; not a protocol or completed-image attestation. */
bool tray_icons_available(const tray_icons *icons);
int tray_icons_write_fd(const tray_icons *icons);
int tray_icons_read_fd(const tray_icons *icons);
int tray_icons_timeout_ms(const tray_icons *icons);
/* Service after either fd is ready, and each turn for deadline/reap progress.
 * All pipe operations are nonblocking; no waitpid call blocks. */
void tray_icons_service(tray_icons *icons, double now);
bool tray_icons_busy(const tray_icons *icons);
bool tray_icons_ready(const tray_icons *icons);
unsigned int tray_icons_count(const tray_icons *icons);
const tray_icon_result *tray_icons_result(const tray_icons *icons,
	unsigned int index);
const char *tray_icons_error(const tray_icons *icons);
/* Call only after every returned filename has finished native loading/upload.
 * Native pictures retain their own pixel resources after this file retirement.
 * Cancellation terminates only the owned process group; reap is asynchronous.
 * Keep servicing a busy adapter; destroy refuses an unreaped child. */
void tray_icons_release(tray_icons *icons);
void tray_icons_cancel(tray_icons *icons);
bool tray_icons_destroy(tray_icons *icons);

/* Pure framing policy shared by the adapter and boundary tests. */
size_t tray_icons_encode(const tray_icon_request *requests, unsigned int count,
	const char *directory, const char *formats, char *output,
	size_t capacity);
bool tray_icons_decode(const tray_icon_request *requests, unsigned int count,
	const char *directory, const unsigned char *bytes, size_t length,
	tray_icon_result results[TRAY_ICON_BATCH_MAX]);

#endif
