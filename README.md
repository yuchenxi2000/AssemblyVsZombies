# AssemblyVsZombies

植物大战僵尸高精度键控框架

## Description

在游戏主循环函数前面注入键控代码，使得键控脚本在每一帧都被调用，从而实现真正意义上100%精确的键控。

该方法主要用于高精度键控，也可和其他脚本配合使用。

（所以有人愿意写一个Windows版本吗）

键控的实现主要依赖于PvZ游戏内部函数 void click_scene_2c3fa(PvZ*, int x, int y, int click_key);

该函数在鼠标点击场地时被调用，通过注入代码调用它可以得到手动点击相同的效果。

游戏版本：PvZ Mac v1.0.40

## API

因为考虑到较新版本clang无法编译32位程序，所以不采用dll注入方式。

所以只能手写汇编，因此api有限，只提供use_card, prejudge, until, delay, pao。

## Notice

必须要以root权限运行

建议在选完卡进入游戏界面后注入

该脚本运行完一遍后就失效了，需要再次注入。
