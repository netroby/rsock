[![Build Status](https://travis-ci.org/iceonsun/rsock.svg?branch=master)](https://travis-ci.org/iceonsun/rsock)
-

### 简介

rsock仅仅不是加速（加速目前由kcptun进行），也不是vpn，而是把udp流量转换成多条**伪tcp/正常udp**流量。 rsock和udp类似，传输的数据是不可靠的，也没有流控，超时重发等算法，所以目前须搭配kcptun使用或者其他有ARQ机制的udp程序使用。rsock的目的是，防止isp对udp流量的qos。目前仅支持mac（包括其他Unix）和Linux。kcptun的简介和使用见[这里](https://github.com/xtaci/kcptun)。 shadowsocks的简介见[这里](https://github.com/shadowsocks/shadowsocks-go) 。

再强调一次，rsock的传输是**不可靠的**，可靠的传输由app层负责。

下面用一张图片简要说一下原理

![](img/principle.png)

### 安装指南

64位Linux和64位Mac已经预编译好了。可以直接下载二进制。点击[这里](https://github.com/iceonsun/rsock/releases).

其他平台可以自己下载源码进行编译。rsock依赖的第三方库有：libuv, libnet, libpcap。

以ubuntu为例：

```
sudo apt-get install g++ libuv1-dev libnet libpcap #note!It's libuv1-dev
git clone https://github.com/iceonsun/rsock.git rsock
cd rsock
mkdir build && cd build
cmake .. -DRSOCK_RELEASE=1 && make
```

为了加快编译速度，make可以指定-j，后跟cpu核数。如：make -j2

注意：libuv是libuv1-dev，不是libuv-dev。那是两个不同的版本

### 快速指南

#### 服务器

注意防火墙，允许端口进入.
以64位linux为例：

```
# port=10001
# while [ $port -le 10010 ]
do
sudo ufw allow $port
port=$[ $port + 1]
done
```

表示允许客户端连接从10001到10010的端口。(**rsock服务端默认使用10001-10010，共10个端口。如果想修改默认端口范围，请见下面的参数详解。**)

`sudo ./server_rsock_Linux -d eth0 -t 127.0.0.1:9999`


参数解释:

-d eth0，外网网卡名称。
-t 127.0.0.1:9999 ，目标地址，即kcptun服务端工作的ip和端口。

#### 客户端

以mac为例：

`sudo ./client_rsock_Darwin -d en0 --taddr=x.x.x.x -l 127.0.0.1:30000`

参数解释：

-d en0，外网网卡名称。对于mac，如果连的是wifi，一般是en0。如果是有线，一般是eth1。

-t x.x.x.x，替换成rsock服务器端地址。注意，这里和服务器端不一样：无需指定端口。

-l 127.0.0.1:30000 是本地监听的udp端口。即kcptun客户端的目标地址(kcptun中-t 参数对应的地址）。

### 退出运行
I
`ps axu|grep rsock`

![](img/pid.png)

`sudo kill -SIGUSR1 pid # 其中pid是rsock运行的进程id, 图中是72294`

### 参数详解
```
	-d, --dev=[device]		外网网卡地址。如, eth0, en0, eth1。必须指定
	-t, --taddr=[addr]		目标地址。如：8.8.8.8:88, 7.7.7.7。必须指定。
	-l, --ludp=[addr]		本地监听的udp地址。仅客户端有效。客户端必须指定。
	-h, --help			显示帮助菜单. macOS暂时不可用
	-f				json配置文件
	--lcapIp=[ip]			外网ip。如果指定了这个参数，可以不指定 -d.
	--unPath			本地监听的unix域套接字地址。暂时不可用
	-p, --ports=[...]		服务器端使用的tcp/udp端口。如：10001,10010(2个） ; 10001-10010(11个）; 80,443,10001-10010(12个)。中间无空格。 默认值：10001-10010
	--duration=[timeSec]		一段duration时间内，app连接如果无数据通信，会被关闭。单位秒。默认30s
	--hash=[hashKey]		不是用来加密的。只是用来判断是否属于本程序的特征。再重复一次， 数据的加密由kcptun进行）
	--type=[tcp|udp|all]		通信的方式。可以选择tcp，udp或者all。默认是tcp。
	--daemon=[1|0]			是否以后台进程运行。1是，0否。默认是1。
	-v				verbose模式。（最好不要改变默认值，目前有个未解的bug，会造成速度慢）
	--log=[path/to/log]		日志文件的目录地址。如果没有则会新建。默认是/var/log/rsock
	--cap_timeout			libpcap抓包的超时时间。默认10ms。除非你知道是什么意思，否则不要改动。

```

### 原理

1. 服务器首先会监听一系列的端口(默认tcp，范围10001到10010，共10个端口）
2. client会正常连接上这些端口(全部连接）
3. 对于一次通信，客户端通过libnet发送数据到服务器任一端口（初始的tcp.seq 和 tcp.ack 是根据tcp三步握手抓取来的），该端口必须是已经和客户端建立好了连接。
4. 服务端通过libpcap接收数据。服务端往客户端发送数据也是一个原理。这样就完成了通信。
5. 对应用层来说，一个连接是local_ip:app_udp_port <-> server_ip:app_udp_port。如果在30s之内检测到这条连接没有数据通信，就会关闭这条连接.
6. 当client接收到rst或者fin的时候，会关闭这条真正的网络连接，重新connect服务器。再进行通讯。

#### 缺点

由于数据的收发并不是通过创建的socket来进行，而是通过libnet,libpcap，会造成每一次收到对方传来的数据，都会向peer端发送一个长度为0的包。其中ack是socket所期望的下一个对方传过来的seq。这样就会造成带宽的浪费。

### 对比

对比对象：rsock、[kcptun](https://github.com/xtaci/kcptun)

##### 服务器端测试环境

digitalocean纽约机房。1G RAM


##### 客户测试环境1：digitalocean新加坡机房。墙外<->墙外

rsock(tcp only, 2个端口)的下载速度. 稳定在700KB左右

![](img/rsock_do_sg.png)

rsock(udp only，2个端口)的下载速度。稳定在1M左右

![](img/rsock_do_sg_udp.png)

rsock(tcp and udp，各2个端口)的下载速度。稳定在900K左右

![](img/rsock_do_sg_udp_tcp.png)

rsock(tcp only, 11个端口）的下载速度. 1.25M

![](img/rsock_do_sg_11tcp.png)

rsock(udp only, 11个端口)的下载速度.  1.5M

![](img/rsock_do_sg_11udp.png)

rsock(tcp and udp, 各11个端口）的下载速度. 1.1M

![](img/rsock_do_sg_11udp_tcp.png)

kcptun. kcptun速度最快。1600KB

![](img/kcptun_do_sg.png)


#####  客户测试环境1：国内某电信宽带。100Mb下行10Mb下行。墙内<->墙外

rsock(tcp only，2个端口)的下载速度. 速度稳定在630KB左右。

![](img/rsock_telecom.png)

rsock(udp only，2个端口)的下载速度。稳定在1MB左右

![](img/rsock_udp_telcom.png)

rsock(udp and tcp，各2个端口)的下载速度. 在700KB左右

![](img/rsock_udp_tcp_telcom.png)

rsock(tcp only, 11个端口）的下载速度，稳定在1.4M左右。

![](img/rsock_11tcp_telcom.png)

rsock(udp only, 11个端口）的下载速度，稳定在1.7M左右。

![](img/rsock_11udp_telcom.png)

rsock(udp and tcp, 各11个端口）的下载速度，稳定在900K左右. 没错，我测试了2次，速度的确变小了。

![](img/rsock_11udp_tcp_telcom.png)

kcptun的下载速度. 速度在2M左右。

![](img/kcptun_telecom.png)

#### 结论
可以看到，rsock目前的速度只有kcptun 70%-90%。在看youtube 1080p快进的时候，感觉还是会比kcptun慢1s的样子. 

注意：并**不是**使用端口越多越好。主要还是受带宽影响。 经过测试，使用5个端口和10的端口的效果，差别不大。

### 注意事项

如果有的时候发现不能上网了，请检查是rsock挂掉了还是kcptun挂掉了.可以运行下面的命令来检查：

`ps axu|egrep 'kcptun|rsock'`

![](img/running.png)

强烈建议服务端kcptun和服务端rsock在后台运行。对于kcptun来说，运行：

`nohup sudo -u nobody ./server_linux_amd64 -r ":port1" -l ":port2" -mode fast2 -key aKey >/dev/null 2>&1 &`

如果都在正常运行，可以重启kcptun的客户端(turn shadowsocks off/turn，这样包括重启了kcptun）。

**rsock没有对data进行加密**，因为加密一般都在app层（kcptun）中做了。

### 其他参考

[udp2raw-tunnel](https://github.com/wangyu-/udp2raw-tunnel)

[kcptun-raw](https://github.com/Chion82/kcptun-raw)

[icmptunnel](https://github.com/DhavalKapil/icmptunnel)

### 详细讲解

**背景**

**普通**上网是这样的：

![normal]

其中第一条竖线左边是本机host。第二条竖线右边是server。中间是路由器，防火墙等。`http/tcp`表示，host和server之间的流量是tcp类型，但协议是http。

缺点：有的网站上不了，比如google。

####shadowsocks####

为了解决有的网站上不了，于是有了**shadowsocks**，shadowsocks的工作流程大概是这样的:

![shadowsocks]

其中 ss_protocol/tcp表示，流浪是tcp类型，但协议是shadowsocks协议，有可能加密过，也有可能没有加密过，且shadowsocks中data字段是上层协议(这里是http)的所有数据。这里host和server之间的流量仍然是tcp。

缺点：出海带宽不稳定，tcp容易丢包，速度慢。

####kcptun####

为了解决tcp速度慢，于是有了**kcptun**，kcptun运行着kcp协议。kcp是一个可靠且快速的协议。kcptun的流程大概如下：

![kcptun]

缺点：运营商会屏蔽udp大流量。

####rsock####

为了解决udp容易被封，于是有了**rsock**，rsock的工作流程是这样的:

![rsock]

下面，详细介绍rsock的工作流程和目的：

1. rsock_server 监听一个端口（实际是监听一个指定范围的端口，比如10000 到 10010)
2. rsock_client 正常connect到rsock_server，建立了一个正常的tcp连接(TCP三步握手完成)。建立这个tcp连接的目的后面再讲。
3. 浏览器把流量传给sslocal，sslocal根据协议加密后发送给kcptun_client，kcptun_client把数据用kcp协议处理过后再加密，然后发给rsock_client
4. rsock_client把收到的流量通过fake_tcp(伪tcp连接）发送给rsock_server。

**注意**： 这里发数据是通过libnet(一个跨平台的原始套接字库)，收数据是通过libpcap(跨平台的抓包库)，数据的收发并没有通过第2步建立的socket。

问题1：什么是faketcp？

答：faketcp不存在于操作系统中，只是rsock构造的一条虚拟连接。fake_tcp的中的任意一个数据包，是通过libnet手动构造的(手动指定的srcip:srcport:dstip:dstport)，它的格式仍然是tcp的(仅仅是seq，ack不对，其他字段都正确）。

问题2：为什么fake_tcp能通信？

答：

1. 发数据，并不是通过第2步建立的socket(没有send(socket, buf, len))，而是通过libnet手动构造的一个假tcp数据包，该数据包的所有字段都是手动输入的。其中srcip:srcport:dstip:dstport和第2步建立的tcp的对应数据一样。
1.  在路由器路由器看来，一个faketcp是数据包是一个正常的tcp包。因为路由器本身无法区分一个包是否是正常的，它只能根据本身的nat表来判断，由srcip:srcport:dstip:dstport构成的这条连接是打开的还是关闭的，如果是打开的，就放转发该数据包(fake_tcp数据包)；如果是关闭的，就丢弃该数据。由于在第2步走完了tcp三步握手，所以nat中有该连接对应的表项，所以nat判断该连接是正常的，可以转发数据。
2. 在操作系统看来，一个faketcp数据包是无效的数据包。因为该数据包的srcip:srcport:dstip:dstport和第2步建立的正常tcp连接是一样的，所以判断该数据包是发送到某个端口的数据包，但里面的seq和内核中记载的seq不匹配，所以会丢弃该数据包。根据tcp协议，系统还会向peer端发送一个ack报文，表示内核中第2步建立的tcp连接所期待的下一个包文。系统不会向peer发送RST，整个连接就不会终端
3. 收数据，是通过libpcap。libpcap能先于内核抓取到数据，抓取到数据后再发送给上层应用（这里是kcptun）


问题3：不预先建立一条tcp连接可以吗？

答：可以。通常情况下，对于peer发过来的faketcp数据包srcip:srcport:dstip:dstport构成的tcp连接，由于内核中没有该连接的记录，根据tcp协议会发送一个RST报文给peer。NAT收到该报文，发现了RST标记，就会释放掉该连接。造成该链接无法再使用。**解决办法**是，屏蔽内核固定端口的数据。linux上通过iptable，macOS上通过bpf. 内核接收不到数据，自然也不会发送rst。但libpcap始终能抓取到包。所以始终能收取到数据。


问题4：rsock数据传输是可靠的吗？

答：rsock数据传输是不可靠的，且没有流控。rsock仅仅是把kcptun发送过来的流量转发到rsock peer端。但kcptun的传输是可靠和快速的，所以在shadowsocks/browser看来数据也是可靠和快速的。

### TODO
   
1. windows 支持

1. 增加闲置模式。当没有数据通过的时候，不要一直重连服务器。

1. 尝试引入类似kcp的可靠数据传输。直接监听tcp，取消kcptun中转。

### 捐赠

非常欢迎。

比特币

11451A1Y4e8vtK3Jb7DoW8BTqj1afuWSn8

或者扫描二维码

![](img/btdonation.jpeg)

以太币

0x648419aE3D49271BB7cC31F2a61bC4c517Ea6578

或者扫描二维码

![](img/ethdonation.jpeg)

[normal]: img/normal.png
[shadowsocks]: img/shadowsocks.png
[kcptun]: img/kcptun.png
[rsock]: img/rsock.png

