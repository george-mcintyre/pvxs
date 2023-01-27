TOP=..

include $(TOP)/configure/CONFIG
# cfg/ sometimes isn't correctly included due to a Base bug
# so we do here (maybe again) as workaround
include $(TOP)/configure/CONFIG_PVXS_MODULE
include $(TOP)/configure/CONFIG_PVXS_VERSION
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_IOC = qsrv

<<<<<<< HEAD
DBDDEPENDS_FILES += qsrv.dbd$(DEP)
qsrv_DBD = base.dbd pvxsIoc.dbd
DBD_INSTALLS = $(COMMON_DIR)/qsrv.dbd

qsrv_DB = qsrvExit.db

=======
>>>>>>> 4d22d26 (Basic functionality without database, hooks, or pvxs)
FINAL_LOCATION ?= $(shell $(PERL) $(TOOLS)/fullPathName.pl $(INSTALL_LOCATION))

# EPICS_BASE macro for our location
USR_CPPFLAGS += -DEPICS_BASE="\"$(FINAL_LOCATION)\""
<<<<<<< HEAD

qsrv_SRCS_DEFAULT += qsrvMain.cpp
qsrv_SRCS += qsrv_registerRecordDeviceDriver.cpp
qsrv_SRCS_vxWorks += -nil-

# Finally link IOC to the EPICS Base libraries
qsrv_LIBS += pvxsIoc
qsrv_LIBS += pvxs
qsrv_LIBS += $(EPICS_BASE_IOC_LIBS)

=======
#USR_CXXFLAGS += -DEPICS_BASE="\"$(FINAL_LOCATION)\""

qsrv_SRCS_DEFAULT += qsrvMain.cpp
qsrv_SRCS_vxWorks += -nil-

# Finally link IOC to the EPICS Base libraries
qsrv_LIBS += $(EPICS_BASE_IOC_LIBS)

DB += qsrvExit.db

>>>>>>> 4d22d26 (Basic functionality without database, hooks, or pvxs)
#===========================

include $(TOP)/configure/RULES
include $(TOP)/configure/RULES_PVXS_MODULE
#----------------------------------------
#  ADD RULES AFTER THIS LINE

