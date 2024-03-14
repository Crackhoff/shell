#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "builtins.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

int echo(char *[]);
int undefined(char *[]);
int lexit(char *[]);
int lls(char *[]);
int lkill(char *[]);
int cd(char *[]);

builtin_pair builtins_table[] = {
	{"exit", &lexit},
	{"lecho", &echo},
	{"lcd", &cd},
	{"cd", &cd},
	{"lkill", &lkill},
	{"lls", &lls},
	{NULL, NULL}};

int echo(char *argv[])
{
	int i = 1;
	if (argv[i])
		printf("%s", argv[i++]);
	while (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int undefined(char *argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}

int cd(char *argv[])
{
	char *dest = getenv("HOME");
	if (argv[2] != 0)
	{
		fprintf(stderr, "Builtin lcd error.\n");
		return EXIT_FAILURE;
	}
	if (argv[1] != 0)
		dest = argv[1];
	int x;
	if ((x = chdir(dest)) == -1)
	{
		// printf("%d %s \n", x, dest);
		fprintf(stderr, "Builtin lcd error.\n");
		return EXIT_FAILURE;
	}
	return 0;
}

int lexit(char *argv[])
{
	// I can just ignore any arguments
	exit(0);
	return 0;
}

int lkill(char *argv[])
{
	char *pidstr;
	int sig = SIGTERM;
	if (argv[1] == 0)
	{
		fprintf(stderr, "Builtin lkill error.\n");
		return EXIT_FAILURE;
	}
	if (argv[2] == 0)
	{
		pidstr = argv[1];
	}
	else if (argv[3] == 0)
	{
		pidstr = argv[2];
		// try to parse signal
		char *res;
		errno = 0;
		long sig_l = strtol(argv[1] + 1, &res, 10);

		if ((errno == ERANGE && (sig_l == LONG_MAX || sig_l == LONG_MIN)) ||
			(errno != 0 && sig_l == 0))
		{
			fprintf(stderr, "Builtin lkill error.\n");
			return EXIT_FAILURE;
		}
		if (sig_l >= INT_MAX || sig_l <= INT_MIN)
		{
			fprintf(stderr, "Builtin lkill error.\n");
			return EXIT_FAILURE;
		};
		sig = (int)sig_l;
	}
	else
	{
		fprintf(stderr, "Builtin lkill error.\n");
		return EXIT_FAILURE;
	}
	char *res;
	long ppid_l = strtol(pidstr, &res, 10);
	if ((errno == ERANGE && (ppid_l == LONG_MAX || ppid_l == LONG_MIN)) ||
		(errno != 0 && ppid_l == 0))
	{

		// printf("here5\n");
		fprintf(stderr, "Builtin lkill error.\n");
		return EXIT_FAILURE;
	}
	else if (ppid_l >= INT_MAX || ppid_l <= INT_MIN)
	{
		fprintf(stderr, "Builtin lkill error.\n");
		return EXIT_FAILURE;
	}
	else
	{
		// ppid_l into int
		int ppid = (int)ppid_l;
		if (kill(ppid, sig) == -1)
		{
			// printf("here3\n");
			fprintf(stderr, "Builtin lkill error.\n");
			return EXIT_FAILURE;
		}
	}
	return 0;
}

int lls(char *argv[])
{
	DIR *dir;
	struct dirent *dirnt;

	if ((dir = opendir(".")) != NULL)
	{
		while ((dirnt = readdir(dir)) != NULL)
		{
			if (dirnt->d_name[0] != '.')
			{
				printf("%s\n", dirnt->d_name);
				fflush(stdout);
			}
		}
		closedir(dir);
	}
	else
	{
		fprintf(stderr, "Builtin lls error.\n");
		return EXIT_FAILURE;
	}
	return 0;
}