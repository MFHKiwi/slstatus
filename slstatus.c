/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <unistd.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
	const char *fg;
	const char *bg;
};

char buf[1024];
static volatile sig_atomic_t done;
static Display *dpy;

#include "config.h"

static void
terminate(const int signo)
{
	if (signo == SIGTSTP)
	{
		pause();
		return;
	}
	if (signo == SIGCONT)
	{
		return;
	}
	if (signo != SIGUSR1)
	{
		done = 1;
	}
}

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
	res->tv_nsec = a->tv_nsec - b->tv_nsec +
	               (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
	die("usage: %s [-v] [-s] [-1]", argv0);
}

int
main(int argc, char *argv[])
{
	struct sigaction act;
	struct timespec start, current, diff, intspec, wait;
	size_t i, len;
	int sflag, ret;
	char status[MAXLEN];
	const char *res;
	const char *fgcolor;
	const char *bgcolor;

	sflag = 0;
	ARGBEGIN {
	case 'v':
		die("slstatus-"VERSION);
		break;
	case '1':
		done = 1;
		/* FALLTHROUGH */
	case 's':
		sflag = 1;
		break;
	default:
		usage();
	} ARGEND

	if (argc)
		usage();

	memset(&act, 0, sizeof(act));
	act.sa_handler = terminate;
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGTSTP, &act, NULL);
	sigaction(SIGCONT, &act, NULL);
	act.sa_flags |= SA_RESTART;

	if (!sflag && !(dpy = XOpenDisplay(NULL)))
		die("XOpenDisplay: Failed to open display");

	puts("{ \"version\": 1, \"stop_signal\": 20, \"cont_signal\": 18 }");
	puts("[");

	do {
		if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
			die("clock_gettime:");

		status[0] = '\0';
		for (i = len = 0; i < LEN(args); i++) {
			if (!(res = args[i].func(args[i].args)))
				res = unknown_str;

			if (!(fgcolor = args[i].fg))
				fgcolor = "#ffffff";

			if (!(bgcolor = args[i].bg))
                                bgcolor = "#000000";

			if ((ret = esnprintf(status + len, sizeof(status) - len,
                                             "%s", "{\"full_text\":\"")) < 0)
                                break;
			
			len += ret;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
			                     args[i].fmt, res)) < 0)
				break;

			len += ret;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
                                             "%s", "\",\"color\":\"")) < 0)
                                break;

                        len += ret;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
                                             "%s", fgcolor)) < 0)
                                break;

                        len += ret;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
                                             "%s", "\",\"background\":\"")) < 0)
                                break;

                        len += ret;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
                                             "%s", bgcolor)) < 0)
                                break;

                        len += ret;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
                                             "%s", "\"},")) < 0)
                                break;

			len += ret;
		}
		
		status[len - 1] = '\0';

		if (sflag) {
			puts("[");
			puts(status);
			puts("],");
			fflush(stdout);
			if (ferror(stdout))
				die("puts:");
		} else {
			if (XStoreName(dpy, DefaultRootWindow(dpy), status) < 0)
				die("XStoreName: Allocation failed");
			XFlush(dpy);
		}

		if (!done) {
			if (clock_gettime(CLOCK_MONOTONIC, &current) < 0)
				die("clock_gettime:");
			difftimespec(&diff, &current, &start);

			intspec.tv_sec = interval / 1000;
			intspec.tv_nsec = (interval % 1000) * 1E6;
			difftimespec(&wait, &intspec, &diff);

			if (wait.tv_sec >= 0 &&
			    nanosleep(&wait, NULL) < 0 &&
			    errno != EINTR)
					die("nanosleep:");
		}
	} while (!done);

	if (!sflag) {
		XStoreName(dpy, DefaultRootWindow(dpy), NULL);
		if (XCloseDisplay(dpy) < 0)
			die("XCloseDisplay: Failed to close display");
	}

	return 0;
}
