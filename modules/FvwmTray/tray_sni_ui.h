#ifndef FVWM_TRAY_SNI_UI_H
#define FVWM_TRAY_SNI_UI_H

#include <X11/Xlib.h>
#include <poll.h>
#include <stdbool.h>

typedef struct tray_ui tray_ui;
typedef struct tray_sni_ui tray_sni_ui;

void tray_sni_init(tray_ui *ui);
void tray_sni_configure(tray_ui *ui, bool retry);
void tray_sni_service(tray_ui *ui, double now);
void tray_sni_sync(tray_ui *ui);
unsigned int tray_sni_pollfds(tray_ui *ui, struct pollfd *fds);
int tray_sni_timeout(tray_ui *ui, double now);
bool tray_sni_event(tray_ui *ui, XEvent *event);
void tray_sni_paint(tray_ui *ui, unsigned int index);
void tray_sni_repaint_popups(tray_ui *ui);
void tray_sni_focus_out(tray_ui *ui);
void tray_sni_cancel_pending_focus(tray_ui *ui);
void tray_sni_retired(tray_ui *ui, Window window);
void tray_sni_creation_error(tray_ui *ui, const XErrorEvent *error);
void tray_sni_status_reply(tray_ui *ui);
void tray_sni_stop(tray_ui *ui);
void tray_sni_destroy(tray_ui *ui);
unsigned int tray_sni_count(const tray_ui *ui);
double tray_sni_now(void);

#endif
