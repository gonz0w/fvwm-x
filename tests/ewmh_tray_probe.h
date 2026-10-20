#ifndef FVWM_TEST_EWMH_TRAY_PROBE_H
#define FVWM_TEST_EWMH_TRAY_PROBE_H

#include "fvwm/ewmh_client_list.h"

void ewmh_tray_probe(void);
void ewmh_tray_allocation(size_t bytes);
bool ewmh_tray_append_refused(ewmh_client_list *list, Window window);
void ewmh_tray_publication(Window window, Atom property, Atom type,
	int format, int mode, const unsigned char *data, int length);

#endif
