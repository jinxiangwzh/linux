#include <linux/module.h>
#include <linux/i2c.h>  // struct i2c_driver
#include <linux/mod_devicetable.h>  // of_device_id
#include <linux/delay.h>  // msleep
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define AW_I2C_RETRIES              5
#define AW_I2C_RETRY_DELAY          5
#define AW_READ_CHIPID_RETRIES      5
#define AW_READ_CHIPID_RETRY_DELAY  5

#define REG_INPUT_P0        0x00
#define REG_INPUT_P1        0x01
#define REG_OUTPUT_P0       0x02
#define REG_OUTPUT_P1       0x03
#define REG_CONFIG_P0       0x04
#define REG_CONFIG_P1       0x05
#define REG_INT_P0          0x06
#define REG_INT_P1          0x07
#define REG_ID              0x10
#define REG_CTRL            0x11
#define REG_WORK_MODE_P0    0x12
#define REG_WORK_MODE_P1    0x13
#define REG_EN_BREATH       0x14
#define REG_FADE_TIME       0x15
#define REG_FULL_TIME       0x16
#define REG_DLY0_BREATH     0x17
#define REG_DLY1_BREATH     0x18
#define REG_DLY2_BREATH     0x19
#define REG_DLY3_BREATH     0x1a
#define REG_DLY4_BREATH     0x1b
#define REG_DLY5_BREATH     0x1c
#define REG_DIM00           0x20  // OUT0 port 256 steps dimming control
#define REG_DIM01           0x21
#define REG_DIM02           0x22
#define REG_DIM03           0x23  // OUT3 port 256 steps dimming control
#define REG_DIM04           0x24
#define REG_DIM05           0x25
#define REG_SWRST           0x7F

/*
 * D[1:0]-ISEL,256 dimming range option
 * 00： 0～37mA
 * 01： 0～27.75mA
 * 10： 0～18.5mA
 * 11： 0～9.25mA
*/
#define I_MAX_37MA                  0x00
#define I_MAX_27MA                  0x01
#define I_MAX_18MA                  0x02
#define I_MAX_9MA                   0x03


/* aw9106 register read/write access*/
#define REG_NONE_ACCESS                 0
#define REG_RD_ACCESS                   1 << 0
#define REG_WR_ACCESS                   1 << 1
#define AW9106_REG_MAX                  0xFF

const unsigned char aw9106_reg_access[AW9106_REG_MAX] = {
  [REG_INPUT_P0    ] = REG_RD_ACCESS,
  [REG_INPUT_P1    ] = REG_RD_ACCESS,
  [REG_OUTPUT_P0   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_OUTPUT_P1   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_CONFIG_P0   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_CONFIG_P1   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_INT_P0      ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_INT_P1      ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_ID          ] = REG_RD_ACCESS,
  [REG_CTRL        ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_WORK_MODE_P0] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_WORK_MODE_P1] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_EN_BREATH   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_FADE_TIME   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_FULL_TIME   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DLY0_BREATH ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DLY1_BREATH ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DLY2_BREATH ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DLY3_BREATH ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DLY4_BREATH ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DLY5_BREATH ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DIM00       ] = REG_WR_ACCESS,
  [REG_DIM01       ] = REG_WR_ACCESS,
  [REG_DIM02       ] = REG_WR_ACCESS,
  [REG_DIM03       ] = REG_WR_ACCESS,
  [REG_DIM04       ] = REG_WR_ACCESS,
  [REG_DIM05       ] = REG_WR_ACCESS,
  [REG_SWRST       ] = REG_WR_ACCESS,
};



#define AW9106_ID 0x23
#define LED_NUM   4   // LED的数量应该当做已知的,不然名字不知道耶没办法查找

struct led_device
{
	struct led_classdev cdev;
    struct work_struct brightness_work;

    // 实际配置这几个参数才发现，芯片并不能单独配置每个管脚,所以一定要仔细看手册再写驱动，不能想当然
    // 设置4个led节点意义也不大，一个就行
    int imax;
    int rise_time;
    int on_time;
    int fall_time;
    int off_time;
	int led_index;
};

struct aw9106 {
    struct i2c_client *i2c;
    struct device *dev;
    struct led_device led_dev[LED_NUM];
    unsigned char chipid;
};

static void aw9106_led0_brightness_work(struct work_struct *work);
static void aw9106_led1_brightness_work(struct work_struct *work);
static void aw9106_led2_brightness_work(struct work_struct *work);
static void aw9106_led3_brightness_work(struct work_struct *work);

typedef void (*brightness_work)(struct work_struct *);

struct brightness_work{
    uint8_t index;
    brightness_work work;
};

struct brightness_work work[LED_NUM] = {
    {0, aw9106_led0_brightness_work},
    {1, aw9106_led1_brightness_work},
    {2, aw9106_led2_brightness_work},
    {3, aw9106_led3_brightness_work},
};

static struct aw9106 *aw9106_dev;
/******************************************************
 *
 * aw9106 i2c write/read
 *
 ******************************************************/
static int aw9106_i2c_write(struct aw9106 *aw9106, 
                            unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(aw9106->i2c, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw9106_i2c_read(struct aw9106 *aw9106, 
        unsigned char reg_addr, unsigned char *reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_read_byte_data(aw9106->i2c, reg_addr);
        if(ret < 0) {
            pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            *reg_data = ret;
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw9106_i2c_write_bits(struct aw9106 *aw9106, 
                                unsigned char reg_addr, unsigned char reg_data, uint8_t pos)
{
    unsigned char reg_val;

    aw9106_i2c_read(aw9106, reg_addr, &reg_val);
    reg_data &= (1 << pos);  // 只有需要的位有效
    reg_val &= ~(1 << pos);  // 要设置的位置为0
    reg_val |= reg_data;
    aw9106_i2c_write(aw9106, reg_addr, reg_val);

    return 0;
}

static uint8_t get_reg_imax(uint8_t dts_val)
{
    uint8_t reg_val;

    if (dts_val <= 9.25)
    {
        reg_val = I_MAX_9MA;
    }
    else if ((dts_val <= 18.5) && (dts_val > 9.25))
    {
        reg_val = I_MAX_18MA;
    }
    else if ((dts_val <= 27.75) && (dts_val > 18.5))
    {
        reg_val = I_MAX_27MA;
    }
    else if ((dts_val <= 37) && (dts_val > 27.75))
    {
        reg_val = I_MAX_37MA;
    }
    else
    {
        printk("dts imax value is error, use default\n");
        reg_val = I_MAX_9MA;
    }
    
    return reg_val;
}

static void aw9106_led0_brightness_work(struct work_struct *work)
{
    uint8_t imax;

    aw9106_i2c_write_bits(aw9106_dev, REG_WORK_MODE_P1, 0x00, 0);   // led mode,0-3

    aw9106_i2c_write_bits(aw9106_dev, REG_EN_BREATH, 0x00, 0);      // disable breath
    
    imax = get_reg_imax(aw9106_dev->led_dev[0].imax);
    aw9106_i2c_write_bits(aw9106_dev, REG_CTRL, imax, 0);           // imax

    if(aw9106_dev->led_dev[0].cdev.brightness > aw9106_dev->led_dev[0].cdev.max_brightness) 
    {
        aw9106_dev->led_dev[0].cdev.brightness = aw9106_dev->led_dev[0].cdev.max_brightness;
    }

    aw9106_i2c_write_bits(aw9106_dev, REG_DIM00, aw9106_dev->led_dev[0].cdev.brightness, 0);                   // dimming
}

static void aw9106_led1_brightness_work(struct work_struct *work)
{
    uint8_t imax;

    aw9106_i2c_write_bits(aw9106_dev, REG_WORK_MODE_P1, 0x00, 1);   // led mode,0-3

    aw9106_i2c_write_bits(aw9106_dev, REG_EN_BREATH, 0x00, 1);      // disable breath
    
    imax = get_reg_imax(aw9106_dev->led_dev[0].imax);
    aw9106_i2c_write_bits(aw9106_dev, REG_CTRL, imax, 1);           // imax

    if(aw9106_dev->led_dev[1].cdev.brightness > aw9106_dev->led_dev[1].cdev.max_brightness) 
    {
        aw9106_dev->led_dev[1].cdev.brightness = aw9106_dev->led_dev[1].cdev.max_brightness;
    }

    aw9106_i2c_write_bits(aw9106_dev, REG_DIM01, aw9106_dev->led_dev[1].cdev.brightness, 1);                   // dimming
}

static void aw9106_led2_brightness_work(struct work_struct *work)
{
    uint8_t imax;

    aw9106_i2c_write_bits(aw9106_dev, REG_WORK_MODE_P1, 0x00, 2);   // led mode,0-3

    aw9106_i2c_write_bits(aw9106_dev, REG_EN_BREATH, 0x00, 2);      // disable breath
    
    imax = get_reg_imax(aw9106_dev->led_dev[2].imax);
    aw9106_i2c_write_bits(aw9106_dev, REG_CTRL, imax, 2);           // imax

    if(aw9106_dev->led_dev[2].cdev.brightness > aw9106_dev->led_dev[2].cdev.max_brightness) 
    {
        aw9106_dev->led_dev[2].cdev.brightness = aw9106_dev->led_dev[2].cdev.max_brightness;
    }

    aw9106_i2c_write_bits(aw9106_dev, REG_DIM02, aw9106_dev->led_dev[2].cdev.brightness, 2);                   // dimming
}

static void aw9106_led3_brightness_work(struct work_struct *work)
{
    uint8_t imax;

    aw9106_i2c_write_bits(aw9106_dev, REG_WORK_MODE_P1, 0x00, 3);   // led mode,0-3

    aw9106_i2c_write_bits(aw9106_dev, REG_EN_BREATH, 0x00, 3);      // disable breath
    
    imax = get_reg_imax(aw9106_dev->led_dev[3].imax);
    aw9106_i2c_write_bits(aw9106_dev, REG_CTRL, imax, 3);           // imax

    if(aw9106_dev->led_dev[3].cdev.brightness > aw9106_dev->led_dev[3].cdev.max_brightness) 
    {
        aw9106_dev->led_dev[3].cdev.brightness = aw9106_dev->led_dev[3].cdev.max_brightness;
    }

    aw9106_i2c_write_bits(aw9106_dev, REG_DIM03, aw9106_dev->led_dev[3].cdev.brightness, 2);                   // dimming
}


static void aw9106b_brightness_set(struct led_classdev *cdev,
                                   enum led_brightness brightness)
{
    struct led_device *led_dev = container_of(cdev, struct led_device, cdev);

    led_dev->cdev.brightness = brightness;

    schedule_work(&led_dev->brightness_work);
}

// echo 1 > blink,会打开led有渐变的效果
static void aw9106_led_blink(struct aw9106 *aw9106, unsigned char blink, uint8_t led_num)
{

    if(blink) {
        aw9106_i2c_write_bits(aw9106, REG_WORK_MODE_P1, 0x00, led_num);   // led mode

        aw9106_i2c_write(aw9106, REG_EN_BREATH, 0x3f);      // enable breath

        aw9106_i2c_write_bits(aw9106, REG_CONFIG_P1, 0x0f, led_num);      // blink mode  0-3

        aw9106_i2c_write(aw9106, REG_FADE_TIME, (aw9106->led_dev->fall_time<<3)|(aw9106->led_dev->rise_time));    // fade time
        aw9106_i2c_write(aw9106, REG_FULL_TIME, (aw9106->led_dev->off_time<<3)|(aw9106->led_dev->on_time));       // on/off time

        aw9106_i2c_write_bits(aw9106, REG_DIM00+led_num, aw9106->led_dev->cdev.brightness);                   // dimming

        aw9106_i2c_write_bits(aw9106, REG_CTRL, 0x80 | aw9106->led_dev->imax);                           // blink enable | imax
    } else {
        aw9106_i2c_write_bits(aw9106, REG_WORK_MODE_P1, 0x00);   // led mode

        aw9106_i2c_write_bits(aw9106, REG_EN_BREATH, 0x00);      // disable breath

        aw9106_i2c_write_bits(aw9106, REG_CTRL, 0x03);           // imax

        for(i=0; i<6; i++) {
            aw9106_i2c_write_bits(aw9106, REG_DIM00+i, 0x00);    // dimming
        }

    }
}


static void aw9106b_max_current(struct aw9106 *aw9106, uint8_t level)
{
    aw9106_i2c_write_bits(aw9106, REG_CTRL, 0xFC, level);
}

static int aw9106b_init(struct aw9106 *aw9106)
{
    int ret = 0;

    /*config OUT0~OUT 3 as output*/
    ret = aw9106_i2c_write(aw9106, REG_CONFIG_P1, 0x00);
    if (ret != 0)
        goto err;

	/*config OUT0~OUT 3 as high  , LED  All off*/
    ret = aw9106_i2c_write(aw9106, REG_OUTPUT_P1, 0x0F);
    if (ret != 0)
        goto err;

	// LED mode,must config to led mode, if not ,brightness cannot be set
	ret = aw9106_i2c_write(aw9106, REG_WORK_MODE_P1, 0x00);
    if (ret != 0)
        goto err;

	aw9106b_max_current(aw9106, I_MAX_9MA);

    return ret;
err:
    printk("aw9106 init fauiler,code=%d\n", ret);
    return ret;
}


static int aw9106_parse_led_cdev(struct aw9106 *aw9106,
                                 struct device_node *np)
{
    struct device_node *child;
    int ret = -1;
    uint8_t node_index = 0;
    struct led_device *led_dev = aw9106->led_dev;

    printk("aw9106 parse led\n");
    for_each_child_of_node(np, child) {
        printk("aw9106 child name=%s\n", child->name);
        if (strcmp(child->name, "battery_led0") == 0)
            node_index = 0;
        else if (strcmp(child->name, "battery_led1") == 0)
            node_index = 1;
        else if (strcmp(child->name, "battery_led2") == 0)
            node_index = 2;
        else if (strcmp(child->name, "battery_led3") == 0)
            node_index = 3;
        else
        {
            printk("aw9106 not match any\n");
            continue;  // should not here
        }
        
        // 从从设备节点child中读取属性名为"aw9106,name"
        ret = of_property_read_string(child, "aw9106,name",
            &led_dev[node_index].cdev.name);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading led name, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,imax", &led_dev[node_index].imax);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading imax, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,brightness", &led_dev[node_index].cdev.brightness);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading brightness, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,max_brightness", &led_dev[node_index].cdev.max_brightness);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading max brightness, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,rise_time", &led_dev[node_index].rise_time);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading rise_time, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,on_time", &led_dev[node_index].on_time);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading on_time, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,fall_time", &led_dev[node_index].fall_time);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading fall_time, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(child, "aw9106,off_time", &led_dev[node_index].off_time);
        if (ret < 0) {
            dev_err(aw9106->dev, "aw9106 Failure reading off_time, ret = %d\n", ret);
            goto free_pdata;
        }

        INIT_WORK(&led_dev[node_index].brightness_work, work[node_index].work);
        led_dev->cdev.brightness_set = aw9106b_brightness_set;
    }

    return 0;

free_pdata:
    return ret;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw9106_reg_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    //struct led_classdev *led_cdev = dev_get_drvdata(dev);
    //struct aw9106 *aw9106 = container_of(dev, struct aw9106, dev);

    unsigned int databuf[2] = {0, 0};

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw9106_i2c_write(aw9106_dev, (unsigned char)databuf[0], (unsigned char)databuf[1]);
    }

    return count;
}

static ssize_t aw9106_reg_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    //struct led_classdev *led_cdev = dev_get_drvdata(dev);
    //struct aw9106 *aw9106 = container_of(dev, struct aw9106, dev);
    ssize_t len = 0;
    unsigned char i = 0;
    unsigned char reg_val = 0;
    for(i = 0; i < AW9106_REG_MAX; i ++) {
        if(!(aw9106_reg_access[i]&REG_RD_ACCESS))
           continue;
        aw9106_i2c_read(aw9106_dev, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x \n", i, reg_val);
    }
    return len;
}


static ssize_t aw9106_blink_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    //struct led_classdev *led_cdev = dev_get_drvdata(dev);
    //struct aw9106 *aw9106 = container_of(dev, struct aw9106, dev);

    sscanf(buf,"%d",&databuf[0]);
    aw9106_led_blink(aw9106_dev, databuf[0]);

    return len;
}

static ssize_t aw9106_blink_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "aw9106_blink()\n");
    len += snprintf(buf+len, PAGE_SIZE-len, "echo 0 > blink\n");
    len += snprintf(buf+len, PAGE_SIZE-len, "echo 1 > blink\n");

    return len;
}


static DEVICE_ATTR(reg0, S_IWUSR | S_IRUGO, aw9106_reg_show0, aw9106_reg_store0);
static DEVICE_ATTR(blink0, S_IWUSR | S_IRUGO, aw9106_blink_show, aw9106_blink_store0);

static DEVICE_ATTR(reg1, S_IWUSR | S_IRUGO, aw9106_reg_show1, aw9106_reg_store1);
static DEVICE_ATTR(blink1, S_IWUSR | S_IRUGO, aw9106_blink_show, aw9106_blink_store1);

static DEVICE_ATTR(reg2, S_IWUSR | S_IRUGO, aw9106_reg_show2, aw9106_reg_store2);
static DEVICE_ATTR(blink2, S_IWUSR | S_IRUGO, aw9106_blink_show, aw9106_blink_store2);

static DEVICE_ATTR(reg3, S_IWUSR | S_IRUGO, aw9106_reg_show3, aw9106_reg_store3);
static DEVICE_ATTR(blink3, S_IWUSR | S_IRUGO, aw9106_blink_show, aw9106_blink_store3);


static struct attribute *aw9106_led0_attributes[] = {
    &dev_attr_reg0.attr,
    &dev_attr_blink0.attr,
    NULL
};

static struct attribute *aw9106_led1_attributes[] = {
    &dev_attr_reg1.attr,
    &dev_attr_blink1.attr,
    NULL
};

static struct attribute *aw9106_led2_attributes[] = {
    &dev_attr_reg2.attr,
    &dev_attr_blink2.attr,
    NULL
};

static struct attribute *aw9106_led3_attributes[] = {
    &dev_attr_reg3.attr,
    &dev_attr_blink3.attr,
    NULL
};

static struct attribute_group aw9106_attribute_group[] = {
    {.attrs = aw9106_led0_attributes},
    {.attrs = aw9106_led1_attributes},
    {.attrs = aw9106_led2_attributes},
    {.attrs = aw9106_led3_attributes},
};

struct of_device_id aw9106b_dt_ids[] = 
{
  {.compatible="qcom,AW9106A",},
  {},
};

static const struct i2c_device_id aw9106b_id_table[] = {
	{"aw9106-led-driver", 0},   // TODO：测试了一下这个和dts不匹配也能正常工作
	{}
};

	/* Standard driver model interfaces */
static int aw9106b_probe(struct i2c_client *i2c_cdev, const struct i2c_device_id *id)
{
    int ret;
    uint8_t i = 0;
    struct device_node *np = i2c_cdev->dev.of_node;

    //printk("aw9106b start probe,\033[1;36;60m  DATE : %s，Time:%s\r\n",__DATE__, __TIME__);

    if (!i2c_check_functionality(i2c_cdev->adapter, I2C_FUNC_I2C)) {
        dev_err(&i2c_cdev->dev, "aw9106 check_functionality failed\n");
        return -EIO;
    }

    aw9106_dev = devm_kzalloc(&i2c_cdev->dev, sizeof(struct aw9106), GFP_KERNEL);
    if (aw9106_dev == NULL)
        return -ENOMEM;

    aw9106_dev->dev = &i2c_cdev->dev;
    aw9106_dev->i2c = i2c_cdev;

    i2c_set_clientdata(i2c_cdev, aw9106_dev);

    aw9106_parse_led_cdev(aw9106_dev, np);

    ret = aw9106b_init(aw9106_dev);

    for(i = 0; i < LED_NUM; i++)
    {
        ret = led_classdev_register(aw9106_dev->dev, &aw9106_dev->led_dev[i].cdev);
        if (ret) {
            dev_err(aw9106_dev->dev, "aw9106 unable to register led ret=%d\n", ret);
            return ret;
        }

        ret = sysfs_create_group(&aw9106_dev->led_dev[i].cdev.dev->kobj, &aw9106_attribute_group);
        if (ret) {
            dev_err(aw9106_dev->dev, "aw9106 led sysfs ret: %d\n", ret);
            goto free_class;
        }
    }

    printk("aw9106 probe success\n");
    return 0;

free_class:
    led_classdev_unregister(&aw9106_dev->led_dev[0].cdev);
    led_classdev_unregister(&aw9106_dev->led_dev[1].cdev);
    led_classdev_unregister(&aw9106_dev->led_dev[2].cdev);
    led_classdev_unregister(&aw9106_dev->led_dev[3].cdev);
    return ret;
}


static int aw9106b_remove(struct i2c_client *i2c)
{
    return 0;
}

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

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("great master");
MODULE_DESCRIPTION("LEDs driver AW9106B&Salix");