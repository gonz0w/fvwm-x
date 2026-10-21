#ifndef FVWMLIB_LOG_H
#define FVWMLIB_LOG_H

#include <stdbool.h>

#define printflike(a, b) __attribute__ ((format (printf, a, b)))

extern int	lib_log_level;

void	set_log_file(char *name);
void	log_open(const char *);
void	log_toggle(const char *);
void	log_close(void);
int	log_get_fd(void);
/* Successful adoption owns fd (stderr stays borrowed); failure leaves it
 * with the caller. Repeated adoption of the current descriptor or -1 is a
 * no-op. Inheritance is intentional. */
bool	log_set_fd(int);
/* Adopt fork-time metadata at most once, including disabled/failed adoption.
 * Later broadcasts do not transfer a new descriptor into this process. */
bool	log_adopt_inherited_fd(int);
void printflike(2, 3) fvwm_debug(const char *, const char *, ...);

#endif
