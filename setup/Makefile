TOP=..

# step 1 in configure/Makefile
# step 2. generate cfg/TOOLCHAIN_PVXS.$(T_A)
#         install cfg/*
# remaining TOP directories will include generated files
_PVXS_BOOTSTRAP = YES

include $(TOP)/configure/CONFIG

# Include TOOLCHAIN to get EVENT2_HAS_OPENSSL even in bootstrap mode
ifdef T_A
-include $(TOP)/configure/O.$(T_A)/TOOLCHAIN
# Strip any whitespace from EVENT2_HAS_OPENSSL if it was set by TOOLCHAIN
ifdef EVENT2_HAS_OPENSSL
EVENT2_HAS_OPENSSL := $(strip $(EVENT2_HAS_OPENSSL))
endif
endif

LIBEVENT ?= $(LIBEVENT_$(T_A))
LIBEVENT_$(T_A) ?= $(wildcard $(abspath $(TOP)/bundle/usr/$(T_A)))

# use libssl in non-default location.  (eg. OSX w/ brew)
OPENSSL ?= $(OPENSSL_$(T_A))
# Auto-detect OpenSSL in common Windows locations
OPENSSL_$(T_A) ?= $(firstword $(wildcard C:/Strawberry/c) $(wildcard C:/OpenSSL-Win64) $(wildcard C:/OpenSSL))

_LIBEVENT_BUNDLE_LIBS_YES = event_openssl
_LIBEVENT_SYS_LIBS_YES += ssl crypto

# For Windows MSVC builds, check if OpenSSL libraries are MinGW format (.a files)
# MSVC linker cannot link MinGW .a files, only .lib files
ifeq (WIN32,$(OS_CLASS))
ifndef GNUISH
# Not using GCC/MinGW, so likely MSVC
# Check if OpenSSL has .lib files (MSVC-compatible) or only .a files (MinGW)
ifdef OPENSSL
OPENSSL_LIB_SSL_LIB = $(wildcard $(OPENSSL)/lib/ssl.lib)
OPENSSL_LIB_CRYPTO_LIB = $(wildcard $(OPENSSL)/lib/crypto.lib)
OPENSSL_LIB_SSL_A = $(wildcard $(OPENSSL)/lib/libssl.a)
OPENSSL_LIB_CRYPTO_A = $(wildcard $(OPENSSL)/lib/libcrypto.a)
# If only .a files exist but no .lib files, skip OpenSSL libraries for MSVC
ifeq (,$(OPENSSL_LIB_SSL_LIB)$(OPENSSL_LIB_CRYPTO_LIB))
ifneq (,$(OPENSSL_LIB_SSL_A)$(OPENSSL_LIB_CRYPTO_A))
# Only MinGW .a files available, MSVC can't link them
_LIBEVENT_SYS_LIBS_YES := $(filter-out ssl crypto,$(_LIBEVENT_SYS_LIBS_YES))
endif
endif
endif
endif
endif

_LIBEVENT_BUNDLE_LIBS += $(_LIBEVENT_BUNDLE_LIBS_$(EVENT2_HAS_OPENSSL))
_LIBEVENT_BUNDLE_LIBS += event_core

_LIBEVENT_SYS_LIBS += $(_LIBEVENT_SYS_LIBS_$(EVENT2_HAS_OPENSSL))

ifeq (WIN32,$(OS_CLASS))
_LIBEVENT_SYS_LIBS += bcrypt iphlpapi netapi32 ws2_32
else
_LIBEVENT_BUNDLE_LIBS += event_pthreads
endif

# at this point we have included the generated O.$(T_A)/TOOLCHAIN
# and use this to generated CONFIG_PVXS_MODULE

CFG += CONFIG_PVXS_MODULE
CFG += RULES_PVXS_MODULE

ifdef T_A
CFG += TOOLCHAIN_PVXS.$(T_A)
endif

include $(TOP)/configure/RULES

ifdef T_A

EXPAND_ARGS = -a $(T_A) -t "$(INSTALL_LOCATION)"
EXPAND_ARGS += "-DOPENSSL=$(OPENSSL)"
EXPAND_ARGS += "-DLIBEVENT=$(LIBEVENT)"
EXPAND_ARGS += "-DLIBEVENT_BUNDLE_LIBS=$(_LIBEVENT_BUNDLE_LIBS)"
EXPAND_ARGS += "-DLIBEVENT_SYS_LIBS=$(_LIBEVENT_SYS_LIBS)"
EXPAND_ARGS += "-DEVENT2_HAS_OPENSSL=$(EVENT2_HAS_OPENSSL)"

TOOLCHAIN_PVXS.$(T_A): ../TOOLCHAIN_PVXS.target@
	$(EXPAND_TOOL) $(EXPAND_ARGS) $< $@

endif
