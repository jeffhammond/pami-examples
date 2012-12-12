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

void dispatch_done_cb(pami_context_t context, void * cookie, pami_result_t result)
{
  free(cookie);
  printf("dispatch_done_cb \n");
  fflush(stdout);
  return;
}

void dispatch_recv_cb(pami_context_t context, 
                      void * cookie, 
                      const void * header_addr, size_t header_size,
                      const void * pipe_addr,   
                      size_t data_size,
                      pami_endpoint_t origin,   
                      pami_recv_t * recv)
{
  pami_result_t result = PAMI_ERROR;

  size_t task;
  size_t ctxoff;
  result = PAMI_Endpoint_query(origin, &task, &ctxoff); 
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Endpoint_query");

  int * c = (int *) cookie;
  printf("dispatch_recv_cb: origin = (%ld,%ld) cookie = %d \n", task, ctxoff, *c);

  printf("dispatch_recv_cb: header_size = %ld \n", header_size);
  //const char * h = header_addr;
  //printf("dispatch_recv_cb: header_addr[] = %s \n", h);
  const int * h = header_addr;
  printf("dispatch_recv_cb: header_addr[] = %d \n", *h);
  fflush(stdout);

  if (pipe_addr!=NULL)
  {
    printf("dispatch_recv_cb: immediate protocol \n");
    fflush(stdout);

    printf("dispatch_recv_cb: data_size = %ld \n", data_size);
    const char * p = pipe_addr;
    printf("dispatch_recv_cb: pipe_addr[] = %s \n", p);
    fflush(stdout);
  }
  else
  {
    printf("dispatch_recv_cb: asychronous protocol \n");
    printf("dispatch_recv_cb: data_size = %ld \n", data_size);
    fflush(stdout);

    char * buffer = safemalloc(data_size);

    recv->cookie      = buffer;
    recv->local_fn    = dispatch_done_cb;
    recv->addr        = buffer;
    recv->type        = PAMI_TYPE_BYTE;
    recv->offset      = 0;
    recv->data_fn     = PAMI_DATA_COPY;
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

  int status = pthread_create(&Progress_thread, NULL, &Progress_function, NULL);
  TEST_ASSERT(status==0, "pthread_create");

  /************************************************************************/

  int n = (argc>1 ? atoi(argv[1]) : 1000);

  size_t bytes = n * sizeof(int);

  int *  local  = (int *) safemalloc(bytes);
  for (int i=0; i<n; i++)
    local[i] = world_rank;

  /* create the endpoint */
  int target = (world_rank>0 ? world_rank-1 : world_size-1);
  pami_endpoint_t target_ep;
  result = PAMI_Endpoint_create(client, (pami_task_t) target, 1, &target_ep);
  //result = PAMI_Endpoint_create(client, (pami_task_t) target, 0, &target_ep);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Endpoint_create");

  /* register the dispatch function */
  pami_dispatch_callback_function dispatch_cb;
  size_t dispatch_id                 = 37;
  dispatch_cb.p2p                    = dispatch_recv_cb;
  pami_dispatch_hint_t dispatch_hint = {0};
  int dispatch_cookie                = 1000000+world_rank;
  //dispatch_hint.recv_immediate       = PAMI_HINT_DISABLE;
  result = PAMI_Dispatch_set(contexts[0], dispatch_id, dispatch_cb, &dispatch_cookie, dispatch_hint);
  result = PAMI_Dispatch_set(contexts[1], dispatch_id, dispatch_cb, &dispatch_cookie, dispatch_hint);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Dispatch_set");

  /* make sure everything is ready */
  result = barrier(world_geometry, contexts[0]);
  TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

  // The iovec structure describes a buffer. It contains two fields:
  // void *iov_base - Contains the address of a buffer. 
  // size_t iov_len - Contains the length of the buffer.

  int header  = 37373;

  int active = 1;
  pami_send_t parameters;
  parameters.send.header.iov_base = &header;
  parameters.send.header.iov_len  = sizeof(int);
  parameters.send.data.iov_base   = local;
  parameters.send.data.iov_len    = bytes;
  parameters.send.dispatch        = dispatch_id;
  //parameters.send.hints           = ;
  parameters.send.dest            = target_ep;
  parameters.events.cookie        = &active;
  parameters.events.local_fn      = cb_done;
  parameters.events.remote_fn     = NULL;//cb_done;

  uint64_t t0 = GetTimeBase();

  result = PAMI_Send(contexts[0], &parameters);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Send");

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

  printf("%ld: PAMI_Send of %ld bytes achieves %lf MB/s \n", (long)world_rank, bytes, 1.6e9*1e-6*(double)bytes/(double)dt );
  fflush(stdout);

  result = barrier(world_geometry, contexts[0]);
  TEST_ASSERT(result == PAMI_SUCCESS,"barrier");

  free(local);

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

