TOP=..

include $(TOP)/configure/CONFIG
# cfg/ sometimes isn't correctly included due to a Base bug
# so we do here (maybe again) as workaround
-include $(wildcard $(TOP)/cfg/CONFIG*)
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# Strip whitespace from EVENT2_HAS_OPENSSL if it was set by TOOLCHAIN
# (CONFIG_PVXS_MODULE should also do this, but we need it here before the condition)
ifdef EVENT2_HAS_OPENSSL
EVENT2_HAS_OPENSSL := $(strip $(EVENT2_HAS_OPENSSL))
endif

# access to private headers
USR_CPPFLAGS += -I$(TOP)/src

ifeq ($(EVENT2_HAS_OPENSSL),YES)
SRC_DIRS += $(TOP)/certs
SRC_DIRS += $(TOP)/src
USR_CPPFLAGS += -DPVXS_ENABLE_OPENSSL
USR_CPPFLAGS += -I$(TOP)/certs -I$(TOP)/bundle/CLI11/include

# pvxcert directly uses OpenSSL functions, so it needs ssl and crypto libraries
# On Windows MSVC, we can only build it if MSVC-compatible .lib files are available
# Check if we can build pvxcert (use same logic as setup/Makefile)
PVXCERT_CAN_BUILD = YES
ifeq (WIN32,$(OS_CLASS))
ifndef GNUISH
# MSVC build - check if OpenSSL has .lib files (MSVC-compatible)
# Use OPENSSL variable (same as setup/Makefile)
OPENSSL ?= $(OPENSSL_$(T_A))
OPENSSL_$(T_A) ?= $(firstword $(wildcard C:/Strawberry/c) $(wildcard C:/OpenSSL-Win64) $(wildcard C:/OpenSSL))
ifdef OPENSSL
OPENSSL_SSL_LIB = $(wildcard $(OPENSSL)/lib/ssl.lib)
OPENSSL_CRYPTO_LIB = $(wildcard $(OPENSSL)/lib/crypto.lib)
OPENSSL_SSL_A = $(wildcard $(OPENSSL)/lib/libssl.a)
OPENSSL_CRYPTO_A = $(wildcard $(OPENSSL)/lib/libcrypto.a)
# If only .a files exist but no .lib files, skip building pvxcert for MSVC
ifeq (,$(OPENSSL_SSL_LIB)$(OPENSSL_CRYPTO_LIB))
ifneq (,$(OPENSSL_SSL_A)$(OPENSSL_CRYPTO_A))
# Only MinGW .a files available, MSVC can't link them - skip pvxcert
PVXCERT_CAN_BUILD = NO
endif
endif
endif
endif
endif

ifeq ($(PVXCERT_CAN_BUILD),YES)
PROD += pvxcert
pvxcert_SRCS += cert.cpp
pvxcert_SRCS += p12filefactory.cpp
pvxcert_SRCS += certfilefactory.cpp
pvxcert_SRCS += certfactory.cpp
pvxcert_SRCS += certstatusmanager.cpp
pvxcert_CPPFLAGS += -DPVXS_API_BUILDING
pvxcert_SYS_LIBS += $(LIBEVENT_SYS_LIBS)
# Add OpenSSL libraries (they're available in the right format)
pvxcert_SYS_LIBS += ssl crypto
endif

endif # EVENT2_HAS_OPENSSL

PROD_LIBS += pvxs Com

PROD += pvxvct
pvxvct_SRCS += pvxvct.cpp

PROD += pvxinfo
pvxinfo_SRCS += info.cpp

PROD += pvxget
pvxget_SRCS += get.cpp

PROD += pvxmonitor
pvxmonitor_SRCS += monitor.cpp

PROD += pvxput
pvxput_SRCS += put.cpp

PROD += pvxcall
pvxcall_SRCS += call.cpp

PROD += pvxlist
pvxlist_SRCS += list.cpp

PROD += pvxmshim
pvxmshim_SRCS += mshim.cpp

#===========================

include $(TOP)/configure/RULES
-include $(wildcard $(TOP)/cfg/RULES*)
#----------------------------------------
#  ADD RULES AFTER THIS LINE

