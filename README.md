# 1、前言
这是一个在uboot命令行使用的设备树插件菜单。

项目地址：[https://github.com/Cohen0415/oMenu](https://github.com/Cohen0415/oMenu)

# 2、设备树插件介绍
<font style="background-color:rgb(253, 253, 254);">设备树插件DTBO（Device Tree Blob Overlay）是一种用于Linux内核和嵌入式系统的重要机制，主要用于动态地修改或扩展系统运行时的设备树配置。</font>

<font style="background-color:rgb(253, 253, 254);">使用设备树插件的话，可以根据具体的外设来定制设备树插件，做到灵活替换。比如一个单板需要适配多种不同型号的屏幕，可以提前为每个屏幕做好一个设备树插件，用到哪个屏就动态替换对应的设备树插件。而不用换屏幕时，又去修改原始设备树文件，这样又得重新编译，重新烧录。</font>

# 3、功能展示
系统启动过程中，进入uboot命令行，执行omenu命令调起菜单：

```shell
=> omenu
========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: 1
```

输入1，进入i2c子菜单，选择i2c-1.dtbo插件。输入0，返回上级菜单：

```shell
========== omenu/i2c ==========
[1] [ ] i2c-1.dtbo
[2] [ ] i2c-2.dtbo
[0] return to previous menu
Select: 1

========== omenu/i2c ==========
[1] [*] i2c-1.dtbo
[2] [ ] i2c-2.dtbo
[0] return to previous menu
Select: 0
```

输入s，保存已选择的插件：

```shell
========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: s
```

输入r，重启系统：

```shell
========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: r
```

系统重启过程中，会在uboot阶段，把刚刚选择的设备树插件覆盖到内核设备树，从而达到通过设备树插件来控制相关外设或功能的开启与关闭。

# 4、oMenu的移植
执行如下命令获取oMenu源码：

```shell
git clone git@github.com:Cohen0415/oMenu.git
```

```shell
oMenu
├── example									
│   └── omenu		# 自定义目录参考示例
│       ├── camera-1.dtbo
│       ├── camera-2.dtbo
│       ├── i2c
│       ├── lcd
│       ├── list.txt
│       └── spi
├── README.md
└── u-boot
    └── drivers
        └── omenu	# oMenu源码
```

复制`oMenu/u-boot/drivers/omenu`目录到你uboot源码的相应位置：

```shell
cp -r oMenu/u-boot/drivers/omenu <your_uboot>/drivers/ 
```

修改`<your_uboot>/drivers/Makefile`，在合适位置添加如下内容，使得可以编译oMenu：

```shell
obj-$(CONFIG_OMENU) += omenu/
```

![](https://i-blog.csdnimg.cn/img_convert/a0ac402843334b5bb93b96a03d1c3799.png)

修改`<your_uboot>/drivers/Kconfig`，在合适位置添加如下内容：

```shell
source "drivers/omenu/Kconfig"
```

![](https://i-blog.csdnimg.cn/img_convert/aedca8c0cb634f85b3630ce74ac1d7ee.png)

修改`<your_uboot>/common/main.c`，在该文件头部添加如下内容：

```shell
#ifdef CONFIG_OMENU
extern void omenu_fdt_apply(void);
#endif
```

![](https://i-blog.csdnimg.cn/img_convert/a9eda2f97649d7ad366f00f3a5daf187.png)

继续在该文件的`main_loop()`函数中添加如下内容：

```shell
#ifdef CONFIG_OMENU
	omenu_fdt_apply();
#endif
```

![](https://i-blog.csdnimg.cn/img_convert/77f909c5ad2d878c6d80fe94b9cf681c.png)

至此，oMenu移植结束。

# 5、oMenu的使用
## 5.1、创建自定义目录
创建自定义目录结构。届时呈现的菜单结构会和现在创建的目录结构是一样的。

如下所示创建一个omenu目录，里面按功能类别又创建了子目录，并按类别放入设备树插件：

```shell
cohen@ubuntu2004:~$ tree omenu/
omenu/
├── camera-1.dtbo
├── camera-2.dtbo
├── i2c
│   ├── i2c-1.dtbo
│   ├── i2c-2.dtbo
│   └── list.txt
├── lcd
│   ├── list.txt
│   ├── mipi
│   │   ├── list.txt
│   │   ├── mipi-3.5inch.dtbo
│   │   └── mipi-7inch.dtbo
│   └── rgb
│       ├── list.txt
│       └── rgb-10.1inch.dtbo
├── list.txt
└── spi
    ├── list.txt
    ├── spi-1.dtbo
    └── spi-2.dtbo
```

每个目录里必须包含一个list.txt文件，它们记录了本目录里的所有子目录或设备树插件的名称。因为程序中是靠list.txt里的内容来实现菜单解析的。

如`omenu/list.txt`内容如下：

```shell
i2c
spi
lcd
camera-1.dtbo
camera-2.dtbo
```

如`omenu/i2c/list.txt`内容如下：

```shell
i2c-1.dtbo
i2c-2.dtbo
```

菜单解析时会忽略带有`'#'`注释的目录或插件。如下所示，在`omenu/list.txt`中注释i2c和camera-1.dtbo，届时将不会出现i2c和camera-1.dtbo的选项：

```shell
#i2c
spi
lcd
#camera-1.dtbo
camera-2.dtbo
```

该示例目录已同步上传，可自行查看：

![](https://i-blog.csdnimg.cn/img_convert/847af08badaa61da2ee492a44f8f3c55.png)

## 5.2、oMenu的参数配置
进入你的uboot menuconfig。进入路径`Device Drivers`，开启oMenu选项：

![](https://i-blog.csdnimg.cn/img_convert/a9610a68a187a38c02de9e0160234856.png)

进入路径`oMenu`：

![](https://i-blog.csdnimg.cn/direct/eb650eb29fd549639b3f754ebacfa5d1.png)

+ `Storage Type`：存储介质类型。目前支持u盘和mmc。
+ `Storage Device Number`：u盘或mmc的设备号。比如你的板卡是emmc存储方案，可以在uboot命令行执行`mmc dev`命令查看设备号。若使用u盘，可执行`usb start`，再执行`usb dev`查看设备号。
+ `Storage Partition Number`：分区号。目录存储在设备上的第几个分区。（目前只支持fat32类型的分区格式）
+ `Directory Name`：目录名称。也就是上面创建的自定义目录名称。
+ `Default log level`：日志的默认等级。数字越大，等级越高。oMenu会输出小于等于默认等级的所有日志。即当默认等级设置为3时，所有日志将会输出。（在初次使用时，可以先设置成3，方便查看所有调试信息）

至此，oMenu参数配置结束。

至此，oMenu参数配置结束。

# 6、编译uboot
当你移植好oMenu，且设置好相关配置后。可以开始重新编译uboot，并更新uboot到你的板卡。

# 7、oMenu菜单的使用介绍
## 7.1、功能选项介绍
在uboot命令行，执行`omenu`命令进入菜单：

```shell
=> omenu
========== omenu ==========
[1] xxxxx
[2] xxxxx
[3] xxxxx
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
```

`[c]`：清除已选择的插件。（比如多选/错选了，可以直接重新来过）

`[s]`：保存已选择的插件。选完插件后，一定要保存后再退出，否则是不会记录你的选择的。如果想清除所有已选插件，也是先执行`c`清除，后执行`s`保存清除后的结果。

`[r]`：重启系统。该选项和在uboot命令行执行`reboot`是一样的。每次选完插件后，必须重启才会生效。因为设备树插件的覆盖是在uboot阶段执行的。

`[q]`：退出oMenu菜单，返回uboot命令行。

## 7.2、实际场景使用演示
这是一块rk3568开发板，登入系统后，执行`ifconfig -a`命令，可以看到有两个can节点：

![](https://i-blog.csdnimg.cn/img_convert/fcca4ae471a320516cfa95e29fc9ae0e.png)

重启系统，进入uboot命令行，执行`omenu`命令，选择`can0-disabled.dtbo`：

```shell
=> omenu
========== omenu ==========
[1] [ ] can0-disabled.dtbo
[2] [ ] can1-disabled.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: 1

========== omenu ==========
[1] [*] can0-disabled.dtbo
[2] [ ] can1-disabled.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: s
writing selected.txt
FAT: Misaligned buffer address (000000007bedfdd0)
oMenu [INFO] : Saved 1 selections to selected.txt

========== omenu ==========
[1] [*] can0-disabled.dtbo
[2] [ ] can1-disabled.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select:	r
```

重启后，再次执行`ifconfig -a`命令，可以看到此时只剩下1个can节点：

![](https://i-blog.csdnimg.cn/img_convert/9b7463dbaded44db3d2ed200b05fecfa.png)

继续把另一个can节点也关闭。重新进入omenu菜单，选择`can1-disabled.dtbo`：

```shell
=> omenu
========== omenu ==========
[1] [*] can0-disabled.dtbo
[2] [ ] can1-disabled.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: 2

========== omenu ==========
[1] [*] can0-disabled.dtbo
[2] [*] can1-disabled.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: s
writing selected.txt
FAT: Misaligned buffer address (000000007bedfe30)
oMenu [INFO] : Saved 2 selections to selected.txt

========== omenu ==========
[1] [*] can0-disabled.dtbo
[2] [*] can1-disabled.dtbo
[c] clear:  clear selections and uncheck all plugins
[s] save:   save current selections to selected.txt
[r] reboot: restart the system without saving changes
[q] quit:   quit the menu without saving changes
Select: r
```

重启后，再次执行`ifconfig -a`命令，可以看到2个can节点均被关闭：

![](https://i-blog.csdnimg.cn/img_convert/30d0b89f2b6232c42897faf0416f398d.png)

# 8、其它

关于设备树插件dtbo的生成和应用可以参考：https://blog.csdn.net/CATTLE_L/article/details/139498061

# 9、问题交流
若在移植使用过程中遇到问题，欢迎提issues。或联系：

本人qq：1033878279

