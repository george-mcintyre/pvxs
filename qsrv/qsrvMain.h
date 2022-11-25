/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_QSRVMAIN_H
#define PVXS_QSRVMAIN_H

#ifndef EPICS_BASE
#define EPICS_BASE ""
#error -DEPICS_BASE=<path-to-epics-base> required while building this component
#endif

// The name of the database initialization file used for startup
#define DEFAULT_INITIALISATION_FILENAME "pvxsIoc.dbd"
#define INITIALISATION_FILENAME "dbd" OSI_PATH_SEPARATOR DEFAULT_INITIALISATION_FILENAME
// The name of the database shutdown file used for exit
#define SHUTDOWN_FILENAME "db" OSI_PATH_SEPARATOR "qsrvExit.db"

#define RELATIVE_PATH_TO_INITIALISATION_FILE ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR INITIALISATION_FILENAME
#define RELATIVE_PATH_TO_SHUTDOWN_FILE ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR SHUTDOWN_FILENAME
#define FULL_PATH_TO_INITIALISATION_FILE EPICS_BASE OSI_PATH_SEPARATOR INITIALISATION_FILENAME
#define FULL_PATH_TO_EXIT_FILE EPICS_BASE OSI_PATH_SEPARATOR SHUTDOWN_FILENAME

#endif //PVXS_QSRVMAIN_H
