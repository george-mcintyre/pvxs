.. _quick_start_ldap:

⚡ LDAP Authenticator
===============================

This section contains a Quick Start Guide (⚡) for Secure PVAccess when configured with an `LDAP Directory`.
If you've configured your LDAP server for user login then it will
show how you could use login credentials to connect to LDAP and then exchange verified login details for
an X.509 certificate.  If, as is normally the case, you use Kerberos for authentication and
LDAP for user profile information - group, contact details, etc - then we'll show how you could
configure Kerberos to handle the credentials verification with SPVA and enhance the
certificates generated with information from the LDAP directory.

See :ref:`secure_pvaccess` for general documentation on Secure PVAccess.

Other Quick Start Guides:

- :ref:`quick_start`
- :ref:`quick_start_std`
- :ref:`quick_start_krb`

In this section you'll find quick starts for

- :ref:`Building and Deploying epics-base and the PVXS libraries and executables <spva_qs_ldap_build_and_deploy>`,
- :ref:`Configuring EPICS Agents to access a Secure PVAccess Network <spva_qs_ldap_add_users>`,
- :ref:`Configuring and running PVACMS <spva_qs_ldap_pvacms>`,
- :ref:`Configuring and running a Secure PVAccess Server<spva_qs_ldap_server>` and
- :ref:`Connecting a Client to an SPVA Server<spva_qs_ldap_client>`

If you want a pre-setup quick-start environment to play around in try this:

.. code-block:: shell

    # Term #1
    docker run -it --name ubuntu_pvxs georgeleveln/spva_qs:latest /bin/bash

    # Term #2
    docker exec -it --user admin ubuntu_pvxs /bin/bash

    # Term #3
    docker exec -it --user pvacms ubuntu_pvxs /bin/bash
    pvacms

    # Term #4
    docker exec -it --user softioc ubuntu_pvxs /bin/bash
    softIocPVX \
     -m user=test,N=tst,P=tst \
     -d ${PROJECT_HOME}/pvxs/test/testioc.db \
     -d ${PROJECT_HOME}/pvxs/test/testiocg.db \
     -d ${PROJECT_HOME}/pvxs/test/image.db \
     -G ${PROJECT_HOME}/pvxs/test/image.json \
     -a ${PROJECT_HOME}/pvxs/test/testioc.acf

    # Term 5
    docker exec -it --user client ubuntu_pvxs /bin/bash
    pvxinfo -v test:enumExample


Create VM for Quick Start
-------------------------

1. Locate the image below
^^^^^^^^^^^^^^^^^^^^^^^^^^

+--------------+----------------+--------------------------------------------+
| Distribution | container name | image                                      |
+==============+================+============================================+
| Ubuntu       | ubuntu_pvxs    | ubuntu_latest                              |
+--------------+----------------+--------------------------------------------+
| RHEL         | rhel_pvxs      | registry.access.redhat.com/ubi8/ubi:latest |
+--------------+----------------+--------------------------------------------+
| CentOS       | centos_pvxs    | centos_latest                              |
+--------------+----------------+--------------------------------------------+
| Rocky        | rocky_pvxs     | rocky_latest                               |
+--------------+----------------+--------------------------------------------+
| Alma         | alma_pvxs      | alma_latest                                |
+--------------+----------------+--------------------------------------------+
| Fedora       | fedora_pvxs    | fedora_latest                              |
+--------------+----------------+--------------------------------------------+
| Alpine       | alpine_pvxs    | alpine_latest                              |
+--------------+----------------+--------------------------------------------+


2. Create a container from the image
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # docker run -it --name <container_name> <image> /bin/bash
    docker run -it --name ubuntu_pvxs ubuntu:latest /bin/bash


.. _spva_qs_build_and_deploy:

Build & Deploy epics-base and PVXS
----------------------------------


1. Initialise Environment
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # Make working directory for building project files
    export PROJECT_HOME=/opt/epics
    mkdir -p ${PROJECT_HOME}


2. Install Requirements
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #############
    # For Debian/Ubuntu

    apt-get update
    apt-get install -y \
        build-essential \
        git \
        openssl \
        libssl-dev \
        libevent-dev \
        libsqlite3-dev \
        libcurl4-openssl-dev \
        pkg-config

    #############
    # For RHEL/CentOS/Rocky/Alma Linux/Fedora

    dnf install -y \
        gcc-c++ \
        git \
        make \
        openssl-devel \
        libevent-devel \
        sqlite-devel \
        libcurl-devel \
        pkg-config

    #############
    # For macOS
    # Install Homebrew if not already installed
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    # Update Homebrew and install dependencies
    brew update
    brew install \
        openssl@3 \
        libevent \
        sqlite3 \
        curl \
        pkg-config

    #############
    # For Alpine Linux

    apk add --no-cache \
        build-base \
        git \
        openssl-dev \
        libevent-dev \
        sqlite-dev \
        curl-dev \
        pkgconfig

    #############
    # For RTEMS
    # First install RTEMS toolchain from https://docs.rtems.org/branches/master/user/start/
    # Then ensure these are built into your BSP:
    #   - openssl
    #   - libevent
    #   - sqlite
    #   - libcurl
    # Note: RTEMS support requires additional configuration. See RTEMS-specific documentation.


Note for MacOS users
~~~~~~~~~~~~~~~~~~~~

If you don't have homebrew and don't want to install it, here's how you would install the prerequisites.

.. code-block:: shell

    # Ensure Xcode Command Line Tools are installed
    xcode-select --install

    # Install OpenSSL
    curl -O https://www.openssl.org/source/openssl-3.1.2.tar.gz
    tar -xzf openssl-3.1.2.tar.gz
    cd openssl-3.1.2
    ./Configure darwin64-x86_64-cc
    make
    sudo make install

    # Install libevent
    curl -O https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz
    tar -xzf libevent-2.1.12-stable.tar.gz
    cd libevent-2.1.12-stable
    ./configure
    make
    sudo make install

    # Install SQLite
    curl -O https://sqlite.org/2023/sqlite-autoconf-3430200.tar.gz
    tar -xzf sqlite-autoconf-3430200.tar.gz
    cd sqlite-autoconf-3430200
    ./configure
    make
    sudo make install

    # Install cURL
    # check if its already there
    curl --version
    # If not then install like this:
    curl -O https://curl.se/download/curl-8.1.2.tar.gz
    tar -xzf curl-8.1.2.tar.gz
    cd curl-8.1.2
    ./configure
    make
    sudo make install

    # Install pkg-config
    curl -O https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz
    tar -xzf pkg-config-0.29.2.tar.gz
    cd pkg-config-0.29.2
    ./configure --with-internal-glib
    make
    sudo make install


3. Build epics-base
^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    cd ${PROJECT_HOME}
    git clone --branch 7.0-method_and_authority https://github.com/george-mcintyre/epics-base.git
    cd epics-base

    make -j10 all
    cd ${PROJECT_HOME}

4. Configure PVXS Build
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    cd ${PROJECT_HOME}
    cat >> RELEASE.local <<EOF
    EPICS_BASE = \$(TOP)/../epics-base
    EOF

    # Optional: To enable appropriate Authenticators.
    # Note: `authnstd` is always available.

    # cat >> CONFIG_SITE.local <<EOF
    # PVXS_ENABLE_KRB_AUTH = YES
    # PVXS_ENABLE_JWT_AUTH = YES
    # PVXS_ENABLE_LDAP_AUTH = YES
    #EOF

5. Build PVXS
^^^^^^^^^^^^^

.. code-block:: shell

    cd ${PROJECT_HOME}
    git clone --recursive  --branch tls https://github.com/george-mcintyre/pvxs.git
    cd pvxs

    # Build PVXS

    make -j10 all
    cd ${PROJECT_HOME}


.. _spva_qs_ldap_add_users:


Configure EPICS Agents
-----------------------

This section shows you what basic configuration you'll need for each type of EPICS agent.
Look at the environment variable settings and the file locations referenced by
this configuration to understand how to configure EPICS agents in
your environment.


1. Add a PVACMS EPICS Agent
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell


    # Add user and when prompted use "PVACMS Server" as Full Name
    adduser pvacms


.. code-block:: shell


    # Set up environment for a PVACMS server
    su - pvacms


.. code-block:: shell

    cat >> ~/.bashrc <<EOF

    export XDG_DATA_HOME=\${XDG_DATA_HOME-~/.local/share}
    export XDG_CONFIG_HOME=\${XDG_CONFIG_HOME-~/.config}
    export PROJECT_HOME=/opt/epics

    #### [optional] Set path and name of the CA database file (default: ./certs.db)
    # Environment: EPICS_PVACMS_DB
    # Default    : \${XDG_DATA_HOME}/pva/1.3/certs.db
    # export EPICS_PVACMS_DB=\${XDG_DATA_HOME}/pva/1.3/certs.db

    #### SETUP CA KEYCHAIN FILE
    # Place your CA's certificate and key in this file if you have one
    # otherwise the CA certificate will be created by PVACMS
    # Environment: EPICS_CA_TLS_KEYCHAIN
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/ca.p12
    # export EPICS_CA_TLS_KEYCHAIN=\${XDG_CONFIG_HOME}/pva/1.3/ca.p12

    # Specify the name of your CA
    # Environment: EPICS_CA_NAME, EPICS_CA_ORGANIZATION, EPICS_CA_ORGANIZATIONAL_UNIT
    # Default    : CN=EPICS Root CA, O=ca.epics.org, OU=EPICS Certificate Authority,
    # export EPICS_CA_NAME="EPICS Root CA"
    # export EPICS_CA_ORGANIZATION="ca.epics.org"
    # export EPICS_CA_ORGANIZATIONAL_UNIT="EPICS Certificate Authority"

    #### SETUP PVACMS KEYCHAIN FILE
    # Environment: EPICS_PVACMS_TLS_KEYCHAIN
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/pvacms.p12
    # export EPICS_PVACMS_TLS_KEYCHAIN=\${XDG_CONFIG_HOME}/pva/1.3/pvacms.p12

    # Configure ADMIN user client certificate (will be created for you)
    # This file will be copied to the admin user
    # Environment: EPICS_ADMIN_TLS_KEYCHAIN
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/admin.p12
    # export EPICS_ADMIN_TLS_KEYCHAIN=\${XDG_CONFIG_HOME}/pva/1.3/admin.p12

    # Configure PVACMS ADMIN user access control file
    # Environment: EPICS_PVACMS_ACF
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/pvacms.acf
    # export EPICS_PVACMS_ACF=\${XDG_CONFIG_HOME}/pva/1.3/pvacms.acf

    # set path
    export PATH="\$(echo \${PROJECT_HOME}/pvxs/bin/*):$PATH"

    cd ~
    EOF

    exit


2. Add a PVACMS Administrator EPICS agent
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # Add user and when prompted use "ADMIN User" as Full Name
    adduser admin


.. code-block:: shell

    # Set up environment for pvacms server
    su - admin


.. code-block:: shell

    cat >> ~/.bashrc <<EOF

    export XDG_DATA_HOME=\${XDG_DATA_HOME-~/.local/share}
    export XDG_CONFIG_HOME=\${XDG_CONFIG_HOME-~/.config}
    export PROJECT_HOME=/opt/epics

    #### SETUP ADMIN KEYCHAIN FILE (will be copied from PVACMS)
    # Environment: EPICS_PVA_TLS_KEYCHAIN
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/client.p12
    # export EPICS_PVA_TLS_KEYCHAIN=\${XDG_CONFIG_HOME}/pva/1.3/client.p12

    # set path
    export PATH="\$(echo \${PROJECT_HOME}/pvxs/bin/*):$PATH"

    cd ~
    EOF

    exit

3. Add a Secure EPICS Server Agent - SoftIOC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # Add user and when prompted use "SOFTIOC Server" as Full Name
    adduser softioc


.. code-block:: shell

    # Set up environment for pvacms server
    su - softioc


.. code-block:: shell

    cat >> ~/.bashrc <<EOF

    export XDG_DATA_HOME=\${XDG_DATA_HOME-~/.local/share}
    export XDG_CONFIG_HOME=\${XDG_CONFIG_HOME-~/.config}
    export PROJECT_HOME=/opt/epics

    #### SETUP SOFTIOC KEYCHAIN FILE
    # Environment: EPICS_PVAS_TLS_KEYCHAIN
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/server.p12
    export EPICS_PVAS_TLS_KEYCHAIN=\${XDG_CONFIG_HOME}/pva/1.3/server.p12

    # set path
    export PATH="\$(echo \${PROJECT_HOME}/pvxs/bin/*):$PATH"

    cd ~
    EOF

    exit

4. Add a Secure EPICS Client agent
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # Add user and when prompted use "SPVA Client" as Full Name
    adduser client


.. code-block:: shell

    # Set up environment for pvacms server
    su - client

.. code-block:: shell

    cat >> ~/.bashrc <<EOF

    export XDG_DATA_HOME=\${XDG_DATA_HOME-~/.local/share}
    export XDG_CONFIG_HOME=\${XDG_CONFIG_HOME-~/.config}
    export PROJECT_HOME=/opt/epics

    #### SETUP SPVA Client KEYCHAIN FILE
    # Environment: EPICS_PVA_TLS_KEYCHAIN
    # Default    : \${XDG_CONFIG_HOME}/pva/1.3/client.p12
    export EPICS_PVA_TLS_KEYCHAIN=\${XDG_CONFIG_HOME}/pva/1.3/client.p12

    # set path
    export PATH="\$(echo \${PROJECT_HOME}/pvxs/bin/*):$PATH"

    cd ~
    EOF

    exit


.. _spva_qs_ldap_pvacms:

Running PVACMS
---------------

1. Login as pvacms in a new shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # If you're using docker
    docker exec -it --user pvacms ubuntu_pvxs /bin/bash


2. Running PVACMS and sharing its ADMIN certificate
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #### RUN PVACMS
    #
    # 1. Create root CA
    #   - creates root CA if does not exist,
    #   - at location specified by EPICS_CA_TLS_KEYCHAIN or ${XDG_CONFIG_HOME}/pva/1.3/ca.p12,
    #   - with CN specified by EPICS_CA_NAME
    #   - with  O specified by EPICS_CA_ORGANIZATION
    #   - with OU specified by EPICS_CA_ORGANIZATIONAL_UNIT
    #
    # 2. Create the PVACMS server certificate
    #   - creates server certificate if does not exist,
    #   - at location specified by EPICS_PVACMS_TLS_KEYCHAIN or ${XDG_CONFIG_HOME}/pva/1.3/pvacms.p12,
    #
    # 3. Create PVACMS certificate database
    #   - creates database if does not exist
    #   - at location pointed to by EPICS_PVACMS_DB or ${XDG_DATA_HOME}/pva/1.3/certs.db
    #
    # 4. Create the default ACF file that controls permissions for the PVACMS service
    #   - creates default ACF (or yaml) file
    #   - at location pointed to by EPICS_PVACMS_ACF or ${XDG_CONFIG_HOME}/pva/1.3/pvacms.acf
    #
    # 5. Create the default admin client certificate that can be used to access PVACMS admin functions like REVOKE and APPROVE
    #   - creates default admin client certificate
    #   - at location specified by EPICS_ADMIN_TLS_KEYCHAIN or ${XDG_CONFIG_HOME}/pva/1.3/admin.p12,
    #
    # 6. Start PVACMS service with verbose logging

    pvacms

    ...

    Certificate DB created  : /home/pvacms/.local/share/pva/1.3/certs.db
    Keychain file created   : /home/pvacms/.config/pva/1.3/ca.p12
    Created Default ACF file: /home/pvacms/.config/pva/1.3/pvacms.acf
    Keychain file created   : /home/pvacms/.config/pva/1.3/admin.p12
    Keychain file created   : /home/pvacms/.config/pva/1.3/pvacms.p12
    PVACMS [6caf749c] Service Running

Note the ``6caf749c`` is the issuer ID which is comprised of the first 8 characters
of the hex Subject Key Identifier of the CA certificate.

Leave this PVACMS service running while running SoftIOC and SPVA client below.

3. Copy Admin Certificate to Admin user
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the root shell (not PVACMS shell)

.. code-block:: shell

    mkdir -p ~admin/.config/pva/1.3
    cp -pr ~pvacms/.config/pva/1.3/admin.p12 ~admin/.config/pva/1.3/client.p12
    chown admin ~admin/.config/pva/1.3/client.p12
    chmod 400 ~admin/.config/pva/1.3/client.p12


.. _spva_qs_ldap_server:

Secure PVAccess SoftIOC Server
-------------------------------

1. Login as softioc in a new shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # If you're using docker
    docker exec -it --user softioc ubuntu_pvxs /bin/bash


2. Create Certificate
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #### 1. Create a new server private key and certificate at location specified by EPICS_PVAS_TLS_KEYCHAIN

    authnstd -u server \
      -n "IOC1" \
      -o "KLI:LI01:10" \
      --ou "FACET"

    ...

    Keychain file created   : /home/softioc/.config/pva/1.3/server.p12
    Certificate identifier  : 6caf749c:853259638908858244

    ...

Note the certificate ID ``6caf749c:853259638908858244`` (<issuer_id>:<serial_number>).
You will need this ID to carry out operations on this certificate including APPROVING it.

3. Verify that certificate is created pending approval
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #### 1. Get the current status of a certificate

    pvxcert <issuer_id>:<serial_number>


4. Approve certificate
^^^^^^^^^^^^^^^^^^^^^^^^^^


.. code-block:: shell

    #### 1. Login as admin in a new shell
    docker exec -it --user admin ubuntu_pvxs /bin/bash

    #### 2. Approve the certificate
    pvxcert --approve <issuer_id>:<serial_number>


5. Check the certificate status has changed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #### 1. Back in softIOC shell, get the current status of a certificate

    pvxcert <issuer_id>:<serial_number>


6. Run an SPVA Service
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    softIocPVX \
        -m user=test,N=tst,P=tst \
        -d ${PROJECT_HOME}/pvxs/test/testioc.db \
        -d ${PROJECT_HOME}/pvxs/test/testiocg.db \
        -d ${PROJECT_HOME}/pvxs/test/image.db \
        -G ${PROJECT_HOME}/pvxs/test/image.json \
        -a ${PROJECT_HOME}/pvxs/test/testioc.acf


.. _spva_qs_client:

SPVA Client
---------------

1. Login as client in a new shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    # If you're using docker
    docker exec -it --user client ubuntu_pvxs /bin/bash



2. Create Certificate
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #### 1. Create client key and certificate at location specified by EPICS_PVA_TLS_KEYCHAIN

    authnstd -u client \
      -n "greg" \
      -o "SLAC.STANFORD.EDU" \
      --ou "Controls"


4. Approve certificate
^^^^^^^^^^^^^^^^^^^^^^^^^^


.. code-block:: shell

    #### 1. Switch back to admin shell

    #### 2. Approve the certificate
    pvxcert --approve <issuer_id>:<serial_number>


4. Run an SPVA Client
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell

    #### 1. Back in client shell, get a value from the SoftIOC

    pvxget -F tree test:structExample

    #### 2. Show that the configuration is using TLS
    pvxinfo -v test:enumExample

    #### 3. Show a connection without TLS
    env EPICS_PVA_TLS_KEYCHAIN= pvxinfo -v test:enumExample
