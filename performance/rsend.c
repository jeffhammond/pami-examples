#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <mpi.h>
#include <hwi/include/bqc/A2_inlines.h>

#include "safemalloc.h"

int main(int argc, char* argv[])
{
  int requested = MPI_THREAD_FUNNELED;
  int provided  = MPI_THREAD_SINGLE;
  MPI_Init_thread(&argc, &argv, requested, &provided);
  if (provided<requested)
    exit(provided);

  int world_rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  if (world_rank==0)
  {
    printf("hello world from rank %d of %d \n", world_rank, world_size );
    fflush(stdout);
  }

  /************************************************************************/

  for (int n=1; n<=67108864; n*=2)
  {

    size_t bytes = n * sizeof(int);
    int *  rbuf = (int *) safemalloc(bytes);
    for (int i=0; i<n; i++)
      rbuf[i] = -1;

    int *  sbuf  = (int *) safemalloc(bytes);
    for (int i=0; i<n; i++)
      sbuf[i] = world_rank;

    int send_to   = (world_rank>0 ? world_rank-1 : world_size-1);
    int recv_from = (world_rank<(world_size-1) ? world_rank+1 : 0);

    MPI_Request req[2];

    MPI_Irecv( rbuf, n, MPI_INT, recv_from, 0, MPI_COMM_WORLD, &req[0] );

    MPI_Barrier(MPI_COMM_WORLD);

    uint64_t t0 = GetTimeBase();

    MPI_Irsend( sbuf, n, MPI_INT, send_to,   0, MPI_COMM_WORLD, &req[1] );
    MPI_Waitall( 2, req, MPI_STATUSES_IGNORE ); 

    uint64_t t1 = GetTimeBase();
    uint64_t dt = t1-t0;

    MPI_Barrier(MPI_COMM_WORLD);

    printf("%d: MPI_Irsend/Irecv/Waitall of %ld bytes achieves %lf MB/s \n", world_rank, bytes, 1.6e9*1e-6*(double)bytes/(double)dt );
    fflush(stdout);

    int errors = 0;
    
    for (int i=0; i<n; i++)
      if (rbuf[i] != recv_from)
         errors++;

    if (errors>0)
      for (int i=0; i<n; i++)
        if (rbuf[i] != recv_from)
          printf("%d: rbuf[%d] = %d (%d) \n", world_rank, i, rbuf[i], recv_from);
    else
      printf("%d: no errors :-) \n", world_rank); 

    fflush(stdout);

    if (errors>0)
      exit(13);

    MPI_Barrier(MPI_COMM_WORLD);

    free(sbuf);
    free(rbuf);
  }

  /************************************************************************/

  MPI_Finalize();

  if (world_rank==0)
    printf("%d: end of test \n", world_rank );
  fflush(stdout);

  return 0;
}

