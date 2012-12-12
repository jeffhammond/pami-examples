#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <hwi/include/bqc/A2_inlines.h>
#include <pami.h>

#include "safemalloc.h"
#include "preamble.h"

int main(int argc, char* argv[])
{
  pami_result_t result = PAMI_ERROR;
  size_t world_size, world_rank;

  /* initialize the client */
  char * clientname = "";
  pami_client_t client;
  result = PAMI_Client_create( clientname, &client, NULL, 0 );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_create");

  /* query properties of the client */
  pami_configuration_t config;
  size_t num_contexts;

  config.name = PAMI_CLIENT_TASK_ID;
  result = PAMI_Client_query( client, &config, 1);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_query");
  world_rank = config.value.intval;

  config.name = PAMI_CLIENT_NUM_TASKS;
  result = PAMI_Client_query( client, &config, 1);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_query");
  world_size = config.value.intval;

  if ( world_rank == 0 ) 
  {
    printf("starting test on %ld ranks \n", world_size);
    fflush(stdout);
  }

  config.name = PAMI_CLIENT_PROCESSOR_NAME;
  result = PAMI_Client_query( client, &config, 1);
  assert(result == PAMI_SUCCESS);
  printf("rank %ld is processor %s \n", world_rank, config.value.chararray);
  fflush(stdout);

  config.name = PAMI_CLIENT_NUM_CONTEXTS;
  result = PAMI_Client_query( client, &config, 1);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_query");
  num_contexts = config.value.intval;

  /* initialize the contexts */
  pami_context_t * contexts = NULL;
  contexts = (pami_context_t *) malloc( num_contexts * sizeof(pami_context_t) ); assert(contexts!=NULL);

  result = PAMI_Context_createv( client, &config, 0, contexts, num_contexts );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_createv");

  /* setup the world geometry */
  pami_geometry_t world_geometry;
  pami_xfer_type_t barrier_xfer = PAMI_XFER_BARRIER;
  size_t num_alg[2];
  pami_algorithm_t * safe_barrier_algs = NULL;
  pami_metadata_t  * safe_barrier_meta = NULL;
  pami_algorithm_t * fast_barrier_algs = NULL;
  pami_metadata_t  * fast_barrier_meta = NULL;

  result = PAMI_Geometry_world( client, &world_geometry );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_world");

  result = PAMI_Geometry_algorithms_num( world_geometry, barrier_xfer, num_alg );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_num");
  if ( world_rank == 0 ) printf("number of barrier algorithms = {%ld,%ld} \n", num_alg[0], num_alg[1] );

  safe_barrier_algs = (pami_algorithm_t *) malloc( num_alg[0] * sizeof(pami_algorithm_t) ); assert(safe_barrier_algs!=NULL);
  safe_barrier_meta = (pami_metadata_t  *) malloc( num_alg[0] * sizeof(pami_metadata_t)  ); assert(safe_barrier_meta!=NULL);
  fast_barrier_algs = (pami_algorithm_t *) malloc( num_alg[1] * sizeof(pami_algorithm_t) ); assert(fast_barrier_algs!=NULL);
  fast_barrier_meta = (pami_metadata_t  *) malloc( num_alg[1] * sizeof(pami_metadata_t)  ); assert(fast_barrier_meta!=NULL);
  result = PAMI_Geometry_algorithms_query( world_geometry, barrier_xfer,
                                           safe_barrier_algs, safe_barrier_meta, num_alg[0],
                                           fast_barrier_algs, fast_barrier_meta, num_alg[1] );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_query");

  /* perform a barrier */
  size_t b;
  pami_xfer_t barrier;
  volatile int active = 0;

  for ( b = 0 ; b < num_alg[0] ; b++ )
  {
      barrier.cb_done   = cb_done;
      barrier.cookie    = (void*) &active;
      barrier.algorithm = safe_barrier_algs[b];

      if ( world_rank == 0 ) printf("trying safe barrier algorithm %ld (%s) \n", b, safe_barrier_meta[b].name );
      fflush(stdout);

      uint64_t t0 = GetTimeBase();
      active = 1;
      result = PAMI_Collective( contexts[0], &barrier );
      TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Collective - barrier");
      while (active)
        result = PAMI_Context_advance( contexts[0], 1 );
      TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance - barrier");
      uint64_t t1 = GetTimeBase();

      if ( world_rank == 0 ) printf("after safe barrier algorithm %ld (%s) - took %llu cycles \n", b, safe_barrier_meta[b].name, t1-t0 );
      fflush(stdout);
  }
  for ( b = 0 ; b < num_alg[1] ; b++ )
  {
      barrier.cb_done   = cb_done;
      barrier.cookie    = (void*) &active;
      barrier.algorithm = fast_barrier_algs[b];

      if ( world_rank == 0 ) printf("trying fast barrier algorithm %ld (%s) \n", b, fast_barrier_meta[b].name );
      fflush(stdout);

      uint64_t t0 = GetTimeBase();
      active = 1;
      result = PAMI_Collective( contexts[0], &barrier );
      TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Collective - barrier");
      while (active)
        result = PAMI_Context_advance( contexts[0], 1 );
      TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance - barrier");
      uint64_t t1 = GetTimeBase();

      if ( world_rank == 0 ) printf("after fast barrier algorithm %ld (%s) - took %llu cycles \n", b, fast_barrier_meta[b].name, t1-t0 );
      fflush(stdout);
  }

  /* finalize the contexts */
  result = PAMI_Context_destroyv( contexts, num_contexts );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_destroyv");

  free(contexts);

  /* finalize the client */
  result = PAMI_Client_destroy( &client );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_destroy");

  if ( world_rank == 0 ) 
  {
    printf("end of test \n");
    fflush(stdout);
  }

  return 0;
}

