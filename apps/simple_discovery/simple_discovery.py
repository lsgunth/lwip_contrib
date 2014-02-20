#!/usr/bin/env python
#***************************************************************//**
#
# @file simple_discovery.py
#
# @author   Logan Gunthorpe <logang@deltatee.com>
#
# @brief    simple device discovery
#
# Copyright (c) Deltatee Enterprises Ltd. 2013
# All rights reserved.
#
#*******************************************************************/

 
# Redistribution and use in source and binary forms, with or without
# modification,are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Author: Logan Gunthorpe <logang@deltatee.com>


 
 
import socket


class SimpleDiscoverer(object):
    def __init__(self, timeout=0.5, broadcast=False):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        if broadcast:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.addr = ("<broadcast>", 9824)
        else:
            self.addr = ("224.0.0.178", 9824)

        self.sock.bind(("0.0.0.0", 9824))
        self.sock.settimeout(timeout)

    def search(self):
        self.sock.sendto("", self.addr)
        self.sock.sendto("", self.addr)
        self.sock.sendto("", self.addr)

        self.results = set()

        try:
            while 1:
                ret = self.sock.recvfrom(1024)
                if not len(ret[0]): continue

                hostname = ret[0][10:].strip("\0")
                macaddr = ":".join("%02X" % ord(x) for x in ret[0][4:10])
                res = (ret[1][0], macaddr, hostname)

                if res in self.results:
                    continue

                self.results.add(res)

                yield res

        except socket.timeout:
            return

if __name__ == "__main__":
    d = SimpleDiscoverer()
    for r in d.search():
        print "%-20s  %-20s  %s" % r
