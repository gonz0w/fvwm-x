/* -*-c-*- */

#ifndef FVWM_UPDATE_H
#define FVWM_UPDATE_H

#include "fvwm.h"

typedef struct update_win update_win;

void destroy_scheduled_windows(void);
void apply_decor_change(FvwmWindow *fw);
void flush_window_updates(void);
Bool update_fvwm_monitor(FvwmWindow *fw);

#endif /* FVWM_UPDATE_H */
