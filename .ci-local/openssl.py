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

def find_msvc():
    """Check if MSVC is available"""
    # Check for cl.exe in PATH
    cl = shutil.which('cl.exe')
    if cl:
        return True
    # Check common Visual Studio locations
    vs_paths = [
        r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC',
        r'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC',
        r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Tools\MSVC',
        r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC',
    ]
    for vs_path in vs_paths:
        if os.path.exists(vs_path):
            # Look for any version subdirectory
            try:
                versions = os.listdir(vs_path)
                if versions:
                    return True
            except:
                pass
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
    print("Configuring OpenSSL...")
    configure_cmd = [
        perl,
        'Configure',
        config_target,
        'no-tests',
        'shared',
        'zlib-dynamic',
        '--prefix={}'.format(install_prefix),
        '--openssldir={}'.format(install_prefix)
    ]
    
    check_call(configure_cmd, cwd=source_dir, shell=True)
    
    # Build OpenSSL
    print("Building OpenSSL (this may take a while)...")
    check_call('nmake', cwd=source_dir, shell=True)
    
    # Install OpenSSL
    print("Installing OpenSSL...")
    check_call('nmake install', cwd=source_dir, shell=True)
    
    print("OpenSSL build and installation complete!")
    print("Installation directory: {}".format(install_prefix))
    
    # Verify installation
    lib_dir = os.path.join(install_prefix, 'lib')
    ssl_lib = os.path.join(lib_dir, 'ssl.lib')
    crypto_lib = os.path.join(lib_dir, 'crypto.lib')
    
    if not os.path.exists(ssl_lib):
        raise RuntimeError("OpenSSL installation verification failed: {} not found".format(ssl_lib))
    if not os.path.exists(crypto_lib):
        raise RuntimeError("OpenSSL installation verification failed: {} not found".format(crypto_lib))
    
    print("OpenSSL libraries verified:")
    print("  - {}".format(ssl_lib))
    print("  - {}".format(crypto_lib))

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

