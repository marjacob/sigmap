#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"

static int sigmap[NSIG] = { 0 };
static int pending_signal = 0;

static void handle_signal(int signo)
{
	pending_signal = signo;
}

static void usage(const char *program)
{
	printf("%s\n", program);
	return;
}

void dump_error(const char *func, int rc, int errnum, int quiet)
{
	if (!quiet) {
		fprintf(stderr,
			"%s (rc: %d): %s (errno: %d)\n",
			func,
			rc,
			strerror(errnum),
			errnum);
	}
}

int map_signal(int from, int to)
{
	struct sigaction sa;

	/* register the signal handler */
	sa.sa_handler = &handle_signal;

	/* restart interrupted system calls if possible */
	sa.sa_flags = 0 /* SA_RESTART */;

	/* block every signal during the handler */
	sigfillset(&sa.sa_mask);

	int rc = 0;

	if (!(rc = sigaction(from, &sa, NULL))) {
		sigmap[from - 1] = to;
	}

	return rc;
}

void parse_signal_map(const char *format, int *from, int *to, int *errnum)
{
	errno = *errnum = 0;
	char *endptr = NULL;
	int src_sig = util_strtoi(format, &endptr, 10);

	if (errno) {
		*errnum = errno;
		return;
	}

	int dst_sig = 0;

	if (':' == endptr[0] && '\0' != endptr[1]) {
		dst_sig = util_strtoi(endptr + 1, &endptr, 10);

		if (errno) {
			*errnum = errno;
			return;
		}
	}

	*from = src_sig;
	*to = dst_sig;
}

int main(int argc, char *argv[])
{
	int rc;
	int ch;

	static struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"map", required_argument, NULL, 'm'},
		{"override", required_argument, NULL, 'o'},
		{"quiet", no_argument, NULL, 'q'},
		{NULL, 0, NULL, 0}
	};

	int sigfrom = 0;
	int sigto = 0;
	int errnum = 0;
	int quiet = 0;

	while ((ch = getopt_long(argc, argv, "hm:o:q", longopts, NULL)) != -1) {
		switch (ch) {
		case 'q':
			quiet = 1;
			break;
		case 'm':
			parse_signal_map(optarg, &sigfrom, &sigto, &errnum);

			if (errnum) {
				fprintf(stderr, "error: %s\n", strerror(errnum));
				return EXIT_FAILURE;
			}

			map_signal(sigfrom, sigto);

			printf("mapping signal %d to %d\n", sigfrom, sigto);
			break;
		case 'o':
			fprintf(stderr, "todo\n");
			return EXIT_FAILURE;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return EXIT_SUCCESS;
		}
	}

	argc -= optind;
	argv += optind;

	if (!argc) {
		if (!quiet) {
			fprintf(stderr, "error: no input files\n");
		}

		return EXIT_FAILURE;
	}

	pid_t child = fork();

	if (0 > child) {
		dump_error("fork", child, errno, quiet);
		return EXIT_FAILURE;
	} else if (!child) {
		char *child_argv[] = { argv[0], "60", NULL };
		char *child_envp[] = { NULL };
		rc = execve(argv[0], child_argv, child_envp);
		dump_error("execve", rc, errno, quiet);
		return EXIT_FAILURE;
	}

	if (!quiet) {
		fprintf(stdout,
			"parent [pid: %d] forked off [pid: %d]\n", 
			getpid(), 
			child);
	}

	int status;
	pid_t pid = 0;

	while (0 > (pid = waitpid(child, &status, 0))) {
		if (pending_signal) {
			int mapped_signal = sigmap[pending_signal - 1];
			if (!quiet) {
				fprintf(stdout,
					"parent received signal %d\n"
					"sending signal %d to pid %d\n",
					pending_signal,
					mapped_signal,
					child);
			}

			kill(child, mapped_signal);
			pending_signal = 0;
		}
	}

	if (WIFSIGNALED(status)) {
		printf("%d terminated on signal %d\n", child, WTERMSIG(status));
	}

	return EXIT_SUCCESS;
}