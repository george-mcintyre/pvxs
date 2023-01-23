TOP=..

include $(TOP)/configure/CONFIG
# cfg/ sometimes isn't correctly included due to a Base bug
# so we do here (maybe again) as workaround
include $(TOP)/configure/CONFIG_PVXS_MODULE
include $(TOP)/configure/CONFIG_PVXS_VERSION
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# access to private headers
USR_CPPFLAGS += -I$(TOP)/src
USR_CPPFLAGS += -DPVXS_IOC_API_BUILDING

DBD += pvxsIoc.dbd

INC += pvxs/iochooks.h

LIBRARY += pvxsIoc
PROD_LIBS = Com

SHRLIB_VERSION = $(PVXS_MAJOR_VERSION).$(PVXS_MINOR_VERSION)

pvxsIoc_SRCS += iocserver.cpp
pvxsIoc_SRCS += dbentry.cpp
pvxsIoc_SRCS += dberrmsg.cpp
pvxsIoc_SRCS += dbeventcontextdeleter.cpp
pvxsIoc_SRCS += groupconfigprocessor.cpp
pvxsIoc_SRCS += groupprocessorcontext.cpp
pvxsIoc_SRCS += grouppv.cpp
pvxsIoc_SRCS += groupsource.cpp
pvxsIoc_SRCS += groupsourcehooks.cpp
pvxsIoc_SRCS += iochooks.cpp
pvxsIoc_SRCS += iocgroup.cpp
pvxsIoc_SRCS += iocgroupchannel.cpp
pvxsIoc_SRCS += iocgroupfield.cpp
pvxsIoc_SRCS += iocgroupfieldname.cpp
pvxsIoc_SRCS += iocgroupfieldnamecomponent.cpp
pvxsIoc_SRCS += singlesource.cpp
pvxsIoc_SRCS += singlesourcehooks.cpp
pvxsIoc_SRCS += typeutils.cpp
pvxsIoc_SRCS += yajlcallbackhandler.cpp

LIB_LIBS += pvxs
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)
#===========================

include $(TOP)/configure/RULES
include $(TOP)/configure/RULES_PVXS_MODULE
#----------------------------------------
#  ADD RULES AFTER THIS LINE

