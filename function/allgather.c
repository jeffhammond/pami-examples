#include <pami.h>
#include "safemalloc.h"
#include "preamble.h"

int allgather(pami_geometry_t geometry, pami_context_t context, size_t count, void * sbuf, void * rbuf)
{
	pami_result_t rc = PAMI_ERROR;

	pami_xfer_type_t xfer = PAMI_XFER_ALLGATHER;

	size_t num_alg[2];
	/* query the geometry */
	rc = PAMI_Geometry_algorithms_num( geometry, xfer, num_alg );
	TEST_ASSERT(rc==PAMI_SUCCESS,"PAMI_Geometry_algorithms_num");

	pami_algorithm_t * safe_algs = (pami_algorithm_t *) safemalloc( num_alg[0] * sizeof(pami_algorithm_t) );
	pami_algorithm_t * fast_algs = (pami_algorithm_t *) safemalloc( num_alg[1] * sizeof(pami_algorithm_t) );
	pami_metadata_t  * safe_meta = (pami_metadata_t  *) safemalloc( num_alg[0] * sizeof(pami_metadata_t)  );
	pami_metadata_t  * fast_meta = (pami_metadata_t  *) safemalloc( num_alg[1] * sizeof(pami_metadata_t)  );

	rc = PAMI_Geometry_algorithms_query(geometry, xfer, safe_algs, safe_meta, num_alg[0], fast_algs, fast_meta, num_alg[1]);
	TEST_ASSERT(rc==PAMI_SUCCESS,"PAMI_Geometry_algorithms_query");

	size_t allgather_alg = 0; /* 0 is not necessarily the best one... */

	pami_xfer_t this;
	volatile int active = 1;

	this.cb_done                       = cb_done;
	this.cookie                        = (void*) &active;
	this.algorithm                     = safe_algs[allgather_alg]; /* safe algs should (must?) work */
	this.cmd.xfer_allgather.sndbuf     = sbuf;
	this.cmd.xfer_allgather.stype      = PAMI_TYPE_BYTE;
	this.cmd.xfer_allgather.stypecount = count;
	this.cmd.xfer_allgather.rcvbuf     = rbuf;
	this.cmd.xfer_allgather.rtype      = PAMI_TYPE_BYTE;
	this.cmd.xfer_allgather.rtypecount = count;

	/* perform a allgather */
	rc = PAMI_Collective( context, &this );
	TEST_ASSERT(rc==PAMI_SUCCESS,"PAMI_Collective - allgather");

	while (active)
	{
		rc = PAMI_Context_trylock_advancev( &context, 1, 1000 );
		TEST_ASSERT(rc==PAMI_SUCCESS,"PAMI_Context_trylock_advancev - allgather");
	}

	free(safe_algs);
	free(fast_algs);
	free(safe_meta);
	free(fast_meta);

	return PAMI_SUCCESS;
}
