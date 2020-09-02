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
  






