#!/usr/bin/env python

from __future__ import print_function

import sys
import os
import subprocess as SP
import shutil
import urllib.request

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

env = os.environ.copy()
PATH = env['PATH'].split(os.pathsep)

# CMake MinGW generator doesn't like sh.exe in PATH
# NMake generator doesn't care
# strip it out
PATH = [ent for ent in PATH if not os.path.isfile(os.path.join(ent, 'sh.exe'))]
env['PATH'] = os.pathsep.join(PATH)

print('ENV')
[print("  ", frag) for frag in env['PATH'].split(os.pathsep)]
print('PATH')
[print("  ", K, "=", V) for K, V in env.items()]

# Create bundle/sqlite3 directory if it doesn't exist
sqlite_dir = 'bundle/sqlite3'
if not os.path.exists(sqlite_dir):
    os.makedirs(sqlite_dir)

# SQLite version to download
SQLITE_VERSION = "3430200"  # 3.43.2
SQLITE_YEAR = "2023"

# Download SQLite amalgamation
sqlite_url = f"https://www.sqlite.org/{SQLITE_YEAR}/sqlite-amalgamation-{SQLITE_VERSION}.zip"
print(f"Downloading SQLite from {sqlite_url}")
zip_file = os.path.join(sqlite_dir, "sqlite.zip")

urllib.request.urlretrieve(sqlite_url, zip_file)
check_call(f'unzip -o {zip_file} -d {sqlite_dir}', shell=True)

# Set paths for build
sqlite_src = f"{sqlite_dir}/sqlite-amalgamation-{SQLITE_VERSION}"
sqlite_include_dir = f"{sqlite_dir}/include"
sqlite_lib_dir = f"{sqlite_dir}/lib"

# Create include and lib directories
os.makedirs(sqlite_include_dir, exist_ok=True)
os.makedirs(sqlite_lib_dir, exist_ok=True)

# Copy header files to include directory
shutil.copy(f"{sqlite_src}/sqlite3.h", sqlite_include_dir)
shutil.copy(f"{sqlite_src}/sqlite3ext.h", sqlite_include_dir)

print("Building SQLite for native target")
# Compile SQLite for native target
check_call(f'gcc -O2 -c {sqlite_src}/sqlite3.c -o {sqlite_dir}/sqlite3.o -DSQLITE_ENABLE_COLUMN_METADATA', shell=True)
check_call(f'ar rcs {sqlite_lib_dir}/libsqlite3.a {sqlite_dir}/sqlite3.o', shell=True)
check_call(f'gcc -shared -o {sqlite_lib_dir}/libsqlite3.so {sqlite_dir}/sqlite3.o -ldl -lpthread', shell=True)

# Check for cross-compilation targets
for arch in os.environ.get('CI_CROSS_TARGETS', '').split(':'):
    if not arch:
        continue

    arch, _sep, arch_ver = arch.partition('@')
    
    print(f'Building SQLite for {arch}')
    
    if arch == "windows-x64-mingw":
        # Create architecture-specific lib directory
        arch_lib_dir = f"{sqlite_dir}/lib/{arch}"
        os.makedirs(arch_lib_dir, exist_ok=True)
        
        # Cross-compile SQLite for MinGW
        check_call(f'x86_64-w64-mingw32-gcc -O2 -c {sqlite_src}/sqlite3.c -o {sqlite_dir}/sqlite3-mingw.o -DSQLITE_ENABLE_COLUMN_METADATA', shell=True)
        check_call(f'x86_64-w64-mingw32-ar rcs {arch_lib_dir}/libsqlite3.a {sqlite_dir}/sqlite3-mingw.o', shell=True)
        check_call(f'x86_64-w64-mingw32-gcc -shared -o {arch_lib_dir}/sqlite3.dll {sqlite_dir}/sqlite3-mingw.o -Wl,--out-implib,{arch_lib_dir}/libsqlite3.dll.a', shell=True)
        
        # Install to appropriate directory in bundle/usr
        bundledir = 'bundle/usr/windows-x64-mingw'
        bundledir_include = f'{bundledir}/include'
        bundledir_lib = f'{bundledir}/lib'
        
        # Create directories if they don't exist
        os.makedirs(bundledir_include, exist_ok=True)
        os.makedirs(bundledir_lib, exist_ok=True)
        
        # Copy headers and libraries
        shutil.copy(f"{sqlite_include_dir}/sqlite3.h", bundledir_include)
        shutil.copy(f"{sqlite_include_dir}/sqlite3ext.h", bundledir_include)
        shutil.copy(f"{arch_lib_dir}/libsqlite3.a", bundledir_lib)
        shutil.copy(f"{arch_lib_dir}/sqlite3.dll", bundledir_lib)
        shutil.copy(f"{arch_lib_dir}/libsqlite3.dll.a", bundledir_lib)
        
        print(f"SQLite installed to {bundledir}")
        
        # Copy to MinGW directory for direct inclusion
        mingw_dir = '/usr/x86_64-w64-mingw32'
        mingw_include = f'{mingw_dir}/include'
        mingw_lib = f'{mingw_dir}/lib'
        
        print(f"Copying SQLite to MinGW directory {mingw_dir}")
        
        # Copy headers and libraries to MinGW directory
        if os.path.exists(mingw_dir):
            shutil.copy(f"{sqlite_include_dir}/sqlite3.h", mingw_include)
            shutil.copy(f"{sqlite_include_dir}/sqlite3ext.h", mingw_include)
            shutil.copy(f"{arch_lib_dir}/libsqlite3.a", mingw_lib)
            
            # Copy DLL to bin directory
            mingw_bin = f'{mingw_dir}/bin'
            if os.path.exists(mingw_bin):
                shutil.copy(f"{arch_lib_dir}/sqlite3.dll", mingw_bin)
            
            print(f"SQLite also installed to MinGW directories")
        else:
            print(f"MinGW directory {mingw_dir} not found, skipping direct copy")

print("SQLite build and installation complete") 