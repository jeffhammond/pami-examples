#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <pami.h>

//#define SLEEP sleep
#define SLEEP usleep

int main(int argc, char* argv[])
{
  pami_result_t        result        = PAMI_ERROR;

  /* initialize the client */
  char * clientname = "";
  pami_client_t client;
  result = PAMI_Client_create (clientname, &client, NULL, 0);
  assert(result == PAMI_SUCCESS);

  /* query properties of the client */
  pami_configuration_t config;

  config.name = PAMI_CLIENT_TASK_ID;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_TASK_ID = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_NUM_TASKS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_NUM_TASKS = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_NUM_CONTEXTS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_NUM_CONTEXTS = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_CONST_CONTEXTS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_CONST_CONTEXTS = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_NUM_LOCAL_TASKS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  size_t num_local_tasks = config.value.intval;
  printf("PAMI_CLIENT_NUM_LOCAL_TASKS = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_LOCAL_TASKS;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  if ( num_local_tasks == 1 ) printf("PAMI_CLIENT_LOCAL_TASKS = %ld \n", config.value.intarray[0] );
  else if ( num_local_tasks > 1  ) printf("PAMI_CLIENT_LOCAL_TASKS = %ld .. %ld \n", config.value.intarray[0], config.value.intarray[num_local_tasks-1] );
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_HWTHREADS_AVAILABLE;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_HWTHREADS_AVAILABLE = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_MEMREGION_SIZE;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_MEMREGION_SIZE = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_MEM_SIZE;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_MEM_SIZE = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_PROCESSOR_NAME;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_PROCESSOR_NAME = %s \n", config.value.chararray);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_WTICK;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_WTICK = %e \n", config.value.doubleval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_WTIMEBASE_MHZ;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_WTIMEBASE_MHZ = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  config.name = PAMI_CLIENT_CLOCK_MHZ;
  result = PAMI_Client_query( client, &config,1);
  assert(result == PAMI_SUCCESS);
  printf("PAMI_CLIENT_CLOCK_MHZ = %ld \n", config.value.intval);
  fflush(stdout);
  SLEEP(1);

  /* finalize the client */
  result = PAMI_Client_destroy(&client);
  assert(result == PAMI_SUCCESS);

  printf("end of test \n");
  fflush(stdout);
  SLEEP(1);

  return 0;
}

