# i am at git master branch

How to Install Open vSwitch on Linux, FreeBSD and NetBSD
========================================================

This document describes how to build and install Open vSwitch on a
generic Linux, FreeBSD, or NetBSD host. For specifics around installation
on a specific platform, please see one of these files:

  - [INSTALL.Debian.md]
  - [INSTALL.Fedora.md]
  - [INSTALL.RHEL.md]
  - [INSTALL.XenServer.md]
  - [INSTALL.NetBSD.md]
  - [INSTALL.Windows.md]
  - [INSTALL.DPDK.md]

Build Requirements
------------------

To compile the userspace programs in the Open vSwitch distribution,
you will need the following software:

  - GNU make.

  - A C compiler, such as:

      * GCC 4.x.

      * Clang.  Clang 3.4 and later provide useful static semantic
        analysis and thread-safety checks.  For Ubuntu, there are
        nightly built packages available on clang's website.

      * MSVC 2013.  See [INSTALL.Windows] for additional Windows build
        instructions.

    While OVS may be compatible with other compilers, optimal
    support for atomic operations may be missing, making OVS very
    slow (see lib/ovs-atomic.h).

  - libssl, from OpenSSL, is optional but recommended if you plan to
    connect the Open vSwitch to an OpenFlow controller.  libssl is
    required to establish confidentiality and authenticity in the
    connections from an Open vSwitch to an OpenFlow controller.  If
    libssl is installed, then Open vSwitch will automatically build
    with support for it.

  - libcap-ng, written by Steve Grubb, is optional but recommended.  It
    is required to run OVS daemons as a non-root user with dropped root
    privileges.  If libcap-ng is installed, then Open vSwitch will
    automatically build with support for it.

  - Python 2.7.

On Linux, you may choose to compile the kernel module that comes with
the Open vSwitch distribution or to use the kernel module built into
the Linux kernel (version 3.3 or later).  See the [FAQ.md] question
"What features are not available in the Open vSwitch kernel datapath that
ships as part of the upstream Linux kernel?" for more information on
this trade-off.  You may also use the userspace-only implementation,
at some cost in features and performance (see [INSTALL.userspace.md]
for details).  To compile the kernel module on Linux, you must also
install the following:

  - A supported Linux kernel version.  Please refer to [README.md] for a
    list of supported versions.

    The Open vSwitch datapath requires bridging support
    (CONFIG_BRIDGE) to be built as a kernel module.  (This is common
    in kernels provided by Linux distributions.)  The bridge module
    must not be loaded or in use.  If the bridge module is running
    (check with "lsmod | grep bridge"), you must remove it ("rmmod
    bridge") before starting the datapath.

    For optional support of ingress policing, you must enable kernel
    configuration options NET_CLS_BASIC, NET_SCH_INGRESS, and
    NET_ACT_POLICE, either built-in or as modules.  (NET_CLS_POLICE is
    obsolete and not needed.)

    To use GRE tunneling on Linux 2.6.37 or newer, kernel support
    for GRE demultiplexing (CONFIG_NET_IPGRE_DEMUX) must be compiled
    in or available as a module.  Also, on kernels before 3.11, the
    ip_gre module, for GRE tunnels over IP (NET_IPGRE), must not be
    loaded or compiled in.

    To configure HTB or HFSC quality of service with Open vSwitch,
    you must enable the respective configuration options.

    To use Open vSwitch support for TAP devices, you must enable
    CONFIG_TUN.

  - To build a kernel module, you need the same version of GCC that
    was used to build that kernel.

  - A kernel build directory corresponding to the Linux kernel image
    the module is to run on.  Under Debian and Ubuntu, for example,
    each linux-image package containing a kernel binary has a
    corresponding linux-headers package with the required build
    infrastructure.

If you are working from a Git tree or snapshot (instead of from a
distribution tarball), or if you modify the Open vSwitch build system
or the database schema, you will also need the following software:

  - Autoconf version 2.63 or later.

  - Automake version 1.10 or later.

  - libtool version 2.4 or later.  (Older versions might work too.)

To run the unit tests, you also need:

  - Perl.  Version 5.10.1 is known to work.  Earlier versions should
    also work.

The ovs-vswitchd.conf.db(5) manpage will include an E-R diagram, in
formats other than plain text, only if you have the following:

  - "dot" from graphviz (http://www.graphviz.org/).

  - Perl.  Version 5.10.1 is known to work.  Earlier versions should
    also work.

If you are going to extensively modify Open vSwitch, please consider
installing the following to obtain better warnings:

  - "sparse" version 0.4.4 or later
    (http://www.kernel.org/pub/software/devel/sparse/dist/).

  - GNU make.

  - clang, version 3.4 or later

Also, you may find the ovs-dev script found in utilities/ovs-dev.py useful.

Installation Requirements
-------------------------

The machine on which Open vSwitch is to be installed must have the
following software:

  - libc compatible with the libc used for build.

  - libssl compatible with the libssl used for build, if OpenSSL was
    used for the build.

  - On Linux, the same kernel version configured as part of the build.

  - For optional support of ingress policing on Linux, the "tc" program
    from iproute2 (part of all major distributions and available at
    http://www.linux-foundation.org/en/Net:Iproute2).

On Linux you should ensure that /dev/urandom exists.  To support TAP
devices, you must also ensure that /dev/net/tun exists.

Building and Installing Open vSwitch for Linux, FreeBSD or NetBSD
=================================================================

Once you have installed all the prerequisites listed above in the
Base Prerequisites section, follow the procedures below to bootstrap,
to configure and to build the code.

Bootstrapping the Sources
-------------------------

This step is not needed if you have downloaded a released tarball.
If you pulled the sources directly from an Open vSwitch Git tree or
got a Git tree snapshot, then run boot.sh in the top source directory
to build the "configure" script.

      `% ./boot.sh`


Configuring the Sources
-----------------------

Configure the package by running the configure script.  You can
usually invoke configure without any arguments.  For example:

      `% ./configure`

By default all files are installed under /usr/local.  If you want
to install into, e.g., /usr and /var instead of /usr/local and
/usr/local/var, add options as shown here:

      `% ./configure --prefix=/usr --localstatedir=/var`

By default, static libraries are built and linked against. If you
want to use shared libraries instead:

      % ./configure --enable-shared

To use a specific C compiler for compiling Open vSwitch user
programs, also specify it on the configure command line, like so:

      `% ./configure CC=gcc-4.2`

To use 'clang' compiler:

      `% ./configure CC=clang`

To supply special flags to the C compiler, specify them as CFLAGS on
the configure command line.  If you want the default CFLAGS, which
include "-g" to build debug symbols and "-O2" to enable optimizations,
you must include them yourself.  For example, to build with the
default CFLAGS plus "-mssse3", you might run configure as follows:

      `% ./configure CFLAGS="-g -O2 -mssse3"`

Note that these CFLAGS are not applied when building the Linux
kernel module.  Custom CFLAGS for the kernel module are supplied
using the EXTRA_CFLAGS variable when running make.  So, for example:

      `% make EXTRA_CFLAGS="-Wno-error=date-time"

To build the Linux kernel module, so that you can run the
kernel-based switch, pass the location of the kernel build
directory on --with-linux.  For example, to build for a running
instance of Linux:

      `% ./configure --with-linux=/lib/modules/`uname -r`/build`

If --with-linux requests building for an unsupported version of
Linux, then "configure" will fail with an error message.  Please
refer to the [FAQ.md] for advice in that case.

If you wish to build the kernel module for an architecture other
than the architecture of the machine used for the build, you may
specify the kernel architecture string using the KARCH variable
when invoking the configure script.  For example, to build for MIPS
with Linux:

      `% ./configure --with-linux=/path/to/linux KARCH=mips`

If you plan to do much Open vSwitch development, you might want to
add --enable-Werror, which adds the -Werror option to the compiler
command line, turning warnings into errors.  That makes it
impossible to miss warnings generated by the build.

To build with gcov code coverage support, add --enable-coverage,
e.g.:

      `% ./configure --enable-coverage`

The configure script accepts a number of other options and honors
additional environment variables.  For a full list, invoke
configure with the --help option.

You can also run configure from a separate build directory.  This
is helpful if you want to build Open vSwitch in more than one way
from a single source directory, e.g. to try out both GCC and Clang
builds, or to build kernel modules for more than one Linux version.
Here is an example:

      `% mkdir _gcc && (cd _gcc && ../configure CC=gcc)`  
      `% mkdir _clang && (cd _clang && ../configure CC=clang)`


Building the Sources
--------------------

1. Run GNU make in the build directory, e.g.:

      `% make`

   or if GNU make is installed as "gmake":

      `% gmake`

   If you used a separate build directory, run make or gmake from that
   directory, e.g.:

      `% make -C _gcc`  
      `% make -C _clang`

   For improved warnings if you installed "sparse" (see "Prerequisites"),
   add C=1 to the command line.

   Some versions of Clang and ccache are not completely compatible.
   If you see unusual warnings when you use both together, consider
   disabling ccache for use with Clang.

2. Consider running the testsuite.  Refer to "Running the Testsuite"
   below, for instructions.

3. Become root by running "su" or another program.

4. Run "make install" to install the executables and manpages into the
   running system, by default under /usr/local.

5. If you built kernel modules, you may install and load them, e.g.:

      `% make modules_install`  
      `% /sbin/modprobe openvswitch`

   To verify that the modules have been loaded, run "/sbin/lsmod" and
   check that openvswitch is listed.

   If the `modprobe` operation fails, look at the last few kernel log
   messages (e.g. with `dmesg | tail`):

      - The message "openvswitch: exports duplicate symbol
        br_should_route_hook (owned by bridge)" means that the bridge
        module is loaded.  Run `/sbin/rmmod bridge` to remove it.

        If `/sbin/rmmod bridge` fails with "ERROR: Module bridge does
        not exist in /proc/modules", then the bridge is compiled into
        the kernel, rather than as a module.  Open vSwitch does not
        support this configuration (see "Build Requirements", above).

      - The message "openvswitch: exports duplicate symbol
        dp_ioctl_hook (owned by ofdatapath)" means that the ofdatapath
        module from the OpenFlow reference implementation is loaded.
        Run `/sbin/rmmod ofdatapath` to remove it.  (You might have to
        delete any existing datapaths beforehand, using the "dpctl"
        program included with the OpenFlow reference implementation.
        "ovs-dpctl" will not work.)

      - Otherwise, the most likely problem is that Open vSwitch was
        built for a kernel different from the one into which you are
        trying to load it.  Run `modinfo` on openvswitch.ko and on
        a module built for the running kernel, e.g.:

           ```
           % /sbin/modinfo openvswitch.ko
           % /sbin/modinfo /lib/modules/`uname -r`/kernel/net/bridge/bridge.ko
           ```

        Compare the "vermagic" lines output by the two commands.  If
        they differ, then Open vSwitch was built for the wrong kernel.

      - If you decide to report a bug or ask a question related to
        module loading, please include the output from the `dmesg` and
        `modinfo` commands mentioned above.

There is an optional module parameter to openvswitch.ko called
vlan_tso that enables TCP segmentation offload over VLANs on NICs
that support it. Many drivers do not expose support for TSO on VLANs
in a way that Open vSwitch can use but there is no way to detect
whether this is the case. If you know that your particular driver can
handle it (for example by testing sending large TCP packets over VLANs)
then passing in a value of 1 may improve performance. Modules built for
Linux kernels 2.6.37 and later, as well as specially patched versions
of earlier kernels, do not need this and do not have this parameter. If
you do not understand what this means or do not know if your driver
will work, do not set this.

6. Initialize the configuration database using ovsdb-tool, e.g.:

      `% mkdir -p /usr/local/etc/openvswitch`  
      `% ovsdb-tool create /usr/local/etc/openvswitch/conf.db vswitchd/vswitch.ovsschema`

Startup
=======

Before starting ovs-vswitchd itself, you need to start its
configuration database, ovsdb-server.  Each machine on which Open
vSwitch is installed should run its own copy of ovsdb-server.
Configure it to use the database you created during installation (as
explained above), to listen on a Unix domain socket, to connect to any
managers specified in the database itself, and to use the SSL
configuration in the database:

      % ovsdb-server --remote=punix:/usr/local/var/run/openvswitch/db.sock \
                     --remote=db:Open_vSwitch,Open_vSwitch,manager_options \
                     --private-key=db:Open_vSwitch,SSL,private_key \
                     --certificate=db:Open_vSwitch,SSL,certificate \
                     --bootstrap-ca-cert=db:Open_vSwitch,SSL,ca_cert \
                     --pidfile --detach

(If you built Open vSwitch without SSL support, then omit
--private-key, --certificate, and --bootstrap-ca-cert.)

Then initialize the database using ovs-vsctl.  This is only
necessary the first time after you create the database with
ovsdb-tool (but running it at any time is harmless):

      % ovs-vsctl --no-wait init

Then start the main Open vSwitch daemon, telling it to connect to the
same Unix domain socket:

      % ovs-vswitchd --pidfile --detach

Now you may use ovs-vsctl to set up bridges and other Open vSwitch
features.  For example, to create a bridge named br0 and add ports
eth0 and vif1.0 to it:

      % ovs-vsctl add-br br0
      % ovs-vsctl add-port br0 eth0
      % ovs-vsctl add-port br0 vif1.0

Please refer to ovs-vsctl(8) for more details.

Upgrading
=========

When you upgrade Open vSwitch from one version to another, you should
also upgrade the database schema:

1. Stop the Open vSwitch daemons, e.g.:

      ```
      % kill `cd /usr/local/var/run/openvswitch && cat ovsdb-server.pid ovs-vswitchd.pid`
      ```

2. Install the new Open vSwitch release.

3. Upgrade the database, in one of the following two ways:

      - If there is no important data in your database, then you may
        delete the database file and recreate it with ovsdb-tool,
        following the instructions under "Building and Installing Open
        vSwitch for Linux, FreeBSD or NetBSD".

      - If you want to preserve the contents of your database, back it
        up first, then use "ovsdb-tool convert" to upgrade it, e.g.:

        `% ovsdb-tool convert /usr/local/etc/openvswitch/conf.db vswitchd/vswitch.ovsschema`

4. Start the Open vSwitch daemons as described under "Building and
   Installing Open vSwitch for Linux, FreeBSD or NetBSD" above.

Hot Upgrading
=============
Upgrading Open vSwitch from one version to the next version with minimum
disruption of traffic going through the system that is using that Open vSwitch
needs some considerations:

1. If the upgrade only involves upgrading the userspace utilities and daemons
of Open vSwitch, make sure that the new userspace version is compatible with
the previously loaded kernel module.

2. An upgrade of userspace daemons means that they have to be restarted.
Restarting the daemons means that the OpenFlow flows in the ovs-vswitchd daemon
will be lost.  One way to restore the flows is to let the controller
re-populate it.  Another way is to save the previous flows using a utility
like ovs-ofctl and then re-add them after the restart. Restoring the old flows
is accurate only if the new Open vSwitch interfaces retain the old 'ofport'
values.

3. When the new userspace daemons get restarted, they automatically flush
the old flows setup in the kernel. This can be expensive if there are hundreds
of new flows that are entering the kernel but userspace daemons are busy
setting up new userspace flows from either the controller or an utility like
ovs-ofctl. Open vSwitch database provides an option to solve this problem
through the other_config:flow-restore-wait column of the Open_vSwitch table.
Refer to the ovs-vswitchd.conf.db(5) manpage for details.

4. If the upgrade also involves upgrading the kernel module, the old kernel
module needs to be unloaded and the new kernel module should be loaded. This
means that the kernel network devices belonging to Open vSwitch is recreated
and the kernel flows are lost. The downtime of the traffic can be reduced
if the userspace daemons are restarted immediately and the userspace flows
are restored as soon as possible.

The ovs-ctl utility's "restart" function only restarts the userspace daemons,
makes sure that the 'ofport' values remain consistent across restarts, restores
userspace flows using the ovs-ofctl utility and also uses the
other_config:flow-restore-wait column to keep the traffic downtime to the
minimum. The ovs-ctl utility's "force-reload-kmod" function does all of the
above, but also replaces the old kernel module with the new one. Open vSwitch
startup scripts for Debian, XenServer and RHEL use ovs-ctl's functions and it
is recommended that these functions be used for other software platforms too.

Testsuites
==========

This section describe Open vSwitch's built-in support for various test
suites.  You must bootstrap, configure and build Open vSwitch (steps are
in "Building and Installing Open vSwitch for Linux, FreeBSD or NetBSD"
above) before you run the tests described here.  You do not need to
install Open vSwitch or to build or load the kernel module to run
these test suites.  You do not need supervisor privilege to run these
test suites.

Self-Tests
----------

Open vSwitch includes a suite of self-tests.  Before you submit patches
upstream, we advise that you run the tests and ensure that they pass.
If you add new features to Open vSwitch, then adding tests for those
features will ensure your features don't break as developers modify
other areas of Open vSwitch.

Refer to "Testsuites" above for prerequisites.

To run all the unit tests in Open vSwitch, one at a time:
      `make check`
This takes under 5 minutes on a modern desktop system.

To run all the unit tests in Open vSwitch, up to 8 in parallel:
      `make check TESTSUITEFLAGS=-j8`
This takes under a minute on a modern 4-core desktop system.

To see a list of all the available tests, run:
      `make check TESTSUITEFLAGS=--list`

To run only a subset of tests, e.g. test 123 and tests 477 through 484:
      `make check TESTSUITEFLAGS='123 477-484'`
(Tests do not have inter-dependencies, so you may run any subset.)

To run tests matching a keyword, e.g. "ovsdb":
      `make check TESTSUITEFLAGS='-k ovsdb'`

To see a complete list of test options:
      `make check TESTSUITEFLAGS=--help`

The results of a testing run are reported in tests/testsuite.log.
Please report test failures as bugs and include the testsuite.log in
your report.

If you have "valgrind" installed, then you can also run the testsuite
under valgrind by using "make check-valgrind" in place of "make
check".  All the same options are available via TESTSUITEFLAGS.  When
you do this, the "valgrind" results for test `<N>` are reported in files
named `tests/testsuite.dir/<N>/valgrind.*`.  You may find that the
valgrind results are easier to interpret if you put "-q" in
~/.valgrindrc, since that reduces the amount of output.

Sometimes a few tests may fail on some runs but not others.  This is
usually a bug in the testsuite, not a bug in Open vSwitch itself.  If
you find that a test fails intermittently, please report it, since the
developers may not have noticed.

OFTest
------

OFTest is an OpenFlow protocol testing suite.  Open vSwitch includes a
Makefile target to run OFTest with Open vSwitch in "dummy mode".  In
this mode of testing, no packets travel across physical or virtual
networks.  Instead, Unix domain sockets stand in as simulated
networks.  This simulation is imperfect, but it is much easier to set
up, does not require extra physical or virtual hardware, and does not
require supervisor privileges.

To run OFTest with Open vSwitch, first read and follow the
instructions under "Testsuites" above.  Second, obtain a copy of
OFTest and install its prerequisites.  You need a copy of OFTest that
includes commit 406614846c5 (make ovs-dummy platform work again).
This commit was merged into the OFTest repository on Feb 1, 2013, so
any copy of OFTest more recent than that should work.  Testing OVS in
dummy mode does not require root privilege, so you may ignore that
requirement.

Optionally, add the top-level OFTest directory (containing the "oft"
program) to your $PATH.  This slightly simplifies running OFTest later.

To run OFTest in dummy mode, run the following command from your Open
vSwitch build directory:
    `make check-oftest OFT=<oft-binary>`
where `<oft-binary>` is the absolute path to the "oft" program in
OFTest.

If you added "oft" to your $PATH, you may omit the OFT variable
assignment:
    `make check-oftest`
By default, "check-oftest" passes "oft" just enough options to enable
dummy mode.  You can use OFTFLAGS to pass additional options.  For
example, to run just the basic.Echo test instead of all tests (the
default) and enable verbose logging:
    `make check-oftest OFT=<oft-binary> OFTFLAGS='--verbose -T basic.Echo'`

If you use OFTest that does not include commit 4d1f3eb2c792 (oft:
change default port to 6653), merged into the OFTest repository in
October 2013, then you need to add an option to use the IETF-assigned
controller port:
    `make check-oftest OFT=<oft-binary> OFTFLAGS='--port=6653'`

Please interpret OFTest results cautiously.  Open vSwitch can fail a
given test in OFTest for many reasons, including bugs in Open vSwitch,
bugs in OFTest, bugs in the "dummy mode" integration, and differing
interpretations of the OpenFlow standard and other standards.

Open vSwitch has not been validated against OFTest.  Please do report
test failures that you believe to represent bugs in Open vSwitch.
Include the precise versions of Open vSwitch and OFTest in your bug
report, plus any other information needed to reproduce the problem.

Ryu
---

Ryu is an OpenFlow controller written in Python that includes an
extensive OpenFlow testsuite.  Open vSwitch includes a Makefile target
to run Ryu in "dummy mode".  See "OFTest" above for an explanation of
dummy mode.

To run Ryu tests with Open vSwitch, first read and follow the
instructions under "Testsuites" above.  Second, obtain a copy of Ryu,
install its prerequisites, and build it.  You do not need to install
Ryu (some of the tests do not get installed, so it does not help).

To run Ryu tests, run the following command from your Open vSwitch
build directory:
    `make check-ryu RYUDIR=<ryu-source-dir>`
where `<ryu-source-dir>` is the absolute path to the root of the Ryu
source distribution.  The default `<ryu-source-dir>` is `$srcdir/../ryu`
where $srcdir is your Open vSwitch source directory, so if this
default is correct then you make simply run `make check-ryu`.

Open vSwitch has not been validated against Ryu.  Please do report
test failures that you believe to represent bugs in Open vSwitch.
Include the precise versions of Open vSwitch and Ryu in your bug
report, plus any other information needed to reproduce the problem.

Vagrant
-------

Requires: Vagrant (version 1.7.0 or later) and a compatible hypervisor

You must bootstrap and configure the sources (steps are in "Building
and Installing Open vSwitch for Linux, FreeBSD or NetBSD" above) before
you run the steps described here.

A Vagrantfile is provided allowing to compile and provision the source
tree as found locally in a virtual machine using the following commands:

	vagrant up
	vagrant ssh

This will bring up w Fedora 20 VM by default, alternatively the
`Vagrantfile` can be modified to use a different distribution box as
base. Also, the VM can be reprovisioned at any time:

	vagrant provision

OVS out-of-tree compilation environment can be set up with:

	./boot.sh
	vagrant provision --provision-with configure_ovs,build_ovs

This will set up an out-of-tree build environment in /home/vagrant/build.
The source code can be found in /vagrant.  Out-of-tree build is preferred
to work around limitations of the sync file systems.

To recompile and reinstall OVS using RPM:

	./boot.sh
	vagrant provision --provision-with configure_ovs,install_rpm

Two provisioners are included to run system tests with the OVS kernel
module or with a userspace datapath.  This tests are different from
the self-tests mentioned above.  To run them:

	./boot.sh
	vagrant provision --provision-with configure_ovs,test_ovs_kmod,test_ovs_system_userspace

Continuous Integration with Travis-CI
-------------------------------------

A .travis.yml file is provided to automatically build Open vSwitch with
various build configurations and run the testsuite using travis-ci.
Builds will be performed with gcc, sparse and clang with the -Werror
compiler flag included, therefore the build will fail if a new warning
has been introduced.

The CI build is triggered via git push (regardless of the specific
branch) or pull request against any Open vSwitch GitHub repository that
is linked to travis-ci.

Instructions to setup travis-ci for your GitHub repository:

1. Go to http://travis-ci.org/ and sign in using your GitHub ID.
2. Go to the "Repositories" tab and enable the ovs repository. You
   may disable builds for pushes or pull requests.
3. In order to avoid forks sending build failures to the upstream
   mailing list, the notification email recipient is encrypted. If you
   want to receive email notification for build failures, replace the
   the encrypted string:
   3.1) Install the travis-ci CLI (Requires ruby >=2.0):
           gem install travis
   3.2) In your Open vSwitch repository:
           travis encrypt mylist@mydomain.org
   3.3) Add/replace the notifications section in .travis.yml and fill
        in the secure string as returned by travis encrypt:

         notifications:
           email:
             recipients:
               - secure: "....."

   (You may remove/omit the notifications section to fall back to
    default notification behaviour which is to send an email directly
    to the author and committer of the failing commit. Note that the
    email is only sent if the author/committer have commit rights for
    the particular GitHub repository).

4. Pushing a commit to the repository which breaks the build or the
   testsuite will now trigger a email sent to mylist@mydomain.org

Bug Reporting
=============

Please report problems to bugs@openvswitch.org.

[README.md]:README.md
[INSTALL.Debian.md]:INSTALL.Debian.md
[INSTALL.Fedora.md]:INSTALL.Fedora.md
[INSTALL.RHEL.md]:INSTALL.RHEL.md
[INSTALL.XenServer.md]:INSTALL.XenServer.md
[INSTALL.NetBSD.md]:INSTALL.NetBSD.md
[INSTALL.DPDK.md]:INSTALL.DPDK.md
[INSTALL.userspace.md]:INSTALL.userspace.md
[FAQ.md]:FAQ.md
