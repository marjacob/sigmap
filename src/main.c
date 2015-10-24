#include <errno.h>
#include <getopt.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* waitpid [Debian] */
#include <sys/wait.h>  /* waitpid [Debian, OSX] */
#include <unistd.h>
#include "utils.h"

static int m_handle_signals = 0;
static int m_pending_signal = 0;
static int m_sigmap[NSIG] = { 0 };
static sigjmp_buf m_sigenv;

static void handle_signal(int signo)
{
	if (m_handle_signals) {
		m_pending_signal = signo;
		siglongjmp(m_sigenv, 1);
	}
}

static void print_usage(const char *program)
{
	fprintf(stdout,
		"usage: %s [-hq] [-f sig] [-m sig:sig] [bin args ...]\n", 
		program);
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

	/* Register the signal handler. */
	sa.sa_handler = &handle_signal;

	/* Do not restart interrupted system calls. */
	sa.sa_flags = 0 /* SA_RESTART */;

	/* Block all other signals during the handler. */
	sigfillset(&sa.sa_mask);

	int rc = 0;

	if (!(rc = sigaction(from, &sa, NULL))) {
		m_sigmap[from - 1] = to;
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
	char *bin = argv[0];
	int ch;
	int rc;
	pid_t my_pid = getpid();
	sigset_t sigset_block_mask;
	sigset_t sigset_unblock_mask;

	sigemptyset(&sigset_unblock_mask);
	sigfillset(&sigset_block_mask);

	static struct option longopts[] = {
		/* TODO: Allow passthrough of signals (same as -m 2:2). */
		{"forward", required_argument, NULL, 'f'},
		/* Print the help text and exit. */
		{"help", no_argument, NULL, 'h'},
		/* Map one signal to another. */
		{"map", required_argument, NULL, 'm'},
		/* Be completely silent. */
		{"quiet", no_argument, NULL, 'q'},
		/* EOF. */
		{NULL, 0, NULL, 0}
	};

	int sigfrom = 0;
	int sigto = 0;
	int errnum = 0;
	int quiet = 0;

	/* Do not remove the leading '+' sign in the opts string, as that may
	 * cause getopt_long to perform permutation of the options, which may 
	 * prevent the program we want to launch from appearing last. */
	const char *opts = "+hm:q";
	
	/* Block all signals. 
	 * They will be processed when signal handlers have been registered. */
	sigprocmask(SIG_SETMASK, &sigset_block_mask, NULL);
	
	while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
		switch (ch) {
		case 'm':
			parse_signal_map(optarg, &sigfrom, &sigto, &errnum);

			if (errnum) {
				fprintf(stderr, 
					"%s [pid: %d]: %s [errno]\n",
					bin,
					my_pid, 
					strerror(errnum));

				return EXIT_FAILURE;
			}

			if ((rc = map_signal(sigfrom, sigto))) {
				dump_error("sigaction", rc, errno, quiet);
			} else if (!quiet) {
				fprintf(stdout,
					"%s [pid: %d]: mapping signal %d to "
					"signal %d\n", 
					bin,
					my_pid,
					sigfrom, 
					sigto);			
			}

			break;
		case 'o':
			fprintf(stderr, "TODO\n");
			return EXIT_FAILURE;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'h':
		default:
			print_usage(bin);
			return EXIT_SUCCESS;
		}
	}

	/* The argument count in argc and pointers in argv are now ready to be
	 * used when executing the child process. */
	argc -= optind;
	argv += optind;

	if (!argc) {
		if (!quiet) {
			fprintf(stderr, "error: no input files\n");
		}

		return EXIT_FAILURE;
	}

	/* Unblock all signals.
	 * This has to be done before the call to fork(2) beacause the child
	 * process inherits the signal mask of the parent process. The signal
	 * handler however, will not do anything until the m_handle_signals
	 * flag evaluates to true. */
	sigprocmask(SIG_SETMASK, &sigset_unblock_mask, NULL);

	pid_t child = fork();

	if (0 > child) {
		dump_error("fork", child, errno, quiet);
		return EXIT_FAILURE;
	} else if (!child) {
		rc = execv(argv[0], argv);
		dump_error("execv", rc, errno, quiet);

		return EXIT_FAILURE;
	}

	if (!quiet) {
		fprintf(stdout,
			"%s [pid: %d]: forked off [pid: %d]\n", 
			bin,
			my_pid, 
			child);
	}

	int status = 0;
	
	if (!sigsetjmp(m_sigenv, 0)) {
		/* Allow the signal handlers to jump. 
		 * This code is only executed once. */
		m_handle_signals = 1;
	} else {
		/* Signals are currently blocked by the signal handler.
		 * The same rules regarding unsafe functions apply here. */

		int mapped_signal = m_sigmap[m_pending_signal - 1];

		if (!quiet) {
			/* This is acceptable because we are not using any
			 * unsafe functions anywhere else at any point where
			 * we can be interrupted by a signal. */
			fprintf(stdout,
				"%s [pid: %d]: caught signal %d\n"
				"%s [pid: %d]: sending signal %d [pid: %d]\n",
				bin,
				my_pid,
				m_pending_signal,
				bin,
				my_pid,
				mapped_signal,
				child);
		}

		m_pending_signal = 0;

		/* Forward the appropriate signal to the child. */
		kill(child, mapped_signal);
	}

	/* Unblock all signals. */
	sigprocmask(SIG_SETMASK, &sigset_unblock_mask, NULL);

	/* Watch the child until it dies. */
	errno = 0;
	while (waitpid(child, &status, 0) != child) {
		if (errno != EINTR) {
			return EXIT_FAILURE;
		}
	}

	/* Block all signals. */
	sigprocmask(SIG_SETMASK, &sigset_block_mask, NULL);

	if (!quiet && WIFSIGNALED(status)) {
		fprintf(stdout,
			"%s [pid: %d]: "
			"%s [pid: %d] terminated on signal %d\n", 
			bin,
			my_pid,
			argv[0],
			child, 
			WTERMSIG(status));
	}

	return EXIT_SUCCESS;
}
