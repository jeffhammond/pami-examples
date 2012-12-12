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

  int n = (argc>1 ? atoi(argv[1]) : 1000000);

  size_t bytes = n * sizeof(int), bytes_out;
  int *  shared = (int *) safemalloc(bytes);
  for (int i=0; i<n; i++)
    shared[i] = -1;

  pami_memregion_t shared_mr;
  result = PAMI_Memregion_create(contexts[1], shared, bytes, &bytes_out, &shared_mr);
  TEST_ASSERT(result == PAMI_SUCCESS && bytes==bytes_out,"PAMI_Memregion_create");

  int *  local  = (int *) safemalloc(bytes);
  for (int i=0; i<n; i++)
    local[i] = world_rank;

  pami_memregion_t local_mr;
  result = PAMI_Memregion_create(contexts[0], local, bytes, &bytes_out, &local_mr);
  TEST_ASSERT(result == PAMI_SUCCESS && bytes==bytes_out,"PAMI_Memregion_create");

  result = barrier(world_geometry, contexts[0]);
  TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

  pami_memregion_t * shmrs = (pami_memregion_t *) safemalloc( world_size * sizeof(pami_memregion_t) );

  result = allgather(world_geometry, contexts[0], sizeof(pami_memregion_t), &shared_mr, shmrs);
  TEST_ASSERT(result == PAMI_SUCCESS,"allgather");

  int target = (world_rank>0 ? world_rank-1 : world_size-1);
  pami_endpoint_t target_ep;
  result = PAMI_Endpoint_create(client, (pami_task_t) target, 1, &target_ep);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Endpoint_create");

  result = barrier(world_geometry, contexts[0]);
  TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

  int active = 2;
  pami_rput_simple_t parameters;
  parameters.rma.dest     		= target_ep;
  //parameters.rma.hints    	  = ;
  parameters.rma.bytes    		= bytes;
  parameters.rma.cookie   		= &active;
  parameters.rma.done_fn  		= cb_done;
  parameters.rdma.local.mr      = &local_mr;
  parameters.rdma.local.offset  = 0;
  parameters.rdma.remote.mr     = &shmrs[target];
  parameters.rdma.remote.offset = 0;
  parameters.put.rdone_fn       = cb_done;

  uint64_t t0 = GetTimeBase();

  result = PAMI_Rput(contexts[0], &parameters);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Rput");

  while (active)
  {
    //result = PAMI_Context_advance( contexts[0], 100);
    //TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance");
    result = PAMI_Context_trylock_advancev(&(contexts[0]), 1, 1000);
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_trylock_advancev");
  }

  uint64_t t1 = GetTimeBase();
  uint64_t dt = t1-t0;

  /* barrier on non-progressing context to make sure CHT does its job */
  barrier(world_geometry, contexts[0]);

  printf("%ld: PAMI_Rput of %ld bytes achieves %lf MB/s \n", (long)world_rank, bytes, 1.6e9*1e-6*(double)bytes/(double)dt );
  fflush(stdout);

  int errors = 0;
  
  target = (world_rank<(world_size-1) ? world_rank+1 : 0);
  for (int i=0; i<n; i++)
    if (shared[i] != target)
       errors++;

  if (errors>0)
    for (int i=0; i<n; i++)
      if (shared[i] != target)
        printf("%ld: shared[%d] = %d (%d) \n", (long)world_rank, i, shared[i], target);
  else
    printf("%ld: no errors :-) \n", (long)world_rank); 

  fflush(stdout);

  result = barrier(world_geometry, contexts[0]);
  TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

  result = PAMI_Memregion_destroy(contexts[0], &shared_mr);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Memregion_destroy");

  result = PAMI_Memregion_destroy(contexts[0], &local_mr);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Memregion_destroy");

  free(shmrs);
  free(local);
  free(shared);

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

