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
  size_t num_contexts = -1;

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

  /* bcast algs */
  pami_xfer_type_t bcast_xfer   = PAMI_XFER_BROADCAST;
  size_t num_bcast_alg[2];

  result = PAMI_Geometry_algorithms_num( world_geometry, bcast_xfer, num_bcast_alg );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_num");
  if ( world_rank == 0 ) printf("number of bcast algorithms = {%ld,%ld} \n", num_bcast_alg[0], num_bcast_alg[1] );

  pami_algorithm_t * safe_bcast_algs = (pami_algorithm_t *) safemalloc( num_bcast_alg[0] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * safe_bcast_meta = (pami_metadata_t  *) safemalloc( num_bcast_alg[0] * sizeof(pami_metadata_t)  );
  pami_algorithm_t * fast_bcast_algs = (pami_algorithm_t *) safemalloc( num_bcast_alg[1] * sizeof(pami_algorithm_t) );
  pami_metadata_t  * fast_bcast_meta = (pami_metadata_t  *) safemalloc( num_bcast_alg[1] * sizeof(pami_metadata_t)  );
  result = PAMI_Geometry_algorithms_query( world_geometry, bcast_xfer,
                                           safe_bcast_algs, safe_bcast_meta, num_bcast_alg[0],
                                           fast_bcast_algs, fast_bcast_meta, num_bcast_alg[1] );
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Geometry_algorithms_query");

  /* perform a bcast */
  volatile int active = 0;

  pami_endpoint_t root;
  PAMI_Endpoint_create(client, (pami_task_t)0, 0, &root);

  int max = (argc>1 ? atoi(argv[1]) : 1000000);
  //int d = max;

  for ( int d = 1; d < max ; d*=2 )
    //for ( size_t b = 0 ; b < 12 /*num_bcast_alg[0]*/ ; b++ )
    for ( size_t b = 0 ; b < num_bcast_alg[0] ; b++ )
    {
        pami_xfer_t bcast;

        bcast.cb_done   = cb_done;
        bcast.cookie    = (void*) &active;
        bcast.algorithm = safe_bcast_algs[b];

        int * buf = safemalloc(d*sizeof(int));
        for (int k=0; k<d; k++) buf[k]   = world_rank;

        bcast.cmd.xfer_broadcast.root      = root;
        bcast.cmd.xfer_broadcast.buf       = (void*)buf;
        bcast.cmd.xfer_broadcast.type      = PAMI_TYPE_SIGNED_INT;
        bcast.cmd.xfer_broadcast.typecount = d;

        if ( world_rank == 0 ) printf("trying safe bcast algorithm %ld (%s) \n", b, safe_bcast_meta[b].name );
        fflush(stdout);

        active = 1;
        double t0 = PAMI_Wtime(client);
        result = PAMI_Collective( contexts[0], &bcast );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Collective - bcast");
        while (active)
          result = PAMI_Context_advance( contexts[0], 1 );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance - bcast");
        double t1 = PAMI_Wtime(client);

        for (int k=0; k<d; k++) 
          if (buf[k]!=0) printf("%4d: buf[%d] = %d \n", (int)world_rank, k, buf[k] );

        free(buf);

        if ( world_rank == 0 ) printf("after safe bcast algorithm %ld (%s) - %d ints took %lf seconds (%lf MB/s) \n", 
                                       b, safe_bcast_meta[b].name, d, t1-t0, 1e-6*d*sizeof(int)/(t1-t0) );
        fflush(stdout);
    }

#if 1
  for ( int d = 1; d < 64 ; d*=2 ) /* fast (shortMU) algorithms fail for >64 bytes */
    for ( size_t b = 0 ; b < 10 /* num_bcast_alg[1] */ ; b++ )
    {
        pami_xfer_t bcast;

        bcast.cb_done   = cb_done;
        bcast.cookie    = (void*) &active;
        bcast.algorithm = fast_bcast_algs[b];

        int * buf = safemalloc(d*sizeof(int));
        for (int k=0; k<d; k++) buf[k]   = world_rank;

        bcast.cmd.xfer_broadcast.root      = root;
        bcast.cmd.xfer_broadcast.buf       = (void*)buf;
        bcast.cmd.xfer_broadcast.type      = PAMI_TYPE_SIGNED_INT;
        bcast.cmd.xfer_broadcast.typecount = d;

        if ( world_rank == 0 ) printf("trying fast bcast algorithm %ld (%s) \n", b, fast_bcast_meta[b].name );
        fflush(stdout);

        active = 1;
        double t0 = PAMI_Wtime(client);
        result = PAMI_Collective( contexts[0], &bcast );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Collective - bcast");
        while (active)
          result = PAMI_Context_advance( contexts[0], 1 );
        TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Context_advance - bcast");
        double t1 = PAMI_Wtime(client);

        for (int k=0; k<d; k++) 
          if (buf[k]!=0) printf("%4d: buf[%d] = %d \n", (int)world_rank, k, buf[k] );

        free(buf);

        if ( world_rank == 0 ) printf("after fast bcast algorithm %ld (%s) - %d ints took %lf seconds (%lf MB/s) \n", 
                                       b, fast_bcast_meta[b].name, d, t1-t0, 1e-6*d*sizeof(int)/(t1-t0) );
        fflush(stdout);
    }
#endif

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

