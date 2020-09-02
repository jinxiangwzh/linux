# LED Bring UP

## 一. 内核LED框架介绍：

### 1.1. 在内核中相关文件

led驱动框架代码在drivers/leds目录下，下面对相关代码先做简要介绍，详细介绍在《二：代码注解》

`led-class.c`
 - 提供了操作led设备类的一些接口,比如初始化、注册、注销、退出等。
`led-core.c`
`led-triggers.c`
'leds.h'

`leds-xxxx.c`，是由不同厂商的驱动工程师编写添加的，厂商驱动工程师结合自己公司的硬件的不同情况来对LED进行操作，使用内核提供的接口来和驱动框架进行交互，最终实现驱动的功能。

### 1.2. 使用LED框架和之前使用写的LED驱动区别（register_chrdev)

        1.2.1. LED框架中相关最终去创建一个属于/sys/class/leds这个类的一个设备。如何在这个类下有brightness      max_brightness   power           subsystem       uevent等文件来操作硬件

        1.2.2. 之前写的LED驱动通过file_operations结构体绑定相关函数来操作硬件

        1.2.3. 这两中方式是并列的。驱动开发者可以选择其中任意一种方式来开发驱动。

## 二：代码注解

### 2.1 led-class.c注解

led-class本身是作为子系统存在的,使用subsys_initcall初始化为子系统，subsys_initcall和module_init的区别是初始化所在的section不同，启动顺序也就不同。

```c
subsys_initcall(leds_init);
module_exit(leds_exit);
```

**初始化:**

```c
static int __init leds_init(void)
{
	leds_class = class_create(THIS_MODULE, "leds");
	if (IS_ERR(leds_class))
		return PTR_ERR(leds_class);
	leds_class->pm = &leds_class_dev_pm_ops;
	leds_class->dev_groups = led_groups;
	return 0;
}
```
从代码中可以看到，`leds_init`函数中使用`class_create`创建一个leds的类。初始化后后会在`/sys/class/`中增加一个名为leds的类。我们自己创建的led设备在`/sys/class/leds/`目录下