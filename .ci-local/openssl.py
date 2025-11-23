#!/usr/bin/env python
"""
Build OpenSSL for Windows MSVC
Downloads and builds OpenSSL specifically for MSVC when MSVC-compatible libraries aren't available
"""

from __future__ import print_function

import sys
import os
import subprocess as SP
import shutil
import urllib.request
import tarfile
import tempfile

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

# OpenSSL version to build
OPENSSL_VERSION = os.environ.get('OPENSSL_VERSION', '3.1.4')
OPENSSL_URL = 'https://www.openssl.org/source/openssl-{}.tar.gz'.format(OPENSSL_VERSION)

# Installation directory - use a standard Windows location
INSTALL_PREFIX = os.environ.get('OPENSSL_INSTALL_PREFIX', 'C:/OpenSSL-Win64')

def find_perl():
    """Find Perl executable"""
    for perl_name in ('perl.exe', 'perl'):
        perl = shutil.which(perl_name)
        if perl:
            return perl
    raise RuntimeError("Perl not found. Please install Strawberry Perl or ActivePerl")

def find_nasm():
    """Find NASM executable"""
    nasm = shutil.which('nasm.exe') or shutil.which('nasm')
    if not nasm:
        print("WARNING: NASM not found in PATH. OpenSSL build may fail.")
        print("Please install NASM from https://www.nasm.us/")
    return nasm

def find_vcvarsall():
    """Find Visual Studio vcvarsall.bat file"""
    vcvarsall_paths = [
        r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat',
    ]
    for vcvarsall_path in vcvarsall_paths:
        if os.path.exists(vcvarsall_path):
            return vcvarsall_path
    return None

def find_msvc():
    """Check if MSVC is available"""
    # Check for cl.exe in PATH
    cl = shutil.which('cl.exe')
    if cl:
        return True
    # Check if vcvarsall.bat exists
    if find_vcvarsall():
        return True
    return False

def download_openssl(version, url, dest_dir):
    """Download OpenSSL source"""
    archive_name = os.path.basename(url)
    archive_path = os.path.join(dest_dir, archive_name)
    
    if os.path.exists(archive_path):
        print("OpenSSL archive already exists: {}".format(archive_path))
        return archive_path
    
    print("Downloading OpenSSL {} from {}...".format(version, url))
    try:
        urllib.request.urlretrieve(url, archive_path)
        print("Download complete: {}".format(archive_path))
        return archive_path
    except Exception as e:
        print("Failed to download OpenSSL: {}".format(e))
        raise

def extract_openssl(archive_path, dest_dir):
    """Extract OpenSSL source"""
    extract_dir = os.path.join(dest_dir, 'openssl-{}'.format(OPENSSL_VERSION))
    
    if os.path.exists(extract_dir):
        print("OpenSSL source already extracted: {}".format(extract_dir))
        return extract_dir
    
    print("Extracting OpenSSL source...")
    with tarfile.open(archive_path, 'r:gz') as tar:
        tar.extractall(dest_dir)
    
    if not os.path.exists(extract_dir):
        raise RuntimeError("Extraction failed: {} not found".format(extract_dir))
    
    print("Extraction complete: {}".format(extract_dir))
    return extract_dir

def build_openssl(source_dir, install_prefix):
    """Build OpenSSL for Windows MSVC"""
    print("Building OpenSSL for Windows MSVC...")
    print("Source: {}".format(source_dir))
    print("Install prefix: {}".format(install_prefix))
    
    perl = find_perl()
    print("Using Perl: {}".format(perl))
    
    nasm = find_nasm()
    if nasm:
        print("Using NASM: {}".format(nasm))
    
    # Determine architecture (64-bit or 32-bit)
    # For now, assume 64-bit (VC-WIN64A)
    # Could be enhanced to detect architecture
    config_target = 'VC-WIN64A'
    
    # Configure OpenSSL
    # Use no-zlib to avoid needing zlib development libraries
    # zlib is optional and not required for core OpenSSL functionality
    print("Configuring OpenSSL...")
    configure_cmd = [
        perl,
        'Configure',
        config_target,
        'no-tests',
        'shared',
        'no-zlib',
        '--prefix={}'.format(install_prefix),
        '--openssldir={}'.format(install_prefix)
    ]
    
    check_call(configure_cmd, cwd=source_dir, shell=True)
    
    # Find vcvarsall.bat to set up Visual Studio environment
    vcvarsall = find_vcvarsall()
    if not vcvarsall:
        raise RuntimeError("Visual Studio vcvarsall.bat not found. Please install Visual Studio.")
    
    # Verify makefile exists before building
    makefile_path = os.path.join(source_dir, 'makefile')
    if not os.path.exists(makefile_path):
        raise RuntimeError("OpenSSL makefile not found at: {}".format(makefile_path))
    
    # Create a batch file to properly set up environment and run nmake
    # Ensure we're in the right directory and environment is set up
    build_bat = os.path.join(source_dir, 'build_openssl.bat')
    with open(build_bat, 'w', encoding='utf-8') as f:
        f.write('@echo off\n')
        f.write('setlocal\n')
        f.write('cd /d "{}"\n'.format(source_dir))
        f.write('if errorlevel 1 exit /b 1\n')
        f.write('call "{}" amd64 >nul 2>&1\n'.format(vcvarsall))
        f.write('if errorlevel 1 exit /b 1\n')
        f.write('REM Clear any make-related environment variables that might interfere\n')
        f.write('set MAKEFLAGS=\n')
        f.write('set MAKE=\n')
        f.write('if not exist makefile (\n')
        f.write('    echo Error: makefile not found\n')
        f.write('    exit /b 1\n')
        f.write(')\n')
        f.write('nmake.exe /f makefile\n')
        f.write('exit /b %errorlevel%\n')
        f.write('endlocal\n')
    
    # Build OpenSSL - execute the batch file
    print("Building OpenSSL (this may take a while)...")
    build_bat_abs = os.path.abspath(build_bat)
    # Use shell=True with proper quoting
    check_call('"{}"'.format(build_bat_abs), shell=True)
    
    # Create a batch file to properly set up environment and run nmake install
    install_bat = os.path.join(source_dir, 'install_openssl.bat')
    with open(install_bat, 'w', encoding='utf-8') as f:
        f.write('@echo off\n')
        f.write('setlocal\n')
        f.write('cd /d "{}"\n'.format(source_dir))
        f.write('if errorlevel 1 exit /b 1\n')
        f.write('call "{}" amd64 >nul 2>&1\n'.format(vcvarsall))
        f.write('if errorlevel 1 exit /b 1\n')
        f.write('REM Clear any make-related environment variables that might interfere\n')
        f.write('set MAKEFLAGS=\n')
        f.write('set MAKE=\n')
        f.write('if not exist makefile (\n')
        f.write('    echo Error: makefile not found\n')
        f.write('    exit /b 1\n')
        f.write(')\n')
        f.write('nmake.exe /f makefile install\n')
        f.write('exit /b %errorlevel%\n')
        f.write('endlocal\n')
    
    # Install OpenSSL - execute the batch file
    print("Installing OpenSSL...")
    install_bat_abs = os.path.abspath(install_bat)
    # Use shell=True with proper quoting
    check_call('"{}"'.format(install_bat_abs), shell=True)
    
    # Clean up batch files
    try:
        if os.path.exists(build_bat):
            os.remove(build_bat)
        if os.path.exists(install_bat):
            os.remove(install_bat)
    except:
        pass
    
    print("OpenSSL build and installation complete!")
    print("Installation directory: {}".format(install_prefix))
    
    # Verify installation - check for library files
    # OpenSSL installs libraries in the lib directory
    lib_dir = os.path.join(install_prefix, 'lib')
    
    # Check what files actually exist
    if not os.path.exists(lib_dir):
        raise RuntimeError("OpenSSL installation verification failed: lib directory not found at {}".format(lib_dir))
    
    # List all .lib files to see what was installed
    lib_files = []
    if os.path.exists(lib_dir):
        try:
            lib_files = [f for f in os.listdir(lib_dir) if f.endswith('.lib')]
        except Exception as e:
            print("Warning: Could not list lib directory: {}".format(e))
    
    print("Library files found in {}:".format(lib_dir))
    for f in sorted(lib_files):
        print("  - {}".format(f))
    
    # Find the actual ssl and crypto library names
    ssl_lib_name = None
    crypto_lib_name = None
    
    for f in lib_files:
        f_lower = f.lower()
        if 'ssl' in f_lower and not ssl_lib_name:
            ssl_lib_name = f
        if 'crypto' in f_lower and not crypto_lib_name:
            crypto_lib_name = f
    
    if not ssl_lib_name:
        raise RuntimeError("OpenSSL installation verification failed: ssl library not found in {}".format(lib_dir))
    if not crypto_lib_name:
        raise RuntimeError("OpenSSL installation verification failed: crypto library not found in {}".format(lib_dir))
    
    # Create symlinks or copies with standard names if needed
    # MSVC linker expects ssl.lib and crypto.lib
    ssl_lib_standard = os.path.join(lib_dir, 'ssl.lib')
    crypto_lib_standard = os.path.join(lib_dir, 'crypto.lib')
    
    ssl_lib_actual = os.path.join(lib_dir, ssl_lib_name)
    crypto_lib_actual = os.path.join(lib_dir, crypto_lib_name)
    
    # If the standard names don't exist, create them
    # On Windows, we can't create symlinks easily, so we'll just verify the actual names exist
    # The Makefile will need to use the actual library names
    if not os.path.exists(ssl_lib_standard) and ssl_lib_name != 'ssl.lib':
        print("Note: OpenSSL installed ssl library as '{}', not 'ssl.lib'".format(ssl_lib_name))
        print("The build system will need to use the actual library name.")
    
    if not os.path.exists(crypto_lib_standard) and crypto_lib_name != 'crypto.lib':
        print("Note: OpenSSL installed crypto library as '{}', not 'crypto.lib'".format(crypto_lib_name))
        print("The build system will need to use the actual library name.")
    
    print("OpenSSL libraries verified:")
    print("  - SSL library: {}".format(ssl_lib_actual))
    print("  - Crypto library: {}".format(crypto_lib_actual))
    
    # Write library names to a file so the Makefile can read them
    lib_names_file = os.path.join(install_prefix, 'lib', 'openssl_lib_names.txt')
    try:
        with open(lib_names_file, 'w') as f:
            f.write("SSL_LIB={}\n".format(ssl_lib_name))
            f.write("CRYPTO_LIB={}\n".format(crypto_lib_name))
    except:
        pass  # Non-critical

def main():
    """Main function"""
    if not find_msvc():
        print("MSVC not detected. This script is for building OpenSSL with MSVC.")
        print("Skipping OpenSSL build.")
        return 0
    
    # Check if MSVC-compatible OpenSSL already exists
    install_prefix = INSTALL_PREFIX
    lib_dir = os.path.join(install_prefix, 'lib')
    ssl_lib = os.path.join(lib_dir, 'ssl.lib')
    crypto_lib = os.path.join(lib_dir, 'crypto.lib')
    
    if os.path.exists(ssl_lib) and os.path.exists(crypto_lib):
        print("MSVC-compatible OpenSSL already exists at: {}".format(install_prefix))
        print("  - {}".format(ssl_lib))
        print("  - {}".format(crypto_lib))
        return 0
    
    # Create temporary directory for download and build
    temp_dir = tempfile.mkdtemp(prefix='openssl-build-')
    try:
        print("Working directory: {}".format(temp_dir))
        
        # Download OpenSSL
        archive_path = download_openssl(OPENSSL_VERSION, OPENSSL_URL, temp_dir)
        
        # Extract OpenSSL
        source_dir = extract_openssl(archive_path, temp_dir)
        
        # Build OpenSSL
        build_openssl(source_dir, install_prefix)
        
    except Exception as e:
        print("Error building OpenSSL: {}".format(e))
        import traceback
        traceback.print_exc()
        return 1
    finally:
        # Clean up temporary directory
        if os.path.exists(temp_dir):
            print("Cleaning up temporary directory: {}".format(temp_dir))
            shutil.rmtree(temp_dir, ignore_errors=True)
    
    return 0

if __name__ == '__main__':
    sys.exit(main())

