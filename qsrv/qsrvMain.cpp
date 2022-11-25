/**
 * qsrvMain.cpp: The main entry point for the pvxs qsrv soft IOC.
 * Use this as is, or as the base for your customised IOC application
 */
#include <iostream>
#include "epicsExit.h"
#include "epicsThread.h"
#include "iocsh.h"

#include <epicsGetopt.h>
#include "osiFileName.h"

#include "qsrvMain.h"

namespace pvxs {
namespace qsrv {
// Verbose flag - if true then show verbose output
bool verboseFlag = false;
// Macro to use to show verbose output
// e.g. VERBOSE_MESSAGE "some verbose message: " << variable << " more verbosity\n";
#define VERBOSE_MESSAGE if (verboseFlag) std::cout <<

// DB Loaded flag - true if the database has already been loaded
bool isDbLoaded = false;

/**
 * Print out usage information for the QSRV application
 *
 * @param executableName 	 the name of the executable as passed to main
 * @param initialisationFile the name of the database initialization file. Either the one
 * 							 specified on the commandline or the default one
 */
void usage(std::string& executableName, const std::string& initialisationFile) {
	std::cout << "PVXS configurable IOC server\n\n"
	             "Usage: " << executableName <<
	          " [-h] [-S] [-v] \n"
	          " [-m <macro-name>=<macro-value>[,<macro-name>=<macro-value>]...] ... \n"
	          " [-D <path>] [-G <path>] [-a <path>] [-d <path>] \n"
	          " [-x <prefix>] [<script-path>]\n"
	          "\nDescription:\n"
	          "  After configuring the IOC server with " << initialisationFile.c_str()
	          << " (or overriden with the -D option)"
	             " this command starts an interactive IOC shell, unless the -S flag is specified.  Group"
	             " configuration can optionally be specified using the -G flag, and security can be configured using the -a flag.  An"
	             " initial database of PV records can be established using the -d flag.  Finally some startup commands can be run if an optional script-path"
	             " is specified."
	             "\n"
	             "\nCommon flags:\n"
	             "  -h                     Print this message and exit.\n"
	             "  -S                     Prevents an interactive shell being started. \n"
	             "  -v                     Verbose, display steps taken during startup.\n"
	             "  -D <path>.             This overrides the default database configuration file.\n"
	             "                         If specified, it must come before any (-G), (-a), or (-d) flags. Specify \n"
	             "                         the path to the configuration file as well as the\n"
	             "                         extension.  By convention, this file has a .dbd \n"
	             "                         extension.  The compile-time default configuration file\n"
	             "                         is " FULL_PATH_TO_INITIALISATION_FILE ".\n"
	             "  -G <path>.             This is the database group definition file used to \n"
	             "                         define database Groups.  The file must be provided in JSON format.  \n"
	             "                         By convention, the extension is .json\n"
	             "  -m <name>=<value>[,<name>=<value>]... \n"
	             "                         Set/replace macro definitions. Macros are used in the \n"
	             "                         iocsh as well as when parsing the access security configuration \n"
	             "                         (-a) and database records file (-d).  You can provide a (-m) flag \n"
	             "                         multiple times on the same command line.  Each occurrence applies to any \n"
	             "                         (-a) and (-d) options that follow it until the next (-m) overrides.  The last\n"
	             "                         macros defined are used in the iocsh\n"
	             "  -a <path>              Access security configuration file.  Use this flag \n"
	             "                         to configure access security.  The security definitions specified \n"
	             "                         are subject to macro substitution.\n"
	             "  -d <path>              Load records from file.  The definitions specified in the given file \n"
	             "                         are subject to macro substitution\n"
	             "  -x <prefix>            Specifies the prefix for the database exit record Load.  It is used as a substitution for\n"
	             "                         the $(IOC) macro in " FULL_PATH_TO_EXIT_FILE ". \n"
	             "                         This file is used to configure database shutdown.\n"
	             "  script-path            A command script is optional.  The iocsh commands will be run *after*\n"
	             "                         the database is initialised with any (-d) or (-x) flags.  If you want to \n"
	             "                         run the script before initialization then don't specify (-d) or (-x) flags and \n"
	             "                         perform all database loading in the script itself, or in the interactive shell.\n"
	             "\n"
	             "Examples:\n"
	             "  " << executableName
	          << " -d my.db                use default configuration and load database records \n"
	             "                         from my.db and start an iteractive IOC shell \n"
	             "  " << executableName
	          << " -m NAME=PV -d my.db     use default configuration and load database records \n"
	             "                         from my.db after setting macro NAME to PV\n"
	             "  " << executableName
	          << " -D myconfig.dbd -G my-group-config.json -d my.db  \n"
	             "                         use custom configuration \n"
	             "                         myconfig.dbd and group configuration in\n"
	             "                         my-group-config.json to configure the IOC then load database records \n"
	             "                         from my.db and start an interactive shell \n";

}
}
} // pvxs::qsrv

using namespace pvxs::qsrv;

/**
 * Main entry point for the qsrv executable.
 *
 * @param argc the number of command line arguments
 * @param argv the command line arguments
 * @return 0 for successful exit, nonzero otherwise
 */
int main(int argc, char* argv[]) {
	std::string executableName(argv[0]);
	std::string databaseInitialisationFile(FULL_PATH_TO_INITIALISATION_FILE);
	std::string databaseShutdownFile(FULL_PATH_TO_EXIT_FILE);
	std::string macros;       // scratch space for macros
	std::string xmacro;
	bool interactive = true;
	bool loadedDb = false;
	bool ranScript = false;

	int opt;

	while ((opt = getopt(argc, argv, "ha:D:d:m:Sx:v")) != -1) {
		switch (opt) {
		case 'h':
			usage(executableName, databaseInitialisationFile);
			epicsExit(0);
			return 0;
		case 'D':
			if (isDbLoaded) {
				throw std::runtime_error("database configuration file override specified "
				                         "after " FULL_PATH_TO_INITIALISATION_FILE " has already been loaded.\n"
				                         "Add the -D option prior to any -d or -x options and try again");
			}
			databaseInitialisationFile = optarg;
			break;
		case 'm':
			macros = optarg;
			break;
		case 'S':
			interactive = false;
			break;
		case 'v':
			verboseFlag = true;
			break;
		default:
			usage(executableName, databaseInitialisationFile);
			std::cerr << "Unknown argument: -" << char(optopt) << "\n";
			epicsExit(2);
			return 2;
		}
	}

	// If interactive then enter shell
	if (interactive) {
		std::cout.flush();
		std::cerr.flush();
		if (iocsh(nullptr)) {
			// if error status then propagate error to epics and shell
			epicsExit(1);
			return 1;
		}
	} else {
		// If non-interactive then exit
		if (loadedDb || ranScript) {
			epicsThreadExitMain();
		} else {
			std::cerr << "Nothing to do!\n";
			epicsExit(1);
			return 1;
		}
	}

	epicsExit(0);
	return (0);
}
