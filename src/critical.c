#include <signal.h>
#include <stdlib.h>
#include "critical.h"

static sigset_t m_set;

int crit_block_signals(void)
{
	sigset_t set;
	sigfillset(&set);
	return pthread_sigmask(SIG_SETMASK, &set, &m_set);
}

int crit_unblock_signals(void)
{
	return pthread_sigmask(SIG_SETMASK, &m_set, NULL);
}
