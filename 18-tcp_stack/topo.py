#!/usr/bin/python

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI

nworkers = 3
class MyTopo(Topo):
	def build(self):
		s1 = self.addSwitch('s1')

		hosts = list()
		for i in range(1, nworkers+2):
			h = self.addHost('h' + str(i))
			hosts.append(h)

		for h in hosts:
			self.addLink(h, s1)

topo = MyTopo()
net = Mininet(topo = topo)
h1, h2, h3, h4 = net.get('h1', 'h2', 'h3', 'h4')
h1.cmd('ifconfig h1-eth0 10.0.0.1/24')
h2.cmd('ifconfig h2-eth0 10.0.0.2/24')
h3.cmd('ifconfig h3-eth0 10.0.0.3/24')
h4.cmd('ifconfig h4-eth0 10.0.0.4/24')
for h in (h1, h2, h3, h4):
	h.cmd('scripts/disable_ipv6.sh')
	h.cmd('scripts/disable_offloading.sh')
	h.cmd('scripts/disable_tcp_rst.sh')

net.start()
CLI(net)
net.stop()
