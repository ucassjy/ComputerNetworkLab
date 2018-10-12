#!/usr/bin/python

import time

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI

def clearIP(n):
    for iface in n.intfList():
        n.cmd('ifconfig %s 0.0.0.0' % (iface))

class RingTopo(Topo):
    def build(self):
        b1 = self.addHost('b1')
        b2 = self.addHost('b2')
        b3 = self.addHost('b3')
        b4 = self.addHost('b4')

        self.addLink(b1, b3)
        self.addLink(b2, b3)
        self.addLink(b2, b4)
        self.addLink(b3, b4)

if __name__ == '__main__':
    topo = RingTopo()
    net = Mininet(topo = topo, controller = None) 

    nports = [ 1, 2, 3, 2 ]

    for idx in range(len(nports)):
        name = 'b' + str(idx+1)
        node = net.get(name)
        clearIP(node)
        node.cmd('./disable_offloading.sh')
        node.cmd('./disable_ipv6.sh')

        # set mac address for each interface
        for port in range(nports[idx]):
            intf = '%s-eth%d' % (name, port)
            mac = '00:00:00:00:0%d:0%d' % (idx+1, port+1)

            node.setMAC(mac, intf = intf)

        # start b2, b3, b4
        if name != 'b1':
            node.cmd('./stp > %s-output.txt 2>&1 &' % name)

    net.start()

    # start b1
    time.sleep(10)
    net.get('b1').cmd('./stp > b1-output.txt 2>&1 &')

    CLI(net)
    net.stop()
