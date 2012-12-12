#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <pami.h>

int main(int argc, char* argv[])
{
  pami_result_t result = PAMI_ERROR;

  /* initialize the client */
  char * clientname = "";
  pami_client_t client;
  result = PAMI_Client_create (clientname, &client, NULL, 0);
  assert(result == PAMI_SUCCESS);

  /* query properties of the client */
  pami_configuration_t config;
  int world_size, world_rank;
  int num_contexts;

  config.name = PAMI_CLIENT_NUM_TASKS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  world_size = config.value.intval;

  config.name = PAMI_CLIENT_TASK_ID;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  world_rank = config.value.intval;
  printf("hello world from rank %d of %d \n",world_rank,world_size);
  fflush(stdout);
  sleep(1);

  config.name = PAMI_CLIENT_NUM_CONTEXTS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  num_contexts = config.value.intval;

  /* initialize the contexts */
  pami_context_t * contexts;
  contexts = (pami_context_t *) malloc( num_contexts * sizeof(pami_context_t) );

  result = PAMI_Context_createv(client, &config, 0, contexts, num_contexts);
  assert(result == PAMI_SUCCESS);

  printf("%d contexts were created by rank %d \n",num_contexts,world_rank);
  fflush(stdout);
  sleep(1);

  result = PAMI_Context_destroyv(contexts, num_contexts);
  assert(result == PAMI_SUCCESS);

  free(contexts);

  result = PAMI_Client_destroy(&client);
  assert(result == PAMI_SUCCESS);

  printf("end of test \n");
  fflush(stdout);
  sleep(1);

  return 0;
}

