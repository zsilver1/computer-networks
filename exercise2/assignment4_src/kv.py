#!/usr/bin/python

# Copyright 2013-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from scapy.all import sniff, sendp
from scapy.all import Packet
from scapy.all import ShortField, IntField, LongField, BitField, ByteField
import struct

import networkx as nx

import sys


class KeyValue(Packet):
    name = "KeyValue"
    fields_desc = [
        LongField("preamble", 0),
        IntField("num_valid", 0),
        ByteField("port", 0),
        ByteField("mtype", 0),
        IntField("key", 0),
        IntField("value", 0),
    ]


def handle_pkt(pkt):
    pkt = str(pkt)
    preamble = pkt[:8]
    num_valid = struct.unpack("<L", pkt[8:12])[0]
    mtype = struct.unpack("<B", pkt[13:14])[0]
    key = struct.unpack(">L", pkt[14:18])[0]
    value = struct.unpack(">L", pkt[18:22])[0]
    print "TYPE: " + str(mtype) + "\nKEY: " + str(key) + \
        "\nVALUE: " + str(value)


def main():
    while (1):
        command = raw_input("\nEnter Command> ")
        command = command.split()
        if len(command) not in [2, 3]:
            print "Invalid command"
            continue
        if command[0] == "get":
            k = int(command[1])
            p = KeyValue(preamble=1, port=1, mtype=0, key=k)
            sendp(p, iface="eth0")
        elif command[0] == "put":
            k = int(command[1])
            v = int(command[2])
            p = KeyValue(preamble=1, port=1, mtype=1, key=k, value=v)
            sendp(p, iface="eth0")
        else:
            print "Invalid command"
            continue


if __name__ == '__main__':
    main()
