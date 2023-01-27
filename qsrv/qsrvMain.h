#ifndef PVXS_QSRVMAIN_H
#define PVXS_QSRVMAIN_H

<<<<<<< HEAD
#include "osiFileName.h"

=======
>>>>>>> 4d22d26 (Basic functionality without database, hooks, or pvxs)
#ifndef EPICS_BASE
#define EPICS_BASE ""
#error -DEPICS_BASE=<path-to-epics-base> required while building this component
#endif

<<<<<<< HEAD
#define IOC_SERVER_NAME "qsrv"

// The name of the database initialization file used for startup
#define DEFAULT_INITIALISATION_FILENAME IOC_SERVER_NAME ".dbd"
#define INITIALISATION_FILENAME "dbd" OSI_PATH_SEPARATOR DEFAULT_INITIALISATION_FILENAME
#define FULL_PATH_TO_INITIALISATION_FILE EPICS_BASE OSI_PATH_SEPARATOR INITIALISATION_FILENAME
#define RELATIVE_PATH_TO_INITIALISATION_FILE ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR INITIALISATION_FILENAME
// automatically generated from dbd file.  This must match the initialization filename you choose above.  xxxx_registerRecordDeviceDriver()
extern "C" int qsrv_registerRecordDeviceDriver(struct dbBase* pdbbase);

// The name of the database shutdown file used for exit
// Must match with `xxxx_DBD = xxxx.dbd` in your Makefile
#define SHUTDOWN_FILENAME "db" OSI_PATH_SEPARATOR IOC_SERVER_NAME "Exit.db"
#define FULL_PATH_TO_EXIT_FILE EPICS_BASE OSI_PATH_SEPARATOR SHUTDOWN_FILENAME
#define RELATIVE_PATH_TO_SHUTDOWN_FILE ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR SHUTDOWN_FILENAME
=======
// The name of the database initialization file used for startup
#define DEFAULT_INITIALISATION_FILENAME "pvxsIoc.dbd"
#define INITIALISATION_FILENAME "dbd" OSI_PATH_SEPARATOR DEFAULT_INITIALISATION_FILENAME
// The name of the database shutdown file used for exit
#define SHUTDOWN_FILENAME "db" OSI_PATH_SEPARATOR "qsrvExit.db"

#define RELATIVE_PATH_TO_INITIALISATION_FILE ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR INITIALISATION_FILENAME
#define RELATIVE_PATH_TO_SHUTDOWN_FILE ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR SHUTDOWN_FILENAME
#define FULL_PATH_TO_INITIALISATION_FILE EPICS_BASE OSI_PATH_SEPARATOR INITIALISATION_FILENAME
#define FULL_PATH_TO_EXIT_FILE EPICS_BASE OSI_PATH_SEPARATOR SHUTDOWN_FILENAME
>>>>>>> 4d22d26 (Basic functionality without database, hooks, or pvxs)

#endif //PVXS_QSRVMAIN_H
