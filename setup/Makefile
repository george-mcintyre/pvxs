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
# Check for both naming conventions: ssl.lib/crypto.lib and libssl.lib/libcrypto.lib
OPENSSL_BUILD_SSL_LIB = $(firstword $(wildcard $(OPENSSL_BUILD_PREFIX)/lib/ssl.lib) $(wildcard $(OPENSSL_BUILD_PREFIX)/lib/libssl.lib))
OPENSSL_BUILD_CRYPTO_LIB = $(firstword $(wildcard $(OPENSSL_BUILD_PREFIX)/lib/crypto.lib) $(wildcard $(OPENSSL_BUILD_PREFIX)/lib/libcrypto.lib))

# Check if we have MSVC-compatible OpenSSL at the detected location
ifdef OPENSSL
# Check for both naming conventions
OPENSSL_LIB_SSL_LIB = $(firstword $(wildcard $(OPENSSL)/lib/ssl.lib) $(wildcard $(OPENSSL)/lib/libssl.lib))
OPENSSL_LIB_CRYPTO_LIB = $(firstword $(wildcard $(OPENSSL)/lib/crypto.lib) $(wildcard $(OPENSSL)/lib/libcrypto.lib))
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

# Determine the actual library names to use for linking
# OpenSSL may install as libssl.lib/libcrypto.lib (with lib prefix) or ssl.lib/crypto.lib
# MSVC linker can handle both, but we need to use the correct name
# Check both the built OpenSSL location and the detected OPENSSL location
_OPENSSL_CHECK_PREFIX = $(if $(OPENSSL_BUILD_SSL_LIB),$(OPENSSL_BUILD_PREFIX),$(if $(OPENSSL),$(OPENSSL),$(OPENSSL_BUILD_PREFIX)))
_OPENSSL_SSL_LIB_FOUND = $(firstword $(wildcard $(_OPENSSL_CHECK_PREFIX)/lib/ssl.lib) $(wildcard $(_OPENSSL_CHECK_PREFIX)/lib/libssl.lib))
_OPENSSL_CRYPTO_LIB_FOUND = $(firstword $(wildcard $(_OPENSSL_CHECK_PREFIX)/lib/crypto.lib) $(wildcard $(_OPENSSL_CHECK_PREFIX)/lib/libcrypto.lib))

ifdef _OPENSSL_SSL_LIB_FOUND
OPENSSL_SSL_LIB_NAME = $(notdir $(_OPENSSL_SSL_LIB_FOUND))
OPENSSL_CRYPTO_LIB_NAME = $(notdir $(_OPENSSL_CRYPTO_LIB_FOUND))
# Extract library name without .lib extension for linker
# If it's libssl.lib, linker name is libssl; if it's ssl.lib, linker name is ssl
OPENSSL_SSL_LIB_LINK = $(patsubst %.lib,%,$(OPENSSL_SSL_LIB_NAME))
OPENSSL_CRYPTO_LIB_LINK = $(patsubst %.lib,%,$(OPENSSL_CRYPTO_LIB_NAME))
# Override library names if they differ from default
ifneq ($(OPENSSL_SSL_LIB_LINK),ssl)
_LIBEVENT_SYS_LIBS_YES := $(filter-out ssl crypto,$(_LIBEVENT_SYS_LIBS_YES)) $(OPENSSL_SSL_LIB_LINK) $(OPENSSL_CRYPTO_LIB_LINK)
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

# Build OpenSSL for MSVC if needed
ifeq ($(_NEED_OPENSSL_BUILD),YES)
# On Windows, use ensure-python.bat which will install Python if needed
# On other platforms, find Python using standard methods
ifeq (WIN32,$(OS_CLASS))
# Use ensure-python.bat which finds or installs Python
# Batch script outputs Python path to stdout, errors to stderr
# Use abspath to convert relative path to absolute path for Windows
PYTHON_SCRIPT := $(abspath $(TOP)/.ci-local/ensure-python.bat)
PYTHON := $(shell "$(PYTHON_SCRIPT)")
PYTHON := $(strip $(PYTHON))
ifeq ($(PYTHON),)
$(error Python not found and installation failed. Check the error messages above for details. Please install Python manually from https://www.python.org/downloads/ and ensure it is in PATH)
endif
else
PYTHON := $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null || which python3 2>/dev/null || which python 2>/dev/null || echo python)
PYTHON := $(strip $(PYTHON))
endif

.PHONY: build-openssl-msvc
build-openssl-msvc:
	@echo "Building OpenSSL for MSVC..."
	@echo "Using Python: $(PYTHON)"
	@"$(PYTHON)" "$(abspath $(TOP)/.ci-local/openssl.py)" || (echo "Error: Failed to build OpenSSL for MSVC" && exit 1)

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
