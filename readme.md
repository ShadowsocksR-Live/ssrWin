# SSR client for Windows

This client integrates [overtls](https://github.com/ShadowsocksR-Live/overtls) and [SSRoT](https://github.com/ShadowsocksR-Live/shadowsocksr-native/wiki) functions and is simple to operate.

This software is open source software.

[Overtls](https://github.com/ShadowsocksR-Live/overtls) server installation is [here](https://github.com/ShadowsocksR-Live/overtls#server-side-one-click-installation-script)

[SSRoT](https://github.com/ShadowsocksR-Live/shadowsocksr-native/wiki) server installation tutorial is [here](https://github.com/ShadowsocksR-Live/shadowsocksr-native/wiki/%E5%85%A8%E8%87%AA%E5%8A%A8%E5%AE%89%E8%A3%85-SSRoT-%E6%9C%8D%E5%8A%A1%E5%99%A8). Sorry for it's Chinese.

[Socks-hub](https://github.com/ssrlive/socks-hub) is a converter from HTTP/HTTPS proxy to SOCKS5 proxy,
which is a bridge between `Overtls`(or `SSRoT`) and your browser.

![img](img/img01.png)

![img](img/img02.png)


# Build

You must use `git` to obtain source code.

```
git clone https://github.com/ShadowsocksR-Live/ssrWin.git
cd ssrWin

# Please be patient, the following operation takes a long time.
git submodule update --init --recursive

```
Then you can open the `ssrWin/src/ssrWin.sln` with Visual Studio 2010~2019 to build it.

# Release link

https://github.com/ShadowsocksR-Live/ssrWin/releases/latest

# Feedback

If you have any questions or suggestions, please post your [issues](https://github.com/ShadowsocksR-Live/ssrWin/issues).

