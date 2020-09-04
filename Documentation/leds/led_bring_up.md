# LED Bring UP

## 一. 内核LED框架介绍：

内核中驱动部分维护者针对每个种类的驱动设计一套成熟的、标准的、典型的驱动实现，并把不同厂家的同类硬件驱动中相同的部分抽出来自己实现好，再把不同部分留出接口给具体的驱动开发工程师来实现，这就叫驱动框架。即标准化的驱动实现,统一管理系统资源,维护系统稳定。

所有led共性:
 有和用户通信的设备节点
 亮和灭
不同点:
 有的led可能是接在gpio管脚上,不同的led有不同的gpio来控制
 有的led可能由其他的芯片来控制(节约cpu的pin,或者为了控制led的电流等)
 可以设置亮度
 可以闪烁
所以Linux中led子系统驱动框架把把所有led的共性给实现了,把不同的地方留给驱动工程师去做.

### 1.1. 在内核中相关文件

led驱动框架代码在drivers/leds目录下，下面对相关代码先做简要介绍，详细介绍在《二：代码注解》

**核心代码**
`led-class.c`
 - 提供了操作led设备类的一些接口,比如初始化、注册、注销、退出等。

`led-core.c`
`led-triggers.c`
`leds.h`

**可选功能代码：**
触发功能：
`driver/leds/led-triggers.c`
`driver/leds/trigger/led-triggers.c`
`driver/leds/trigger/ledtrig-oneshot.c`
`driver/leds/trigger/ledtrig-timer.c`
`driver/leds/trigger/ledtrig-heartbeat.c`

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
- 1.创建类
  从代码中可以看到，`leds_init`函数中使用`class_create`创建一个leds的类。初始化后后会在`/sys/class/`中增加一个名为leds的类。我们自己创建的led设备在`/sys/class/leds/`目录下/.

- 2.初始化操作接口
  leds_class_dev_pm_ops包含电源管理的操作函数，通过SIMPLE_DEV_PM_OPS定义的一个结构体。
  使用电源管理是要使能宏CONFIG_PM_SLEEP。但是我们的代码`leds_class->pm = &leds_class_dev_pm_ops;`并没有使用宏包起来。
  看代码是如何定义的

  ```c
	#define SIMPLE_DEV_PM_OPS(name, suspend_fn, resume_fn) \
	const struct dev_pm_ops __maybe_unused name = { \
		SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn) \
	}
  ```
  当没有使能宏的时候定义如下
  #define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn)，这个时候leds_class_dev_pm_ops就为{}。

  下面我们研究一下suspend和resume的机制。
  先看代码：
  ```c
	static int led_suspend(struct device *dev)
	{
		struct led_classdev *led_cdev = dev_get_drvdata(dev);

		if (led_cdev->flags & LED_CORE_SUSPENDRESUME)
			led_classdev_suspend(led_cdev);

		return 0;
	}

	static int led_resume(struct device *dev)
	{
		struct led_classdev *led_cdev = dev_get_drvdata(dev);

		if (led_cdev->flags & LED_CORE_SUSPENDRESUME)
			led_classdev_resume(led_cdev);

		return 0;
	}
  ```
  从代码中可以看到在设置了suspend和resume的回调函数之后并不是就会调用,要先判断标志位flags是不是包含LED_CORE_SUSPENDRESUME。
  **所以如果我们写的led驱动要想被电源管理(PM)框架管理起来,需要在驱动中设置标志LED_CORE_SUSPENDRESUME**
  示例：
  ```C	
  static struct led_classdev hp6xx_red_led = {
		.name			    = "hp6xx:red",
		.default_trigger	= "hp6xx-charge",
		.brightness_set		= hp6xxled_red_set,
		.flags			    = LED_CORE_SUSPENDRESUME,
	};
  ```

  再回到刚才的代码继续往下看，判断标志位之后会调用`led_classdev_suspend`和`led_classdev_resume`两个函数，贴一下代码
  ```c
	void led_classdev_suspend(struct led_classdev *led_cdev)
	{
		led_cdev->flags |= LED_SUSPENDED;
		led_set_brightness_nopm(led_cdev, 0);
		flush_work(&led_cdev->set_brightness_work);
	}

	void led_classdev_resume(struct led_classdev *led_cdev)
	{
		led_set_brightness_nopm(led_cdev, led_cdev->brightness);

		if (led_cdev->flash_resume)
			led_cdev->flash_resume(led_cdev);

		led_cdev->flags &= ~LED_SUSPENDED;
	}
  ```
   
	两个函数中都有`led_set_brightness_nopm`这个函数，我们先看看这个是用来干嘛的。

   ```C
	void led_set_brightness_nopm(struct led_classdev *led_cdev,
			      enum led_brightness value)
	{
		/* Use brightness_set op if available, it is guaranteed not to sleep */
		if (!__led_set_brightness(led_cdev, value))
			return;

		/* If brightness setting can sleep, delegate it to a work queue task */
		led_cdev->delayed_set_value = value;
		schedule_work(&led_cdev->set_brightness_work);
	}
  ```
    `__led_set_brightness`函数跟进去其实就是调用我们自己实现的set_brightness，如果我们没有实现，这个函数返回一个负数.**对负数取非为0**，所以这里没有实现set_brightness的时候会继续往下执行。

	delayed_set_value就是把要设置的值暂时保存起来,然后在work队列可能会调用。

	`flush_work`是内核接口，这里先不做研究，功能就是等待队列里的任务执行完。

	resume还比较简单,只要把实现的resume函数赋给flash_resume即可。

- 3.属性
  
  属性是通过led_groups这个结构体添加的,进一步看，分别到了`dev_attr_brightness`、`dev_attr_max_brightness`和`bin_attr_trigger`。
  然而搜索这几个变量却搜索不到。网上看代码发现`dev_attr_brightness`是通过
  `static DEVICE_ATTR_RW(brightness);`定义，`dev_attr_max_brightness`是通过`static DEVICE_ATTR_RO(max_brightness);`定义，`bin_attr_trigger`是通过`static BIN_ATTR(trigger, 0644, led_trigger_read, led_trigger_write, 0);`定义。
  下面对这些宏进行展开
  ```c
  static DEVICE_ATTR_RW(brightness) 
  ----> 
  #define DEVICE_ATTR_RW(_name)   \
          struct device_attribute dev_attr_##_name = __ATTR_RW(_name)
  ---->
  struct device_attribute dev_attr_brightness = __ATTR_RW(brightness)
  ---->右边继续展开
  __ATTR(brightness, 0644, brightness##_show, brightness##_store)
  ---->右边继续展开
  {
  .attr = {.name = __stringify(_brightness),				\
		 .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },		\
  .show	= _show,						\
  .store	= _store,
  }		
  ---->
  {
  .attr = {.name = brightness,				\
	       .mode = VERIFY_OCTAL_PERMISSIONS(0644) },		\
  .show	= brightness_show,						\
  .store	= brightness_store,
  }
  ```
  show数据可以使用cat读出来,store可以使用echo写入

  展开就到这，回到这个结构体
  ```c
  static struct attribute *led_class_attrs[] = {
	&dev_attr_brightness.attr,
	&dev_attr_max_brightness.attr,
	NULL,
  };
  ```
  led_class_attrs结构体数组中的数据就上上述展开的
  ```c
  .attr = {.name = brightness,				\
	       .mode = VERIFY_OCTAL_PERMISSIONS(0644) },
  ```

  >写代码show就是把读到的数据填充到接口的buf里，store就是把数据写入
**注册**

注册接口有两个，

```c
/**
 * led_classdev_register - 注册一个新的led类对象
 * @parent: 驱动LED的控制设备
 * @led_cdev: the led_classdev structure for this device
 *
 * Register a new object of LED class, with name derived from the name property
 * of passed led_cdev argument.
 *
 * Returns: 0 on success or negative error value on failure
 */
static inline int led_classdev_register(struct device *parent,
					struct led_classdev *led_cdev)
{
	return led_classdev_register_ext(parent, led_cdev, NULL);
}
```

也可以在注册的时候初始化led数据

```c
int led_classdev_register_ext(struct device *parent,
			      struct led_classdev *led_cdev,
			      struct led_init_data *init_data)
```
示例:
```c
	init_data.fwnode = flash->indicator_node;
	init_data.devicename = "as3645a";
	init_data.default_label = "indicator";

	rval = led_classdev_register_ext(&flash->client->dev, iled_cdev,
					 &init_data);
```

注册的代码比较长，就不贴出来了。
完成的主要功能如下：
- 1.根据初始据使用led_compose_name接口形成一个名字（没有初始数据就用led_cdev中给定的名字）。
- 2.判断是否已经有这个名字的led设备
- 3.把led设备注册到sysfs中
- 4.led链表中的一些处理

当我们使用led驱动框架去编写驱动的时候，这个led_classdev_register函数的作用类似于我们之前使用file_operations方式去注册字符设备驱动时的register_chrdev函数。

**注销**

```c
void led_classdev_unregister(struct led_classdev *led_cdev)
```

**退出**

```c
class_destroy(leds_class);
```
销毁注册的led类对象

**其他**

设置led闪烁
```c
void led_blink_set(struct led_classdev *led_cdev,
		   unsigned long *delay_on,
		   unsigned long *delay_off)

```

设置led闪烁一次
```c
void led_blink_set_oneshot(struct led_classdev *led_cdev,
			   unsigned long *delay_on,
			   unsigned long *delay_off,
			   int invert)
```
停止led闪烁
```c
void led_stop_software_blink(struct led_classdev *led_cdev)
```
## 三：示例

**1.led最简单示例**

代码路径:/deivers/leds/led-simple.c

>说明：led连接在aw9106b芯片上,通过IIC控制led。

1.在drivers/leds目录下创建一个led_test.c的文件

写led驱动，首先它是一个module，而module需要实现init和exit。所以我们先实现这两个函数,

```c
static int __init aw9106b_driver_init(void)
{
	
}
module_init(aw9106b_driver_init);

static void __exit aw9106b_driver_exit(void)
{
	
}
module_exit(aw9106b_driver_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LEDs driver AW9106B&Salix");
```
上面说了我们其实是控制I2C设备来间接控制led.控制I2C module有更简单的实现方式，上面的接口我们改为

```c
#include <linux/module.h>
#include <linux/i2c.h>

static struct i2c_driver aw9106b_i2c_driver = {
	.driver = {
		.name = "aw9106b-leds",
		.of_match_table = aw9106b_dt_ids,
	},
	.probe = aw9106b_probe,
	.remove = aw9106b_remove,
	.id_table= aw9106b_id_table,
};

module_i2c_driver(aw9106b_i2c_driver);
```

> 为啥要实现这几个成员，因为是最基本的，大多数驱动实现这个几个就好，感兴趣额的可以查看struct i2c_driver结构体。或者查看`drivers/i2c/i2c_bring_up.md`。

使用module_i2c_driver实现了i2c_add_driver和i2c_del_driver，即module_init和module_exit.
下面我们就分别实现结构体中的内容。

`struct i2c_driver`”继承于“`struct device_driver`，`.driver`就是父类的成员，这个driver成员中比较重要的就是名字和`of_match_table`

```c
struct of_device_id aw9106b_dt_ids[] = 
{
  {.compatible="qcom,AW9106A",},
  {}，  //TODO：不知道为啥看到的所有代码几乎都加一个空的
};
```

这里的compatible对应的字符串很重要,系统要根据这个字符串匹配dts文件。dts文件怎么修改下面再说。

接着实现probe和remove，进入`12c.h`查看这两个类型。
```c
/* Standard driver model interfaces */
int (*probe)(struct i2c_client *, const struct i2c_device_id *);
int (*remove)(struct i2c_client *);
```
从注释可以看出，一个标准的module至少要实现这两个接口。

>probe何时被调用：在总线上驱动和设备的名字匹配，就会调用驱动的probe函数

对于probe的功能主要是做一些资源分配、注册、初始化等



## 四：总结



