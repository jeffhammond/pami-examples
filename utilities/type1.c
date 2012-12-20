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

  char * client1name = "";
  pami_client_t client;
  result = PAMI_Client_create(client1name, &client, NULL, 0);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_create");

  uint64_t t0 = GetTimeBase();
  const int rep = 100;
  pami_type_t * type = safemalloc(rep*sizeof(pami_type_t));
  for (int i=0; i<rep; i++)
  {
    result = PAMI_Type_create(&(type[i]));
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Type_create");
    result = PAMI_Type_add_simple(type[i],
                                  100*sizeof(double),  /* bytes */
                                  0,                   /* offset */
                                  100,                 /* count */
                                  400*sizeof(double)); /* stride */
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Type_add_simple");
    result = PAMI_Type_complete(type[i], sizeof(double));
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Type_complete");
    result = PAMI_Type_destroy(&(type[i]));
    TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Type_destroy");
  }
  uint64_t t1 = GetTimeBase();
  uint64_t dt = t1-t0;
  printf("PAMI_Type_add_simple took %llu cycles = %lf microseconds \n", dt/rep, dt/rep/1.6e3);

  free(type);

  result = PAMI_Client_destroy(&client);
  TEST_ASSERT(result == PAMI_SUCCESS,"PAMI_Client_destroy");

  printf("end of test \n");
  fflush(stdout);

  return 0;
}

