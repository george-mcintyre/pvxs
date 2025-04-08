# Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

# Directories to build, any order
DIRS += configure

DIRS += setup
setup_DEPEND_DIRS = configure

DIRS += src
src_DEPEND_DIRS = setup

DIRS += tools
tools_DEPEND_DIRS = src

DIRS += ioc
ioc_DEPEND_DIRS = src

ifdef BASE_3_15
DIRS += qsrv
qsrv_DEPEND_DIRS = src ioc
endif

DIRS += test
test_DEPEND_DIRS = src ioc

DIRS += certs
certs_DEPEND_DIRS = src

DIRS += example
example_DEPEND_DIRS = src

USR_LDFLAGS += -L/usr/x86_64-w64-mingw32/lib64

include $(TOP)/configure/RULES_TOP
