ifndef COMPILER
  COMPILER = GNU
endif

ifeq ($(COMPILER),GNU)
  CC      = mpicc
  COPT    = -g -Wall -std=gnu99 -O2 -mcpu=a2 -mtune=a2
endif

ifeq ($(COMPILER),XL)
  CC      = mpixlc_r 
  COPT    = -g -qlanglvl=extc99 -O3 -qarch=qp -qtune=qp
endif

LD      = $(CC)
CFLAGS  = $(COPT) -I. -I../include
LDFLAGS = $(COPT) -lpthread

CFLAGS += -DPROGRESS_THREAD

obj = safemalloc.o barrier.o allgather.o

all: p2p coll
p2p: send.x put.x rput.x get.x rget.x rmw.x
coll: test-barrier.x test-bcast.x test-allreduce.x test-allgather.x

%.o: %.c safemalloc.h coll.h preamble.h
	$(CC) $(CFLAGS) -c $< -o $@

%.x: %.o $(obj)
	$(LD) $(LDFLAGS) $< $(obj) -o $@

clean:
	$(RM) $(RMFLAGS) *.o
	$(RM) $(RMFLAGS) *.lst

realclean: clean
	$(RM) $(RMFLAGS) *.x
	$(RM) $(RMFLAGS) core.*

