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

# For Windows MSVC builds, check if OpenSSL libraries are MSVC-compatible
# If only MinGW .a files are available, or OpenSSL is not found, build OpenSSL for MSVC
ifeq (WIN32,$(OS_CLASS))
ifndef GNUISH
# Not using GCC/MinGW, so likely MSVC
OPENSSL_BUILD_PREFIX = C:/OpenSSL-Win64
OPENSSL_BUILD_SSL_LIB = $(wildcard $(OPENSSL_BUILD_PREFIX)/lib/ssl.lib)
OPENSSL_BUILD_CRYPTO_LIB = $(wildcard $(OPENSSL_BUILD_PREFIX)/lib/crypto.lib)

# Check if we have MSVC-compatible OpenSSL at the detected location
ifdef OPENSSL
OPENSSL_LIB_SSL_LIB = $(wildcard $(OPENSSL)/lib/ssl.lib)
OPENSSL_LIB_CRYPTO_LIB = $(wildcard $(OPENSSL)/lib/crypto.lib)
OPENSSL_LIB_SSL_A = $(wildcard $(OPENSSL)/lib/libssl.a)
OPENSSL_LIB_CRYPTO_A = $(wildcard $(OPENSSL)/lib/libcrypto.a)
# If only .a files exist but no .lib files, we need to build OpenSSL for MSVC
ifeq (,$(OPENSSL_LIB_SSL_LIB)$(OPENSSL_LIB_CRYPTO_LIB))
ifneq (,$(OPENSSL_LIB_SSL_A)$(OPENSSL_LIB_CRYPTO_A))
# Only MinGW .a files available, MSVC can't link them
# Build OpenSSL for MSVC instead of giving up
ifeq (,$(OPENSSL_BUILD_SSL_LIB)$(OPENSSL_BUILD_CRYPTO_LIB))
# OpenSSL for MSVC not found, need to build it
_NEED_OPENSSL_BUILD = YES
endif
# Use the built OpenSSL location
OPENSSL_$(T_A) = $(OPENSSL_BUILD_PREFIX)
OPENSSL = $(OPENSSL_BUILD_PREFIX)
endif
endif
else
# OPENSSL not defined, check if we have it at the default build location
ifeq (,$(OPENSSL_BUILD_SSL_LIB)$(OPENSSL_BUILD_CRYPTO_LIB))
# OpenSSL for MSVC not found, need to build it
_NEED_OPENSSL_BUILD = YES
endif
# Use the built OpenSSL location
OPENSSL_$(T_A) = $(OPENSSL_BUILD_PREFIX)
OPENSSL = $(OPENSSL_BUILD_PREFIX)
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

# Build OpenSSL for MSVC if needed
ifeq ($(_NEED_OPENSSL_BUILD),YES)
.PHONY: build-openssl-msvc
build-openssl-msvc:
	@echo "Building OpenSSL for MSVC..."
	@python $(TOP)/.ci-local/openssl.py || (echo "Error: Failed to build OpenSSL for MSVC" && exit 1)

# Make sure OpenSSL is built before generating TOOLCHAIN
TOOLCHAIN_PVXS.$(T_A): build-openssl-msvc
endif

EXPAND_ARGS = -a $(T_A) -t "$(INSTALL_LOCATION)"
EXPAND_ARGS += "-DOPENSSL=$(OPENSSL)"
EXPAND_ARGS += "-DLIBEVENT=$(LIBEVENT)"
EXPAND_ARGS += "-DLIBEVENT_BUNDLE_LIBS=$(_LIBEVENT_BUNDLE_LIBS)"
EXPAND_ARGS += "-DLIBEVENT_SYS_LIBS=$(_LIBEVENT_SYS_LIBS)"
EXPAND_ARGS += "-DEVENT2_HAS_OPENSSL=$(EVENT2_HAS_OPENSSL)"

TOOLCHAIN_PVXS.$(T_A): ../TOOLCHAIN_PVXS.target@
	$(EXPAND_TOOL) $(EXPAND_ARGS) $< $@

endif
