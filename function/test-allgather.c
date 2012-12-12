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

  /* allgather algs */
  pami_xfer_type_t allgather_xfer   = PAMI_XFER_ALLGATHER;
  size_t num_allgather_alg[2];

  result = PAMI_Geometry_algorithms_num( world_geometry, allgather_xfer, num_allgather_alg );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_num");
  if ( world_rank == 0 ) printf("number of allgather algorithms = {%ld,%ld} \n", num_allgather_alg[0], num_allgather_alg[1] );

  pami_algorithm_t * safe_allgather_algs = (pami_algorithm_t *) safemalloc( num_allgather_alg[0] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * safe_allgather_meta = (pami_metadata_t  *) safemalloc( num_allgather_alg[0] * sizeof(pami_metadata_t)  );
  pami_algorithm_t * fast_allgather_algs = (pami_algorithm_t *) safemalloc( num_allgather_alg[1] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * fast_allgather_meta = (pami_metadata_t  *) safemalloc( num_allgather_alg[1] * sizeof(pami_metadata_t)  );
  result = PAMI_Geometry_algorithms_query( world_geometry, allgather_xfer,
                                           safe_allgather_algs, safe_allgather_meta, num_allgather_alg[0],
                                           fast_allgather_algs, fast_allgather_meta, num_allgather_alg[1] );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_query");

  /* perform a gather */
  volatile int active = 0;

  pami_endpoint_t root;
  PAMI_Endpoint_create(client, (pami_task_t)0, 0, &root);

  int max = (argc>1 ? atoi(argv[1]) : 1000000);
  max /= world_size;

  for ( int d = 1; d < max ; d*=2 )
    for ( size_t b = 0 ; b < num_allgather_alg[0] ; b++ )
    {
        pami_xfer_t allgather;

        allgather.cb_done   = cb_done;
        allgather.cookie    = (void*) &active;
        allgather.algorithm = safe_allgather_algs[b];

        int * sbuf = safemalloc(d*sizeof(int));
        int * rbuf = safemalloc(world_size*d*sizeof(int));
        for (int k=0; k<d; k++)              sbuf[k]   = world_rank;
        for (int k=0; k<(world_size*d); k++) rbuf[k]   = 0;

        allgather.cmd.xfer_allgather.sndbuf     = (void*)sbuf;
        allgather.cmd.xfer_allgather.stype      = PAMI_TYPE_SIGNED_INT;
        allgather.cmd.xfer_allgather.stypecount = d;
        allgather.cmd.xfer_allgather.rcvbuf     = (void*)rbuf;
        allgather.cmd.xfer_allgather.rtype      = PAMI_TYPE_SIGNED_INT;
        allgather.cmd.xfer_allgather.rtypecount = d;

        if ( world_rank == 0 ) printf("trying safe allgather algorithm %ld (%s) \n", b, safe_allgather_meta[b].name );
        fflush(stdout);

        active = 1;
        double t0 = PAMI_Wtime(client);
        result = PAMI_Collective( contexts[0], &allgather );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Collective - allgather");
        while (active)
          result = PAMI_Context_advance( contexts[0], 1 );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance - allgather");
        double t1 = PAMI_Wtime(client);

        for (int s=0; s<world_size; s++ )
          for (int k=0; k<d; k++)
            if (rbuf[s*d+k]!=s) printf("%4d: rbuf[%d] = %d \n", (int)world_rank, s*d+k, rbuf[s*d+k] );

        free(sbuf);
        free(rbuf);

        if ( world_rank == 0 ) printf("after safe allgather algorithm %ld (%s) - %d ints took %lf seconds (%lf MB/s) \n",
                                       b, safe_allgather_meta[b].name, (int)world_size*d, t1-t0, 1e-6*world_size*d*sizeof(int)/(t1-t0) );
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
