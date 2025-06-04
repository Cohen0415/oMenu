# 1、前言
这是一个在uboot命令行使用的设备树插件菜单。

项目地址：[https://github.com/Cohen0415/oMenu](https://github.com/Cohen0415/oMenu)

项目还在进行中，目前无法使用，待所有功能完成后，会在此文档详细介绍如何移植及使用方法。

# 2、设备树插件介绍
<font style="background-color:rgb(253, 253, 254);">设备树插件DTBO（Device Tree Blob Overlay）是一种用于Linux内核和嵌入式系统的重要机制，主要用于动态地修改或扩展系统运行时的设备树配置。</font>

<font style="background-color:rgb(253, 253, 254);">使用设备树插件的话，可以根据具体的外设来定制设备树插件，做到灵活替换。比如一个单板需要适配多种不同型号的屏幕，可以提前为每个屏幕做好一个设备树插件，用到哪个屏就动态替换对应的设备树插件。而不用换屏幕时，又去修改原始设备树文件，这样又得重新编译，重新烧录。</font>

# 3、预期功能展示
系统启动过程中，进入uboot命令行，执行omenu命令调起菜单：

```python
=> omenu
========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] Clear
[s] Save
[q] Quit
Select: 1
```

输入1后，进入i2c子菜单，选择i2c-1.dtbo插件：

```python
========== omenu/i2c ==========
[1] [ ] i2c-1.dtbo
[2] [ ] i2c-2.dtbo
[0] Back
Select: 1

========== omenu/i2c ==========
[1] [*] i2c-1.dtbo
[2] [ ] i2c-2.dtbo
[0] Back
Select: 0

========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] Clear
[s] Save
[q] Quit
Select: 3
```

输入3后，进入lcd子菜单。输入1再进入mipi子菜单，并选择mipi-3.5inch.dtbo插件。最后返回主菜单并按s保存：

```python
========== omenu/lcd ==========
[1] mipi
[2] lcd
[0] Back
Select: 1

========== omenu/lcd/mipi ==========
[1] [ ] mipi-3.5inch.dtbo
[2] [ ] mipi-7inch.dtbo
[0] Back
Select: 1

========== omenu/lcd/mipi ==========
[1] [*] mipi-3.5inch.dtbo
[2] [ ] mipi-7inch.dtbo
[0] Back
Select: 0

========== omenu/lcd ==========
[1] mipi
[2] lcd
[0] Back
Select: 0

========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] Clear
[s] Save
[q] Quit
Select: s
```

输入q退出菜单，返回uboot终端，执行reboot命令重启系统：

```python
========== omenu ==========
[1] i2c
[2] spi
[3] lcd
[4] [ ] camera-1.dtbo
[5] [ ] camera-2.dtbo
[c] Clear
[s] Save
[q] Quit
Select: q

=> reboot
```

系统重启过程中，会在uboot阶段，把刚刚选择的设备树插件覆盖到内核设备树，从而达到通过设备树插件来控制相关外设或功能的开启与关闭。

# 4、oMenu的移植
待项目结束后补充。

# 5、oMenu的使用
## 5.1、创建自定义目录
创建自定义目录结构。届时呈现的菜单结构和现在创建的目录结构是一样的。

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

## 5.2、oMenu的参数配置
进入你的uboot menuconfig。进入路径`Device Drivers -> oMenu`：

![](https://i-blog.csdnimg.cn/img_convert/c19660fe9d37f2ff06160ef8e7038ace.png)

+ `Directory Name`：目录名称。也就是上面创建的自定义目录名称。
+ `MMC Device`：mmc设备号。比如你的板卡是emmc存储方案，可以在uboot命令行执行`mmc dev`命令查看设备号。同样，如果你的板卡支持sd卡，可以选择将目录放入sd卡中。
+ `MMC Partition`：分区号。目录存储在mmc设备上的第几个分区。（目前只支持fat类型的分区格式）

至此，oMenu参数配置结束。

# 6、测试
重新编译uboot。更新uboot。

# 7、总结
项目还未结束。此文档会据最终结果再更新。
