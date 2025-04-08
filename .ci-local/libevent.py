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

# Inspect the CMake toolchain file
print("=== CHECKING CMAKE TOOLCHAIN FILE ===")
cmake_toolchain_file = 'bundle/cmake/x86_64-w64-mingw32.cmake'
if os.path.exists(cmake_toolchain_file):
    print(f"CMake toolchain file exists: {cmake_toolchain_file}")
    try:
        with open(cmake_toolchain_file, 'r') as f:
            content = f.read()
            print(f"Content of {cmake_toolchain_file}:")
            print(content)
    except Exception as e:
        print(f"Error reading CMake toolchain file: {e}")
else:
    print(f"CMake toolchain file NOT found: {cmake_toolchain_file}")
print("=== END CMAKE TOOLCHAIN FILE CHECK ===")

# Diagnostic: Print OpenSSL version to verify it's available
print("=== CHECKING OPENSSL INSTALLATION ===")
check_call('openssl version -a', shell=True)
print("=== END OPENSSL CHECK ===")

# Enhanced Diagnostic: Check MinGW OpenSSL installation
print("=== CHECKING MINGW OPENSSL INSTALLATION ===")
mingw_ssl_dir = '/usr/x86_64-w64-mingw32'
print(f"Checking if MinGW OpenSSL directory exists: {mingw_ssl_dir}")
if os.path.exists(mingw_ssl_dir):
    print(f"MinGW base directory exists: {mingw_ssl_dir}")
    try:
        # Check base directory content
        print(f"Contents of MinGW base directory:")
        check_call(f'ls -la {mingw_ssl_dir}', shell=True)
        
        # Check for OpenSSL headers in various possible locations
        mingw_include_dirs = [
            f'{mingw_ssl_dir}/include/openssl',  # Standard location
            f'{mingw_ssl_dir}/ssl/include/openssl',  # Alternative location
        ]
        
        for include_dir in mingw_include_dirs:
            if os.path.exists(include_dir):
                print(f"Found OpenSSL headers at: {include_dir}")
                check_call(f'ls -la {include_dir} | head -5', shell=True)
            else:
                print(f"No OpenSSL headers at: {include_dir}")
        
        # Check for OpenSSL libraries in various possible locations
        mingw_lib_dirs = [
            f'{mingw_ssl_dir}/lib',  # Standard location
            f'{mingw_ssl_dir}/ssl/lib',  # Alternative location
            f'{mingw_ssl_dir}/bin',  # DLLs might be here
        ]
        
        for lib_dir in mingw_lib_dirs:
            if os.path.exists(lib_dir):
                print(f"Checking for OpenSSL libraries in: {lib_dir}")
                
                # Check for specific library names with different patterns
                try:
                    print(f"Looking for libssl/libcrypto files in {lib_dir}:")
                    check_call(f'find {lib_dir} -name "*ssl*" -o -name "*crypto*" | sort', shell=True)
                except:
                    print(f"No files matching ssl/crypto patterns found in {lib_dir}")
                
                # List all files to see what's actually there
                print(f"First 20 files in {lib_dir}:")
                check_call(f'ls -la {lib_dir} | head -20', shell=True)
            else:
                print(f"Directory does not exist: {lib_dir}")
        
        # Check for pkg-config files which could help with configuration
        pkgconfig_dir = f'{mingw_ssl_dir}/lib/pkgconfig'
        if os.path.exists(pkgconfig_dir):
            print(f"Checking pkg-config directory: {pkgconfig_dir}")
            check_call(f'ls -la {pkgconfig_dir}', shell=True)
            try:
                check_call(f'cat {pkgconfig_dir}/openssl.pc', shell=True)
            except:
                print("No openssl.pc file found")
        else:
            print(f"No pkg-config directory found at: {pkgconfig_dir}")
            
    except Exception as e:
        print(f"Error checking MinGW OpenSSL: {e}")
else:
    print(f"ERROR: MinGW directory does NOT exist: {mingw_ssl_dir}")

# Check for any other SSL-related files across the MinGW directory structure
print("Searching for any OpenSSL-related files in the entire MinGW directory:")
try:
    check_call(f'find {mingw_ssl_dir} -name "*ssl*" | head -20', shell=True)
except Exception as e:
    print(f"Error searching for SSL files: {e}")

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
