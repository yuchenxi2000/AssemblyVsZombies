# AssemblyVsZombies

植物大战僵尸高精度键控框架

## 说在前面（2020.7.12）

这个只是一个demo，而且针对pvz 1.0.40版本（mac版）。

所以，

* 不能在Windows上使用（mac专用）

* 功能非常简陋。实现是直接写汇编然后拷贝到pvz游戏程序的内存空间。如果使用注入动态库的方式，那么可以直接写c++，那么就可以利用标准库的函数，而且可以对键控做更多的控制。

鉴于没有多少人玩mac版，而且macOS 10.15开始放弃了对32位程序的支持（pvz是32位程序），意味着从此以后不能直接在mac新系统上运行pvz。

所以，该项目永久废弃。作为替代，我和向量wlc一起开发了Windows版的键控框架avz。项目地址：

https://github.com/vector-wlc/AsmVsZombies

该项目现由 https://github.com/vector-wlc/ 维护。实现是动态库注入，而不是该项目的实现方法。

## Description

在游戏主循环函数前面注入键控代码，使得键控脚本在每一帧都被调用，从而实现真正意义上100%精确的键控。

该方法主要用于高精度键控，也可和其他脚本配合使用。（比如 https://github.com/yuchenxi2000/CppVsZombies-Mac ）

（所以有人愿意写一个Windows版本吗）

键控的实现主要依赖于PvZ游戏内部函数 void click_scene_2c3fa(PvZ*, int x, int y, int click_key);

该函数在鼠标点击场地时被调用，通过注入代码调用它可以得到手动点击相同的效果。

代码注入基于自己写的代码注入框架 https://github.com/yuchenxi2000/mac-code-inject

游戏版本：PvZ Mac v1.0.40

## API

因为考虑到较新版本clang无法编译32位程序，所以不采用dll注入方式。

所以只能手写汇编，因此api有限，只提供use_card, prejudge, until, delay, pao。

## Notice

该项目需要asmjit库：https://github.com/asmjit/asmjit

请设置好header/library path

必须要以root权限运行

建议在选完卡进入游戏界面后注入

该脚本运行完一遍后就失效了，需要再次注入。
