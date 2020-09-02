
LED handling under Linux
========================

以最简单的形式，LED类仅允许控制来自用户空间。LED出现在/sys/class/leds/中。 最大亮度
LED的最大亮度在max_brightness文件中定义。 亮度文件将设置LED的亮度（取值为0-max_brightness）。 
大多数LED不支持硬件亮度，因此只能打开（非零亮度设置）。

该类还介绍了LED触发器的可选概念。触发器是基于内核的led事件的来源。触发器
可以是简单的，也可以是复杂的。一个简单的触发器是不可配置的，旨在以最少的
附加代码插入现有子系统。例如磁盘活动，非磁盘和Sharpsl-Charger触发器。 
禁用led触发器后，代码即可优化。

所有LED均可使用的复杂触发器，具有特定于LED的参数并根据每个LED进行工作。
以计时器触发器为例，计时器触发器会定期在LED_OFF和当前亮度设置之间更改LED
亮度。可以通过/sys/class/leds/<device>/delay_{on，off}以毫秒为单位
指定“打开”和“关闭”时间。您可以独立于计时器更改LED的亮度值触发。但是，如果
将亮度值设置为LED_OFF，它将也禁用计时器触发器。

您可以按照与IO调度程序类似的方式更改触发器已选择（通过/sys/class/leds/<设备>/trigger）。 
一旦一个给定的触发器被选择，触发特定参数就会出现在/sys/class/leds/<device>。

设计哲学
=================

基本的设计理念是简单性。LED是简单的设备，其目的是保留少量代码，以提供尽可
能多的功能。在建议增强功能时，请记住这一点。


LED设备命名
=================

当前格式为：
“设备名称：颜色：功能”

- devicename:
    它应该引用由内核创建的唯一标识符，例如 phyN用于网络设备，而inputN用于输入设备，
    而不是硬件；sysfs中提供了与产品和挂钩到给定设备的总线有关的信息，并且
    可以使用tools/leds/get_led_device_info.sh脚本从中检索该信息；通常，
    本节主要适用于以某种方式与其他设备关联的LED。
- color：
    其中一个LED_COLOR_ID_*定义在头文件`include/dt-bindings/leds/common.h`。
- function:
    其中一个LED_FUNCTION_* 定义在头文件`include/dt-bindings/leds/common.h`.

挑出一部分看看

    #define LED_FUNCTION_TX "tx"
    #define LED_FUNCTION_USB "usb"
    #define LED_FUNCTION_WAN "wan"
    #define LED_FUNCTION_WLAN "wlan"
    #define LED_FUNCTION_WPS "wps"


如果缺少所需的颜色或功能，请提交补丁到linux-leds@vger.kernel.org。

这是可能的多个LED具有相同的颜色和功能对于给定的平台来说是必需的，只与序数不同。
在本例中，最好是使用预定义的LED_FUNCTION_*在驱动程序中和后缀“-N”相连作为名称。
基于fwnode的驱动程序可以使用“函数-枚举器属性”，然后将处理连接自动由LED核心对
LED类器件进行注册。

当LED类设备是由热插拔设备的驱动程序创建的并且它没有提供唯一的devicename节时，
可能会发生名称冲突，LED子系统还提供了一个保护。在这种情况下，数字后缀(例如。
"_1"， "_2"， "_3"等)被添加到所请求的LED类设备名中。

仍然可能有使用供应商或产品名称的LED类驱动程序对于devicename，但是这种方法
现在不推荐使用，因为它不能传递信息任何附加价值。产品信息可以在sysfs的其他
地方找到(见tool/led/get_led_device_info.sh)。

正确的LED名称的例子：
  - "red:disk"
  - "white:flash"
  - "red:indicator"
  - "phy1:green:wlan"
  - "phy3::wlan"
  - ":kbd_backlight"
  - "input5::kbd_backlight"
  - "input3::numlock"
  - "input3::scrolllock"
  - "input3::capslock"
  - "mmc1::status"
  - "white:status"

可以使用get_led_device_info.sh脚本验证LED名称是否满足这里指出的要求。
它对LED类devicename部分进行验证，并在验证失败的情况下给出预期值的提示。
到目前为止，脚本支持验证led与以下类型的设备之间的关联:
        - 输入设备
        - 符合ieee80211的USB设备

该脚本对扩展开放。

已经有人呼吁将LED属性(如颜色)导出为单独的led类属性。作为一个解决方案，
它不会带来太多的开销。但是我建议让这些这些成为设备名称的一部分的命名方案
以上为进一步需要的属性留出了空间。如果部分名字不适用，那部分留空即可。


亮度设置接口
======================

LED内核子系统暴露(lù)下面的API用来设置亮度:

    - led_set_brightness : 保证不休眠，通过LED_OFF停止闪烁，
    - led_set_brightness_sync : 对于需要立即生效的用例，它可以阻塞
                                调用者访问设备寄存器所需的时间，并进行休眠，
                                传递LED_OFF会停止硬件闪烁，如果启用了软件闪烁回调，则返回-EBUSY。
                                （休眠直到设置的寄存器生效）


LED注册API
====================
想要注册为LED classdev以供其他驱动程序使用的驱动程序，用户空间需要分配
并填充led_classdev结构，然后调用[devm_]led_classdev_register。
如果使用非devm版本，驱动程序必须在释放led_classdev结构之前从其remove函数调用led_classdev_unregister。

如果驱动程序可以检测到硬件启动后的亮度变化，因此希望具有Brightness_hw_changed属性，
则必须在注册之前，在标志中设置LED_BRIGHT_HW_CHANGED标志。
在未注册LED_BRIGHT_HW_CHANGED标志的类dev上调用led_classdev_notify_brightness_hw_changed是一个错误，将触发WARN_ON。

硬件加速LEDS的闪烁
==================================

某些LED可以编程为闪烁，而无需任何CPU交互。为了支持此功能，LED驱动器可以选择实现
blink_set()函数（请参见<include/linux/leds.h>）。但是要将LED设置为闪烁，最好使用API函数led_blink_set()，因为它
必要时将检查并实施软件回调。


要关闭闪烁，请使用API函数led_brightness_set()亮度值LED_OFF，应停止
所有闪烁可能需要的软件定时器。

blink_set()函数应选择一个用户友好的闪烁值
如果使用\*delay_on == 0 && \*delay_off == 0参数调用。在这种情况下，
驱动应通过delay_on和delay_off参数将所选值返回给leds子系统。


使用Brightness_set()回调函数将亮度设置为零,应该完全关闭LED,
并取消以前编程的硬件闪烁功能（如果有）。


已知问题
============

LED触发核心不能是模块，因为简单的触发功能会引起噩梦相关性问题。
与简单触发功能带来的好处相比，我认为这是一个小问题。LED子系统的
其余部分可以是模块化的。

未来发展
==================

目前，无法专门为单个LED创建触发器。在许多情况下，触发器可能只能映射到
特定的LED（ACPI？）。LED驱动器提供的触发器的添加应涵盖此选项，并且
可以在不破坏当前接口的情况下进行添加。
