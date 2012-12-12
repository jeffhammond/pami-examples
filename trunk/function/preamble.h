#ifndef PREAMBLE_H
#define PREAMBLE_H

#include <pami.h>

#ifdef DEBUG
#define TEST_ASSERT(c,m) \
        do { \
        if (!(c)) { \
                    printf(m" FAILED\n"); \
                    fflush(stdout); \
                  } \
        sleep(1); \
        assert(c); \
        } \
        while(0);
#else
#define TEST_ASSERT(c,m) 
#endif

static void cb_done (void *ctxt, void * clientdata, pami_result_t err)
{
  int * active = (int *) clientdata;
  (*active)--;
}

pami_context_t * contexts;

pthread_t Progress_thread;

static void * Progress_function(void * dummy)
{
	pami_result_t result = PAMI_ERROR;

	while (1)
	{
        result = PAMI_Context_trylock_advancev(&(contexts[1]), 1, 1000);
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_trylock_advancev");
		usleep(1);
	}

	return NULL;
}

#endif
