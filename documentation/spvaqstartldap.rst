.. _quick_start_ldap:

|guide| LDAP Authenticator
===============================

This section contains a Quick Start |guide| for the Secure PVAccess *LDAP Authenticator*.

    The LDAP Authenticator is an Authenticator that creates an X.509
    certificate from LDAP credentials.

    It prompts the user to log in to the LDAP directory service.
    It then signs a certificate creation request with its private key and passes
    it to the PVACMS, which decodes it with the public key that
    it finds in the LDAP directory entry for the user.  If this succeeds it means that the
    requestor holds the matching private key and so the certificate is generated.

    It uses the LDAP username as the ``common name`` and then concatenates all the ``dc`` components it finds
    to create the organisation while leaving the ``organizational unit`` blank.
    e.g. ``dn: uid=admin,dc=epics,dc=org`` becomes ``CN=admin``, ``O=epics.org``.

Our starting point for this Quick Start Guide is the end of the :ref:`quick_start_std` so if you haven't gone through it yet
do that now then come back here.  You need to have user's configured (``pvacms``, ``admin``, ``softioc``, and ``client``).
We will set up a containerised LDAP Service and configure it so that the users can log in.

When to use the LDAP Authenticator?

- Your network uses Kerberos for login

  If, as is normally the case, you use Kerberos for authentication and
  LDAP for user profile information - *group*, *contact details*, etc - then
  use the Kerberos Authenticator.

- Your network clients login directly to LDAP

  If you've configured your LDAP server for user login then we will
  show how you could use LDAP login credentials to get
  an X.509 certificate.

See :ref:`secure_pvaccess` for general documentation on Secure PVAccess.

Other Quick Start Guides:

- :ref:`quick_start`
- :ref:`quick_start_std`
- :ref:`quick_start_krb`

|learn| You will learn:
******************************

- :ref:`Creating a containerised LDAP Service <spva_qs_ldap_ldap>`,
- :ref:`Building PVXS with LDAP Authenticator support <spva_qs_ldap_build>`,
- :ref:`Configuring the LDAP schema to support the LDAP Authenticator <spva_qs_ldap_pvacms>`,
- :ref:`Creating certificates using the LDAP Authenticator<spva_qs_ldap_server>` and
- :ref:`Connecting a LDAP Client to an SPVA Server<spva_qs_ldap_client>`

|pre-packaged|\Prepackaged
*********************************

If you want a prepackaged environment, try the following.  You will need three terminal sessions.

|1| Load image
------------------------------

- |terminal|\¹
- start new container with Prepackaged Secure PVAccess with LDAP Authenticator and 4 Users

.. code-block:: shell

    docker run -it --name spva_ldap georgeleveln/spva_ldap:latest

.. code-block:: console

    2025-03-08 19:53:45,557 CRIT Supervisor is running as root.  Privileges were not dropped because no user is specified in the config file.  If you intend to run as root, you can set user=root in the config file to avoid this message.
    2025-03-08 19:53:45,557 INFO Included extra file "/etc/supervisor/conf.d/ldap.conf" during parsing
    2025-03-08 19:53:45,557 INFO Included extra file "/etc/supervisor/conf.d/pvacms.conf" during parsing
    2025-03-08 19:53:45,557 INFO Included extra file "/etc/supervisor/conf.d/sssd.conf" during parsing
    2025-03-08 19:53:45,559 INFO supervisord started with pid 1
    2025-03-08 19:53:46,568 INFO spawned: 'ldap' with pid 7
    2025-03-08 19:53:46,573 INFO spawned: 'pvacms' with pid 8
    2025-03-08 19:53:46,574 INFO spawned: 'sssd' with pid 9
    2025-03-08 19:53:47,688 INFO success: ldap entered RUNNING state, process has stayed up for > than 1 seconds (startsecs)
    2025-03-08 19:53:47,688 INFO success: pvacms entered RUNNING state, process has stayed up for > than 1 seconds (startsecs)
    2025-03-08 19:53:47,688 INFO success: sssd entered RUNNING state, process has stayed up for > than 1 seconds (startsecs)

|2| Service
------------------------------

- |terminal|\²
- log in as softioc service account

.. code-block:: shell

    docker exec -it --user softioc spva_ldap /bin/bash

create a server certificate using the LDAP Authenticator, enter ``secret`` when prompted for LDAP password

.. code-block:: shell

    authnldap -u server

.. code-block:: console

    Enter password for softioc@ca130cc9b352:
    Keychain file created   : /home/softioc/.config/pva/1.3/server.p12
    Certificate identifier  : 47530d89:12147807175996825338


.. code-block:: shell

    pvxcert -f ~/.config/pva/1.3/server.p12

.. code-block:: console

    Certificate Details:
    ============================================
    Subject        : CN=softioc, O=epics.org
    Issuer         : CN=EPICS Root CA, C=US, O=ca.epics.org, OU=EPICS Certificate Authority
    Valid from     : Sat Mar 08 19:56:17 2025 UTC
    Cert Expires   : Sun Mar 08 19:56:17 2026 UTC
    --------------------------------------------

    Certificate Status:
    ============================================
    Certificate ID: 47530d89:12147807175996825338
    Status        : VALID
    Status Issued : Sat Mar 08 19:57:22 2025 UTC
    Status Expires: Sat Mar 08 20:27:22 2025 UTC
    --------------------------------------------

|3| Client
------------------------------

- |terminal|\³
- log in as a Secure PVAccess client

.. code-block:: shell

    docker exec -it --user client spva_ldap /bin/bash

- create a client certificate using the LDAP Authenticator, enter ``secret`` when prompted for LDAP password

.. code-block:: shell

    authnldap

.. code-block:: console

    Enter password for client@epics.org:
    Keychain file created   : /home/client/.config/pva/1.3/client.p12
    Certificate identifier  : 47530d89:11547935522995899879

.. code-block:: shell

    pvxcert -f ~/.config/pva/1.3/client.p12

.. code-block:: console

    Certificate Details:
    ============================================
    Subject        : CN=client, O=epics.org
    Issuer         : CN=EPICS Root CA, C=US, O=ca.epics.org, OU=EPICS Certificate Authority
    Valid from     : Sat Mar 08 20:00:41 2025 UTC
    Cert Expires   : Sun Mar 08 20:00:41 2026 UTC
    --------------------------------------------

    Certificate Status:
    ============================================
    Certificate ID: 47530d89:11547935522995899879
    Status        : VALID
    Status Issued : Sat Mar 08 20:01:59 2025 UTC
    Status Expires: Sat Mar 08 20:31:59 2025 UTC
    --------------------------------------------


|4| Start SoftIOC
------------------------------

- |terminal|\²
- start SoftIOC

.. code-block:: shell

    softIocPVX \
        -m user=test,N=tst,P=tst \
        -d ${PROJECT_HOME}/pvxs/test/testioc.db \
        -d ${PROJECT_HOME}/pvxs/test/testiocg.db \
        -d ${PROJECT_HOME}/pvxs/test/image.db \
        -G ${PROJECT_HOME}/pvxs/test/image.json \
        -a ${PROJECT_HOME}/pvxs/test/testioc.acf

.. code-block:: console

    INFO: PVXS QSRV2 is loaded, permitted, and ENABLED.
    2025-03-08T20:02:23.012770920 WARN pvxs.tcp.init Server unable to bind TCP port 5075, falling back to [::]:40965
    2025-03-08T20:02:23.012856587 WARN pvxs.tcp.init Server unable to bind TLS port 5076, falling back to [::]:35255
    Starting iocInit
    ############################################################################
    ## EPICS R7.0.8.2-DEV
    ## Rev. R7.0.8.1-123-g48607a42586b1a316cd6
    ## Rev. Date Git: 2024-11-29 17:08:28 +0000
    ############################################################################
    iocRun: All initialization complete
    epics>

|5| Get PV value
------------------------------

- |terminal|\³
- get a PV ``test:enumExample`` value from the SoftIOC

.. code-block:: shell

    pvxinfo -v test:enumExample

.. code-block:: console

    Effective config
    EPICS_PVA_AUTO_ADDR_LIST=YES
    EPICS_PVA_BROADCAST_PORT=5076
    EPICS_PVA_CONN_TMO=30
    EPICS_PVA_SERVER_PORT=5075
    EPICS_PVA_TLS_KEYCHAIN=/home/client/.config/pva/1.3/client.p12
    EPICS_PVA_TLS_OPTIONS=on_expiration=fallback-to-tcp on_no_cms=fallback-to-tcp
    EPICS_PVA_TLS_PORT=5076
    XDG_CONFIG_HOME=/home/client/.config/pva/1.3
    XDG_DATA_HOME=/home/client/.local/share/pva/1.3
    # TLS x509:EPICS Root CA/softioc@172.17.0.2:35255
    test:enumExample from 172.17.0.2:35255
    struct "epics:nt/NTEnum:1.0" {
        struct "enum_t" {
            int32_t index
            string[] choices
        } value
        struct "alarm_t" {
            int32_t severity
            int32_t status
            string message
        } alarm
        struct "time_t" {
            int64_t secondsPastEpoch
            int32_t nanoseconds
            int32_t userTag
        } timeStamp
        struct {
            string description
        } display
    }

- verify that connection is TLS

- `TLS x509:EPICS Root CA/softioc @ 172.17.0.2` indicates that:

  - The connection is `TLS`,
  - The Server end of the channel has been authenticated by the Root CA `EPICS Root CA`
  - The Server end of the channel's name has been authenticated as `softioc` and is connecting from host ``172.17.0.2``



|step-by-step| Step-By-Step
********************************

|step| Docker Image
------------------------------------------

|1| Use a Prepackaged spva_std image
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- |terminal|\¹
- Load image

|step| LDAP
------------------------------------------

- Install LDAP
- Create and apply custom LDAP Authenticator schema addition
- Load User Data LDIF
- |terminal|\²
- Run LDAP Service

|step| EPICS Agents
------------------------------------------

|1| Rebuild pvxs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- |terminal|\¹
- rebuild pvxs with LDAP Authenticator support

|2| PVACMS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Configure point to LDAP Service

|3| SoftIOC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Create certificate

|4| Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Create certificate

|step| Run PVACMS
------------------------------------------

|1| Run PVACMS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- |terminal|\³
- Run PVACMS

|step| Run SoftIOC
------------------------------------------

|1| Run SoftIOC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- |terminal|\⁴
- Run SoftIOC

|step| SPVA Client
------------------------------------------

|1| Get Value
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- |terminal|\⁵
- Get PV values

