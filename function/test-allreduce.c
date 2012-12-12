#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
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
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_query");
  printf("rank %ld is processor %s \n", world_rank, config.value.chararray);
  fflush(stdout);

  config.name = PAMI_CLIENT_NUM_CONTEXTS;
  result = PAMI_Client_query( client, &config, 1);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_query");
  num_contexts = config.value.intval;

  /* initialize the contexts */
  pami_context_t * contexts = NULL;
  contexts = (pami_context_t *) safemalloc( num_contexts * sizeof(pami_context_t) );

  result = PAMI_Context_createv( client, &config, 0, contexts, num_contexts );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_createv");

  /* setup the world geometry */
  pami_geometry_t world_geometry;

  result = PAMI_Geometry_world( client, &world_geometry );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_world");

  pami_xfer_type_t barrier_xfer = PAMI_XFER_BARRIER;
  size_t num_barrier_alg[2];

  /* barrier algs */
  result = PAMI_Geometry_algorithms_num( world_geometry, barrier_xfer, num_barrier_alg );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_num");
  if ( world_rank == 0 ) printf("number of barrier algorithms = {%ld,%ld} \n", num_barrier_alg[0], num_barrier_alg[1] );

  pami_algorithm_t * safe_barrier_algs = (pami_algorithm_t *) safemalloc( num_barrier_alg[0] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * safe_barrier_meta = (pami_metadata_t  *) safemalloc( num_barrier_alg[0] * sizeof(pami_metadata_t)  );
  pami_algorithm_t * fast_barrier_algs = (pami_algorithm_t *) safemalloc( num_barrier_alg[1] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * fast_barrier_meta = (pami_metadata_t  *) safemalloc( num_barrier_alg[1] * sizeof(pami_metadata_t)  );
  result = PAMI_Geometry_algorithms_query( world_geometry, barrier_xfer,
                                           safe_barrier_algs, safe_barrier_meta, num_barrier_alg[0],
                                           fast_barrier_algs, fast_barrier_meta, num_barrier_alg[1] );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_query");

  /* allreduce algs */
  pami_xfer_type_t allreduce_xfer   = PAMI_XFER_ALLREDUCE;
  size_t num_allreduce_alg[2];

  result = PAMI_Geometry_algorithms_num( world_geometry, allreduce_xfer, num_allreduce_alg );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_num");
  if ( world_rank == 0 ) printf("number of allreduce algorithms = {%ld,%ld} \n", num_allreduce_alg[0], num_allreduce_alg[1] );

  pami_algorithm_t * safe_allreduce_algs = (pami_algorithm_t *) safemalloc( num_allreduce_alg[0] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * safe_allreduce_meta = (pami_metadata_t  *) safemalloc( num_allreduce_alg[0] * sizeof(pami_metadata_t)  );
  pami_algorithm_t * fast_allreduce_algs = (pami_algorithm_t *) safemalloc( num_allreduce_alg[1] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * fast_allreduce_meta = (pami_metadata_t  *) safemalloc( num_allreduce_alg[1] * sizeof(pami_metadata_t)  );
  result = PAMI_Geometry_algorithms_query( world_geometry, allreduce_xfer,
                                           safe_allreduce_algs, safe_allreduce_meta, num_allreduce_alg[0],
                                           fast_allreduce_algs, fast_allreduce_meta, num_allreduce_alg[1] );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_query");

  /* perform a reduction */
  volatile int active = 0;

  pami_endpoint_t root;
  PAMI_Endpoint_create(client, (pami_task_t)0, 0, &root);

  int max = (argc>1 ? atoi(argv[1]) : 1000000);

  for ( int d = 1; d < max ; d*=2 )
    for ( size_t b = 0 ; b < num_allreduce_alg[0] ; b++ )
    {
        pami_xfer_t allreduce;

        allreduce.cb_done   = cb_done;
        allreduce.cookie    = (void*) &active;
        allreduce.algorithm = safe_allreduce_algs[b];

        int * sbuf = safemalloc(d*sizeof(int));
        int * rbuf = safemalloc(d*sizeof(int));
        for (int k=0; k<d; k++) sbuf[k]   = 1;
        for (int k=0; k<d; k++) rbuf[k]   = 0;

        allreduce.cmd.xfer_allreduce.op         = PAMI_DATA_SUM;
        allreduce.cmd.xfer_allreduce.sndbuf     = (void*)sbuf;
        allreduce.cmd.xfer_allreduce.stype      = PAMI_TYPE_SIGNED_INT;
        allreduce.cmd.xfer_allreduce.stypecount = d;
        allreduce.cmd.xfer_allreduce.rcvbuf     = (void*)rbuf;
        allreduce.cmd.xfer_allreduce.rtype      = PAMI_TYPE_SIGNED_INT;
        allreduce.cmd.xfer_allreduce.rtypecount = d;

        if ( world_rank == 0 ) printf("trying safe allreduce algorithm %ld (%s) \n", b, safe_allreduce_meta[b].name );
        fflush(stdout);

        active = 1;
        double t0 = PAMI_Wtime(client);
        result = PAMI_Collective( contexts[0], &allreduce );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Collective - allreduce");
        while (active)
          result = PAMI_Context_advance( contexts[0], 1 );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance - allreduce");
        double t1 = PAMI_Wtime(client);

        for (int k=0; k<d; k++) 
          if (rbuf[k]!=world_size) printf("%4d: rbuf[%d] = %d \n", (int)world_rank, k, rbuf[k] );

        free(sbuf);
        free(rbuf);

        if ( world_rank == 0 ) printf("after safe allreduce algorithm %ld (%s) - %d ints took %lf seconds (%lf MB/s) \n",
                                       b, safe_allreduce_meta[b].name, d, t1-t0, 1e-6*d*sizeof(int)/(t1-t0) );
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

