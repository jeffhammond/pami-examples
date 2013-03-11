#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <pami.h>
#include <hwi/include/bqc/A2_inlines.h>

#include "safemalloc.h"
#include "preamble.h"
#include "coll.h"

#define DO_ACCUMULATE

typedef struct {
  void * target_addr;
  double scaling;
} remote_acc_info_t;

static void dispatch_recv_cb(pami_context_t context,
                             void * cookie,
                             const void * header_addr, size_t header_size,
                             const void * pipe_addr,
                             size_t data_size,
                             pami_endpoint_t origin,
                             pami_recv_t * recv)
{
  //old void ** h = (void **)header_addr;

  if (sizeof(remote_acc_info_t) != header_size)
    printf("something is wrong \n");

  remote_acc_info_t * info = (remote_acc_info_t *) header_addr;

  void * h = info->target_addr;
  double s = info->scaling;

  if (pipe_addr!=NULL)
  {
#ifdef DO_ACCUMULATE
    size_t count = data_size/sizeof(double);
    //old double * target_data = *h;
    double * target_data = h;
    const double * pipe_data = (const double *) pipe_addr;
    for (size_t i=0; i<count; i++)
      target_data[i] += s*pipe_data[i];
#else
    //old memcpy(*h, pipe_addr, data_size);
    memcpy(h, pipe_addr, data_size);
#endif
  }
  else
  {
    if (s != 1.0)
      printf("not supported \n");

    recv->cookie      = 0;
    recv->local_fn    = NULL;
    //old recv->addr        = *h;
    recv->addr        = h;
    recv->type        = PAMI_TYPE_DOUBLE;
    recv->offset      = 0;
#ifdef DO_ACCUMULATE
    recv->data_fn     = PAMI_DATA_SUM;
#else
    recv->data_fn     = PAMI_DATA_COPY;
#endif
    recv->data_cookie = NULL;
  }

  return;
}

int main(int argc, char* argv[])
{
  pami_result_t result = PAMI_ERROR;
  size_t world_size, world_rank;

  /* initialize the second client */
  char * clientname = "";
  pami_client_t client;
  result = PAMI_Client_create(clientname, &client, NULL, 0);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_create");

  /* query properties of the client */
  pami_configuration_t config[3];
  size_t num_contexts;

  config[0].name = PAMI_CLIENT_NUM_TASKS;
  config[1].name = PAMI_CLIENT_TASK_ID;
  config[2].name = PAMI_CLIENT_NUM_CONTEXTS;
  result = PAMI_Client_query(client, config, 3);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_query");
  world_size   = config[0].value.intval;
  world_rank   = config[1].value.intval;
  num_contexts = config[2].value.intval;
  TEST_ASSERT(num_contexts>1,"num_contexts>1");

  if (world_rank==0)
  {
    printf("hello world from rank %ld of %ld \n", world_rank, world_size );
    fflush(stdout);
  }

  /* initialize the contexts */
  contexts = (pami_context_t *) safemalloc( num_contexts * sizeof(pami_context_t) );

  result = PAMI_Context_createv( client, NULL, 0, contexts, num_contexts );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_createv");

  /* setup the world geometry */
  pami_geometry_t world_geometry;
  result = PAMI_Geometry_world(client, &world_geometry );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_world");

#ifdef PROGRESS_THREAD
  int status = pthread_create(&Progress_thread, NULL, &Progress_function, NULL);
  TEST_ASSERT(status==0, "pthread_create");
#endif

  /************************************************************************/

  /* register the dispatch function */
  pami_dispatch_callback_function dispatch_cb;
  size_t dispatch_id                 = 37;
  dispatch_cb.p2p                    = dispatch_recv_cb;
  pami_dispatch_hint_t dispatch_hint = {0};
  int dispatch_cookie                = 1000000+world_rank;
  //dispatch_hint.recv_immediate       = PAMI_HINT_DISABLE;
  result = PAMI_Dispatch_set(contexts[0], dispatch_id, dispatch_cb, &dispatch_cookie, dispatch_hint);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Dispatch_set");
  result = PAMI_Dispatch_set(contexts[1], dispatch_id, dispatch_cb, &dispatch_cookie, dispatch_hint);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Dispatch_set");

  for (int n=1; n<=16777216; n*=2)
  {
    size_t bytes = n * sizeof(double);
    double *  shared = (double *) safemalloc(bytes);
    for (int i=0; i<n; i++)
#ifdef DO_ACCUMULATE
      shared[i] = -10.0;
#else
      shared[i] = 0.0;
#endif

    double *  local  = (double *) safemalloc(bytes);
    for (int i=0; i<n; i++)
      local[i] = (double)world_rank;

    double ** shptrs = (double **) safemalloc( world_size * sizeof(double *) );

    result = allgather(world_geometry, contexts[0], sizeof(double*), &shared, shptrs);
    TEST_ASSERT(result == PAMI_SUCCESS,"allgather");

    int target = (world_rank>0 ? world_rank-1 : world_size-1);
    pami_endpoint_t target_ep;
    result = PAMI_Endpoint_create(client, (pami_task_t) target, 1, &target_ep);
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Endpoint_create");

    result = barrier(world_geometry, contexts[0]);
    TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

    int active = 2;
    pami_send_t parameters;

    remote_acc_info_t info;
    info.target_addr = &(shptrs[target]);
    info.scaling     = 1.0;
    parameters.send.header.iov_base = &info;
    parameters.send.header.iov_len  = sizeof(remote_acc_info_t);
    //old parameters.send.header.iov_base = &(shptrs[target]);
    //old parameters.send.header.iov_len  = sizeof(void *);
    parameters.send.data.iov_base   = local;
    parameters.send.data.iov_len    = bytes;
    parameters.send.dispatch        = dispatch_id;
    //parameters.send.hints           = ;
    parameters.send.dest            = target_ep;
    parameters.events.cookie        = &active;
    parameters.events.local_fn      = cb_done;
    parameters.events.remote_fn     = cb_done;

    uint64_t t0 = GetTimeBase();

    result = PAMI_Send(contexts[0], &parameters);
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Send");

    while (active>1)
    {
      result = PAMI_Context_trylock_advancev(&(contexts[0]), 1, 1000);
      TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_trylock_advancev");
    }

    uint64_t t1 = GetTimeBase();
    uint64_t dt = t1-t0;

    while (active>0)
    {
      result = PAMI_Context_trylock_advancev(&(contexts[0]), 1, 1000);
      TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_trylock_advancev");
    }

#ifdef PROGRESS_THREAD
    /* barrier on non-progressing context to make sure CHT does its job */
    barrier(world_geometry, contexts[0]);
#else
    /* barrier on remote context since otherwise put cannot complete */
    barrier(world_geometry, contexts[1]);
#endif

    printf("%ld: PAMI_Send of %ld bytes achieves %lf MB/s \n", (long)world_rank, bytes, 1.6e9*1e-6*(double)bytes/(double)dt );
    fflush(stdout);

    int errors = 0;
    
    target = (world_rank<(world_size-1) ? world_rank+1 : 0);
    for (int i=0; i<n; i++)
#ifdef DO_ACCUMULATE
      if (shared[i] != ((double)target-10.0))
#else
      if (shared[i] != (double)target)
#endif
         errors++;

    if (errors>0)
      for (int i=0; i<n; i++)
        if (shared[i] != (double)target)
          printf("%ld: shared[%d] = %lf (%lf) \n", (long)world_rank, i, shared[i], (double)target);
    else
      printf("%ld: no errors :-) \n", (long)world_rank); 

    fflush(stdout);

    if (errors>0)
      exit(13);

    result = barrier(world_geometry, contexts[0]);
    TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

    free(shptrs);
    free(local);
    free(shared);
  }

  /************************************************************************/

#ifdef PROGRESS_THREAD
  void * rv;

  status = pthread_cancel(Progress_thread);
  TEST_ASSERT(status==0, "pthread_cancel");

  status = pthread_join(Progress_thread, &rv);
  TEST_ASSERT(status==0, "pthread_join");
#endif

  result = barrier(world_geometry, contexts[0]);
  TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

  /* finalize the contexts */
  result = PAMI_Context_destroyv( contexts, num_contexts );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_destroyv");

  free(contexts);

  /* finalize the client */
  result = PAMI_Client_destroy( &client );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_destroy");

  if (world_rank==0)
    printf("%ld: end of test \n", world_rank );
  fflush(stdout);

  return 0;
}

