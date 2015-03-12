PAMI is the successor to LAPI and DCMF that is being used on IBM Blue Gene/Q, IBM PERCS/POWER, and other IBM systems to implement MPI, Charm++, GASNet and other protocols.

This repo includes example code for those who want to learn how to use PAMI.

Argonne National Laboratory maintains [an email list](http://lists.alcf.anl.gov/mailman/listinfo/pami-discuss) for discussing PAMI.

The best documentation of PAMI is found in the pami.h header file that one must include to use it.  The location of this file will vary from system to system; on Blue Gene/Q, it should be located in `/bgsys/drivers/ppcfloor/comm/sys/include/`.

EPFL hosts PAMI Doxygen at `https://bgq1.epfl.ch/navigator/resources/doc/pami/index.html`.

[PAMI Programming Guide](http://publibfp.dhe.ibm.com/epubs/pdf/a2322733.pdf) was the first publicly available documentation of the PAMI API.  Please note that this document is about the AIX implementation of PAMI.  This document was originally full of errors and still may not be correct in all cases.  Please always verify syntax with <tt>pami.h</tt> on your system.
