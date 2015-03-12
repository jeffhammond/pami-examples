# Introduction #

PAMI supports multiple clients, e.g., MPI and ARMCI, MPI and UPC, MPI and CAF, etc.  Using a PAMI clients (e.g. MPI) with an SPI client (e.g. the lattice QCD codes) requires different resource control.

# Warning #

The client name 'MPI' is reserved by MPI and should not be used by any client that intends to inter-operate with the provided MPI library.

# Example Code #

This test demonstrates how to use multiple clients:

```
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

  /* initialize the client1 */
  char * client1name = "CLIENT1";
  pami_client_t client1;
  result = PAMI_Client_create(client1name, &client1, NULL, 0);
  assert(result == PAMI_SUCCESS);

  char * client2name = "CLIENT2";
  pami_client_t client2;
  result = PAMI_Client_create(client2name, &client2, NULL, 0);
  assert(result == PAMI_SUCCESS);

  /* finalize the client1 */
  result = PAMI_Client_destroy(&client2);
  assert(result == PAMI_SUCCESS);

  result = PAMI_Client_destroy(&client1);
  assert(result == PAMI_SUCCESS);

  printf("end of test \n");
  fflush(stdout);
  sleep(1);

  return 0;
}
```

You can submit with `qsub -t 5 -n 1 --mode c1 --env PAMI_CLIENTS=CLIENT1,CLIENT2 ./clients.x`.