/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/Module.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* A real module launched by the core. The shared logging descriptor is an
 * intentional FVWM module contract; accept that inode alongside pipe ends. */
int
main(int argc, char **argv)
{
	if (argc == 4)
	{
		for (int i = 2; i < 4; i++)
		{
			if (fcntl(atoi(argv[i]), F_GETFD) != -1 ||
			    errno != EBADF)
				return 1;
		}
		return 0;
	}
	if (argc < 8)
	{
		return 2;
	}
	ModuleArgs *module = ParseModuleArgs(argc, argv, 0);
	if (module == NULL)
		return 2;
	int output = module->to_fvwm, input = module->from_fvwm;
	struct stat log_status, status;
	if (stat(argv[7], &log_status) != 0)
	{
		return 3;
	}
	int leaks = 0;
	for (int descriptor = 3; descriptor < 1024; descriptor++)
	{
		if (descriptor == input || descriptor == output ||
		    fcntl(descriptor, F_GETFD) < 0)
		{
			continue;
		}
		if (fstat(descriptor, &status) != 0 ||
		    status.st_dev != log_status.st_dev ||
		    status.st_ino != log_status.st_ino)
		{
			leaks++;
		}
	}
	/* A later exec must lose both protocol endpoints, while the module
	 * itself retains them. This is exercised through a real core launch. */
	pid_t child = fork();
	if (child == 0)
	{
		execl(argv[0], argv[0], "--child", argv[1], argv[2],
			(char *)NULL);
		_exit(127);
	}
	int child_status;
	if (child < 0 || waitpid(child, &child_status, 0) != child ||
	    !WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0 ||
	    fcntl(output, F_GETFD) < 0 ||
	    (input >= 0 && fcntl(input, F_GETFD) < 0))
		leaks++;
	FILE *report = fopen(argv[6], "w");
	if (report == NULL)
	{
		return 4;
	}
	fprintf(report, "%d\n", leaks);
	fclose(report);
	return leaks == 0 ? 0 : 1;
}
