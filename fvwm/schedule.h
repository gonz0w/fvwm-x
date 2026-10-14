/* -*-c-*- */

#ifndef FVWM_SCHEDULE_H
#define FVWM_SCHEDULE_H

void squeue_execute(void);
void squeue_after_poll(void);
void squeue_execute_interaction(void);
int squeue_get_interaction_ms(void);
int squeue_get_next_ms(void);
int squeue_get_next_id(void);
int squeue_get_last_id(void);

#endif /* FVWM_SCHEDULE_H */
