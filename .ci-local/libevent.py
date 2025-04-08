#!/usr/bin/env python

from __future__ import print_function

import sys
import os
import subprocess as SP

def inspectpath():
    for dir in os.environ['PATH'].split(os.pathsep):
        for tool in ('make', 'sh', 'gcc', 'cl'):
            for ext in ('', '.exe'):
                candidate = os.path.join(dir, tool+ext)
                if os.path.isfile(candidate):
                    print('Found', tool, candidate)
inspectpath()

def logcall(fn):
    def logit(*args, **kws):
        print('CALL', fn, args, kws)
        sys.stdout.flush()
        sys.stderr.flush()
        ret = fn(*args, **kws)
        sys.stdout.flush()
        sys.stderr.flush()
        return ret
    return logit

check_call = logcall(SP.check_call)
check_output = logcall(SP.check_output)

env=os.environ.copy()
PATH=env['PATH'].split(os.pathsep)

# CMake MinGW generator doesn't like sh.exe in PATH
# NMake generator doesn't care
# strip it out
PATH = [ent for ent in PATH if not os.path.isfile(os.path.join(ent, 'sh.exe'))]
env['PATH'] = os.pathsep.join(PATH)

print('ENV')
[print("  ", frag) for frag in env['PATH'].split(os.pathsep)]
print('PATH')
[print("  ", K, "=", V) for K, V in env.items()]

# update to specific libevent version
libevent_tag = os.environ.get('LIBEVENT_TAG', '')
if len(libevent_tag):
    if libevent_tag.startswith('origin/'):
        libevent_tag = libevent_tag[7:]

    check_call('git fetch --unshallow --tags origin',
               shell=True, cwd='bundle/libevent')
    check_call('git log -n1 '+libevent_tag+' --',
               shell=True, cwd='bundle/libevent')
    check_call('git reset --hard '+libevent_tag+' --',
               shell=True, cwd='bundle/libevent')

# Diagnostic: Print OpenSSL version to verify it's available
print("=== CHECKING OPENSSL INSTALLATION ===")
check_call('openssl version -a', shell=True)
print("=== END OPENSSL CHECK ===")

# Diagnostic: Check MinGW OpenSSL installation
print("=== CHECKING MINGW OPENSSL INSTALLATION ===")
mingw_ssl_dir = '/usr/x86_64-w64-mingw32'
print(f"Checking if MinGW OpenSSL directory exists: {mingw_ssl_dir}")
if os.path.exists(mingw_ssl_dir):
    print(f"MinGW base directory exists: {mingw_ssl_dir}")
    try:
        check_call(f'ls -la {mingw_ssl_dir}', shell=True)
        
        # Check for OpenSSL headers
        mingw_include = f'{mingw_ssl_dir}/include/openssl'
        if os.path.exists(mingw_include):
            print(f"MinGW OpenSSL headers directory exists: {mingw_include}")
            check_call(f'ls -la {mingw_include} | head -5', shell=True)
        else:
            print(f"ERROR: MinGW OpenSSL headers directory NOT found: {mingw_include}")
        
        # Check for OpenSSL libraries
        mingw_lib = f'{mingw_ssl_dir}/lib'
        if os.path.exists(mingw_lib):
            print(f"MinGW library directory exists: {mingw_lib}")
            check_call(f'ls -la {mingw_lib}/libssl* {mingw_lib}/libcrypto*', shell=True)
        else:
            print(f"ERROR: MinGW library directory NOT found: {mingw_lib}")
    except Exception as e:
        print(f"Error checking MinGW OpenSSL: {e}")
else:
    print(f"ERROR: MinGW directory does NOT exist: {mingw_ssl_dir}")
print("=== END MINGW OPENSSL CHECK ===")

check_call('make -C bundle libevent VERBOSE=1', shell=True, env=env)

for arch in os.environ.get('CI_CROSS_TARGETS', '').split(':'):
    if not arch:
        continue

    arch, _sep, arch_ver = arch.partition('@')

    print('Enable', arch, arch_ver)

    with open('configure/CONFIG_SITE.local', 'a') as F:
        F.write('\nCROSS_COMPILER_TARGET_ARCHS+=%s\n'%arch)

    check_call('make -C bundle libevent.'+arch+' VERBOSE=1', shell=True, env=env)
