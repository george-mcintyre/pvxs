TOP=..

# step 1.  Use -I... to test event-config.h
#          produce configure/O.$(T_A)/TOOLCHAIN
# step 2 in setup/Makefile
_PVXS_BOOTSTRAP = YES

include $(TOP)/configure/CONFIG

# use custom libevent2 install prefix by:
#   setting LIBEVENT only for single arch build
#   setting LIBEVENT_$(T_A) for each arch
#   leave unset to use implicit system search path
# NOTE: only needed if not present in default search paths
LIBEVENT ?= $(LIBEVENT_$(T_A))
LIBEVENT_$(T_A) ?= $(wildcard $(abspath $(TOP)/bundle/usr/$(T_A)))

INCLUDES += $(if $(LIBEVENT),-I$(LIBEVENT)/include)

# use libssl in non-default location.  (eg. OSX w/ brew)
OPENSSL ?= $(OPENSSL_$(T_A))
# Auto-detect OpenSSL in common Windows locations
OPENSSL_$(T_A) ?= $(firstword $(wildcard C:/Strawberry/c) $(wildcard C:/OpenSSL-Win64) $(wildcard C:/OpenSSL))

INCLUDES += $(if $(OPENSSL),-I$(OPENSSL)/include)

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

CFG += CONFIG_PVXS_VERSION

include $(TOP)/configure/RULES

ifdef T_A
install: TOOLCHAIN

TOOLCHAIN: toolchain.c
	$(CPP) $(CPPFLAGS) $(INCLUDES) ../toolchain.c > $@.tmp
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c ../probe-openssl.c -o probe-openssl.o > probe-openssl.out 2>&1 && $(ECHO) EVENT2_HAS_OPENSSL = YES >> $@.tmp || $(ECHO) No OpenSSL
	$(MV) $@.tmp $@

endif
