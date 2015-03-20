lwip_contrib - Miscelaneous LWIP Related Code
=============================================

This repository contains some code I wrote for LWIP embedded applications that
would likely be useful to other developers. There are a few independent components
contained herein:

* mDNS responder
* iperf server
* Simple Discovery responder
* Generic TFTP Server
* Zero copy driver for the STM32F2x7 family of devices.

These components are described in more detail below.


apps/mdns
---------

This application is a very simple mDNS and mDNS-SD responder. It's been tested
to work with avahi and bonjour for windows. An example usage follows:


    static const struct mdns_service services[] = {
        {
            .name = "\x06_iperf\x04_tcp\x05local",
            .port = IPERF_SERVER_PORT,
        },
        {
            .name = "\x05_echo\x04_tcp\x05local",
            .port = 7,
        },
    };

    static const char *txt_records[] = {
        "product=LWIP Example",
        "version=" VERSION,
        NULL
    };

    mdns_responder_init(&netif, services, sizeof(services) / sizeof(*services),
                        txt_records);


With this configuration two services are advertised: one for an iperf server
and one for a tcp echo server. Note: the service names must be specified
similar to how they are sent on the wire. That is, each segment (where the dot
would be) is prefixed by single byte with the number of characters that
follow.

The code will also use the hostname set in LWIP for host lookups and will respond to
A record requests for the following domain names:

1. *hostname*.local
2. *mac_address*.local    (there are no colons in the mac address, just 12
                          hexadecimal characters)
3. *hostname*-*XX*.local  (where XX is the last two hexadecimal digits of the
                          mac address)
4. *hostname*-*mac_address*.local

Note: the mDNS app requires LWIP_IGMP and SO_REUSE options to be enabled in
your lwipopts config. NETIF_FLAG_IGMP must also be enabled in the interface
you are using.


apps/iperf
----------

This code provides an iperf server compatible with version 2 of the standard
iperf utility. Bi-directional and simultaneous transfers are supported. Some
examples:

    $ iperf -c lwip-38.local -r
    ------------------------------------------------------------
    Server listening on TCP port 5001
    TCP window size: 85.3 KByte (default)
    ------------------------------------------------------------
    ------------------------------------------------------------
    Client connecting to lwip-38.local, TCP port 5001
    TCP window size: 56.3 KByte (default)
    ------------------------------------------------------------
    [  5] local 172.16.1.111 port 55995 connected with 172.16.1.215 port 5001
    [ ID] Interval       Transfer     Bandwidth
    [  5]  0.0-10.0 sec   113 MBytes  94.9 Mbits/sec
    [  4] local 172.16.1.111 port 5001 connected with 172.16.1.215 port 49156
    [  4]  0.0- 9.9 sec   112 MBytes  94.6 Mbits/sec


    $ iperf -c lwip-38.local -d
    ------------------------------------------------------------
    Server listening on TCP port 5001
    TCP window size: 85.3 KByte (default)
    ------------------------------------------------------------
    ------------------------------------------------------------
    Client connecting to lwip-38.local, TCP port 5001
    TCP window size: 70.4 KByte (default)
    ------------------------------------------------------------
    [  5] local 172.16.1.111 port 55994 connected with 172.16.1.215 port 5001
    [  4] local 172.16.1.111 port 5001 connected with 172.16.1.215 port 49155
    [ ID] Interval       Transfer     Bandwidth
    [  4]  0.0- 9.9 sec  59.2 MBytes  50.2 Mbits/sec
    [  5]  0.0-10.0 sec  99.1 MBytes  83.0 Mbits/sec


apps/simple_discovery
---------------------

A very simple UDP broadcast (or multi-cast) based discovery method. This has
probably been implemented a thousand times with different slight variations. A
simple python script is included to do the discovery.


apps/tftp_server
----------------

This is an implementation of a generic TFTP server. File data is handled
through four user-implemented callback functions: open, close, read and write.


ports/stm32f2x7
---------------

The example code provided from ST was pretty weak to say the least. It wasn't
zero-copy and had no support for detecting link changes. I wrote this driver as
a replacement from scratch which does the transfer using no copies and only the
(very nicely designed) DMA engine. Link detection is also supported and the
appropriate netif functions are called when the connection changes.

The code is designed to use the Micrel KS8721 PHY. This is the PHY used on the
Olimex STM32-P207 development kit which this code was originally developed for.
However, supporting other PHY's shouldn't be overly difficult.

The driver will not likely compile as is. It uses a small amount of custom code
for registering and enabling interrupt functions which I have not included. The
user will have to replace this with equivalent functions.

To implement this driver, the stif_init function must be passed to the netif_add
function in the usual way. Then the stif_loop function should be called in the
main loop. It will return 1 if there may be more work to do (in which case
sleeping should be avoided).


examples/stm32f2x7
------------------

An example project is included which demonstrates much of the above code.
However, it requires some custom library code that I have not included so it
will not build on it's own. It should be at least useful as a reference for
the how the GPIO should be setup and how to initialize the above applications.
