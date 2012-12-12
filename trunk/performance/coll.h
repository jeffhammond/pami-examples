#ifndef COLL_H
#define COLL_H

#include <pami.h>

int barrier(pami_geometry_t geometry, pami_context_t context);
int allgather(pami_geometry_t geometry, pami_context_t context, size_t count, void * sbuf, void * rbuf);

#endif
