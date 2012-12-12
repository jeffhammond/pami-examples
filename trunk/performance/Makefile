ifndef COMPILER
  COMPILER = GNU
endif

ifeq ($(COMPILER),GNU)
  CC      = mpicc
  COPT    = -g -Wall -std=gnu99 -O2 -mcpu=a2 -mtune=a2
endif

ifeq ($(COMPILER),XL)
  CC      = mpixlc_r
  COPT    = -g -qstrict --qlanglvl=extc99 -O3 -qarch=qp -qtune=qp
endif

LD      = $(CC)
CFLAGS  = $(COPT) -I.
LDFLAGS = $(COPT) -lpthread

CFLAGS += -DPROGRESS_THREAD

obj = safemalloc.o barrier.o allgather.o

all: pami mpi

pami: put.x rput.x get.x rget.x send.x acc1.x rmw.x
mpi: rsend.x

%.o: %.c safemalloc.h coll.h
	$(CC) $(CFLAGS) -c $< -o $@

%.x: %.o $(obj)
	$(LD) $(LDFLAGS) $< $(obj) -o $@

clean:
	$(RM) $(RMFLAGS) *.o
	$(RM) $(RMFLAGS) *.lst

realclean: clean
	$(RM) $(RMFLAGS) *.x
	$(RM) $(RMFLAGS) core.*

