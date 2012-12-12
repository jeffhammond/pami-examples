ifndef COMPILER
  COMPILER = GNU
endif

ifeq	($(COMPILER),GNU)
  CC      = mpicc
  COPT    = -g -Wall -std=gnu99 -O2 -mcpu=a2 -mtune=a2
endif

ifeq	($(COMPILER),XL)
  CC      = mpixlc_r
  COPT    = -g -qstrict --qlanglvl=extc99 -O3 -qarch=qp -qtune=qp
endif

LD      = $(CC)
CFLAGS  = $(COPT) -I.
LDFLAGS = $(COPT) -lpthread

all: hello.x query.x clients.x

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.x: %.o
	$(LD) $(LDFLAGS) $< -o $@

clean:
	$(RM) $(RMFLAGS) *.o
	$(RM) $(RMFLAGS) *.lst

realclean: clean
	$(RM) $(RMFLAGS) *.x core.*

