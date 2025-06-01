目的开发一个通用的、可高效移植的、方便使用的，基于uboot的设备树插件菜单。持续开发中...

目标功能演示，在uboot命令行执行omenu命令，调起设备树插件菜单：

```shell
=> omenu
========== /omenu ==========
[1] i2c
[2] lcd
[3] spi
[s] Save
[q] Quit
Select: 1

========== /omenu/i2c ==========
[1] [ ] i2c1.dtbo
[2] [ ] i2c2.dtbo
[0] Back
Select: 1

========== /omenu/i2c ==========
[1] [*] i2c1.dtbo
[2] [ ] i2c2.dtbo
[0] Back
Select: 0

========== /omenu ==========
[1] i2c
[2] lcd
[3] spi
[s] Save
[q] Quit
Select: 2

========== /omenu/lcd ==========
[1] [ ] lcd-3.5inch.dtbo
[2] [ ] lcd-7inch.dtbo
[0] Back
Select: 1

========== /omenu/lcd ==========
[1] [*] lcd-3.5inch.dtbo
[2] [ ] lcd-7inch.dtbo
[0] Back
Select: 0

========== /omenu ==========
[1] i2c
[2] lcd
[3] spi
[s] Save
[q] Quit
Select: 3

========== /omenu/spi ==========
[1] [ ] spi1.dtbo
[2] [ ] spi2.dtbo
[0] Back
Select: 2

========== /omenu/spi ==========
[1] [ ] spi1.dtbo
[2] [*] spi2.dtbo
[0] Back
Select: 0

========== /omenu ==========
[1] i2c
[2] lcd
[3] spi
[s] Save
[q] Quit
Select:

...

```
