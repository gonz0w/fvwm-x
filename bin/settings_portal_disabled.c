/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or any later version. */
#include "config.h"
#include <stdio.h>

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	fputs("fvwm-settings-portal: optional Settings backend disabled\n", stderr);
	return 1;
}
