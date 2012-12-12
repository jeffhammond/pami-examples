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

  int status = pthread_create(&Progress_thread, NULL, &Progress_function, NULL);
  TEST_ASSERT(status==0, "pthread_create");

  /************************************************************************/

    size_t bytes = sizeof(int);

    int *  shared = (int *) safemalloc(bytes);
    int *  local  = (int *) safemalloc(bytes);
    int *  value  = (int *) safemalloc(bytes);

    int ** shptrs = (int **) safemalloc( world_size * sizeof(int *) );
 
    result = allgather(world_geometry, contexts[0], sizeof(int*), &shared, shptrs);
    TEST_ASSERT(result == PAMI_SUCCESS,"allgather");
 
    int target = 0;
    pami_endpoint_t target_ep;
    result = PAMI_Endpoint_create(client, (pami_task_t) target, 1, &target_ep);
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Endpoint_create");
 
    result = barrier(world_geometry, contexts[0]);
    TEST_ASSERT(result == PAMI_SUCCESS,"barrier");
 
    int active = 0;
    pami_rmw_t parameters;
    parameters.dest      = target_ep;
    //parameters.hints    = ;
    parameters.cookie    = &active;
    parameters.done_fn   = cb_done;
    parameters.local     = local;
    parameters.remote    = shptrs[target];
    parameters.value     = value;
    parameters.test      = value; /* unused */
    parameters.operation = PAMI_ATOMIC_FETCH_ADD;
    parameters.type      = PAMI_TYPE_SIGNED_INT;
 
    /* PAMI_ATOMIC_FETCH_ADD : local=remote and remote+=value */
 
    //for (int n=1; n<=67108864; n*=2)
    for (int n=8; n<=65536; n*=8)
    {
      shared[0] = 0;
      local[0] = 0;
      value[0] = 1;

      active = n;
 
      uint64_t t0 = GetTimeBase();

      for (int i=0; i<n; i++)
      {
        result = PAMI_Rmw(contexts[0], &parameters);
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Rmw");
      }

      while (active)
      {
        result = PAMI_Context_trylock_advancev(&(contexts[0]), 1, 1000);
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_trylock_advancev");
      }
     
      uint64_t t1 = GetTimeBase();
      uint64_t dt = t1-t0;
     
      /* barrier on non-progressing context to make sure CHT does its job */
      barrier(world_geometry, contexts[0]);
     
      printf("%6ld: PAMI_Rmw %d times: local = %12d shared = %12d in %20llu cycles = %13.6lf seconds \n", 
             (long)world_rank, n, local[0], shared[0], (long long unsigned) dt, dt/1.6e9 );
      fflush(stdout);
    }
    
    free(shared);
    free(local);
    free(value);

  /************************************************************************/

  void * rv;

  status = pthread_cancel(Progress_thread);
  TEST_ASSERT(status==0, "pthread_cancel");

  status = pthread_join(Progress_thread, &rv);
  TEST_ASSERT(status==0, "pthread_join");

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

