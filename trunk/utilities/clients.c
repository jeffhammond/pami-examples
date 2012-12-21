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

  char * envvar = getenv("PAMI_CLIENTS");
  if (envvar == NULL)
  {
    printf("You did not set the environment variable PAMI_CLIENTS. \n");
    printf("Please set PAMI_CLIENTS=CLIENT1,CLIENT2,CLIENT3 so this test will work as designed. \n");
    fflush(stdout);
    exit(1);
  }

  /* initialize the client1 */
  char * client1name = "";
  pami_client_t client1;
  result = PAMI_Client_create(client1name, &client1, NULL, 0);
  assert(result == PAMI_SUCCESS);

  char * client2name = "";
  pami_client_t client2;
  result = PAMI_Client_create(client2name, &client2, NULL, 0);
  assert(result == PAMI_SUCCESS);

  char * client3name = "";
  pami_client_t client3;
  result = PAMI_Client_create(client3name, &client3, NULL, 0);
  assert(result == PAMI_SUCCESS);

  /* finalize the client1 */
  result = PAMI_Client_destroy(&client1);
  assert(result == PAMI_SUCCESS);

  result = PAMI_Client_destroy(&client2);
  assert(result == PAMI_SUCCESS);

  result = PAMI_Client_destroy(&client3);
  assert(result == PAMI_SUCCESS);

  printf("end of test \n");
  fflush(stdout);
  sleep(1);

  return 0;
}

