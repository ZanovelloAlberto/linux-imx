#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>

#define SPI_CLASS_NAME "ABE LCD"
#define ABE_LCD_NAME "abe-lcd-spi"
#define ABE_LCD_MAX_DEVICES (1)
#define  ABE_LCD_USER_BUF_LENGTH (32)

static int SPI_WriteComm(unsigned char data);
static int SPI_WriteData(unsigned short data);
static int setup_lcd(struct spi_device *spi);

static dev_t abe_lcd_dev_t;
static struct device *abe_lcd_dev;
static struct cdev *abe_lcd_cdev;
static struct class *abe_lcd_class;
static struct spi_device *abe_lcd_spi;
struct gpio_desc  *wr_gpio;
struct gpio_desc  *cs_gpio;
struct gpio_desc  *reset_gpio;
struct fb_info *fb_info = NULL;



void set_fb_info_abe_lcd(struct fb_info *ex_fb_info)
{
	fb_info=ex_fb_info;
}

EXPORT_SYMBOL_GPL(set_fb_info_abe_lcd);

// (1) finding a match from full device-tree (with vendor part)
static const struct of_device_id abe_lcd_of_match[] = {
    {
      .compatible = "abe,abe_lcd",
      .data = (void *) 0,
    },
    { }
};
MODULE_DEVICE_TABLE(of, abe_lcd_of_match);

// (2) finding a match from stripped device-tree (no vendor part)
static const struct spi_device_id abe_lcd_id[] = {
    { "abe_lcd", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, abe_lcd_id);

static int abe_lcd_open(struct inode *inode, struct file *filp)
{
    printk("SPI_slave::abe_lcd_open called.\n");
    return 0;
}

static int abe_lcd_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t abe_lcd_read(struct file *filp, char __user * buf, size_t lbuf, loff_t * ppos)
{
	int i;
    printk("setup LCD.\n");
    gpiod_set_value_cansleep(reset_gpio, 0);
	for(i=0;i<10;i++) udelay(1000); //Delay 10ms
    /* The minimum delay between power supplies and reset rising can be 0 */
    gpiod_set_value_cansleep(reset_gpio, 1);
	for(i=0;i<10;i++) udelay(1000); //Delay 10
	setup_lcd(abe_lcd_spi);
    return 0;
}

static ssize_t abe_lcd_write(struct file *filep, const char __user * buf, size_t lbuf, loff_t * ppos)
{
    printk("mySPI_slave::abe_lcd_write called.\n");
	SPI_WriteComm(0x21);
    return lbuf;
}

static const struct file_operations abe_lcd_fops = {
    .owner =    THIS_MODULE,
    .write =    abe_lcd_write,
    .read =        abe_lcd_read,
    .open =        abe_lcd_open,
    .release =    abe_lcd_release
};


static int SPI_WriteComm(unsigned char data)
{
    gpiod_set_value(wr_gpio, 0);
	udelay(2);
    gpiod_set_value(cs_gpio, 0);
	udelay(1);
	//spi_write_then_read(abe_lcd_spi, &data, sizeof(data), NULL, 0);
	spi_write(abe_lcd_spi, &data, 1);
	udelay(2);
    gpiod_set_value(cs_gpio, 1);
	udelay(1);
	return 0;
}

static int SPI_WriteData(unsigned short data)
{
    gpiod_set_value(wr_gpio, 1);
	udelay(2);
    gpiod_set_value(cs_gpio, 0);
	udelay(1);
	//spi_write_then_read(abe_lcd_spi, &data, sizeof(data), NULL, 0);
	spi_write(abe_lcd_spi, &data, 1);
	udelay(2);
    gpiod_set_value(cs_gpio, 1);
	udelay(1);
	return 0;
}



#define ROTATION_0    0
#define ROTATION_90   1
#define ROTATION_180  2
#define ROTATION_270  3
static void SetRotation(uint8_t Rotation)
{
	uint8_t madctl=0;
	switch (Rotation)
	{
		case 0: //0
			madctl = 0x00;
		break;

		case 1: //90
			madctl = 0x60;
		break;

		case 2: //180
			madctl = 0xC0;
		break;

		case 3: //270
			madctl = 0xB0;
		break;
	}

	SPI_WriteComm(0x36);
	SPI_WriteData(madctl);
}



static int setup_lcd(struct spi_device *spi)
{
	int i;

	SPI_WriteComm(0x01);
	for(i=0;i<1200;i++) udelay(1000); //Delay 120ms
	//------------------------------display and color format setting--------------------------------//

	SPI_WriteComm(0xb0);
	SPI_WriteData(0x11);

	SPI_WriteComm(0xb1);
	SPI_WriteData(0x40);

	SPI_WriteComm(0x36);
	SPI_WriteData(0xB0);

	SPI_WriteComm(0x2A);
	SPI_WriteData(0x00);
	SPI_WriteData(0x00);
	SPI_WriteData(0x01);
	SPI_WriteData(0x3F);
	
	SPI_WriteComm(0x2B);
	SPI_WriteData(0x00);
	SPI_WriteData(0x00);
	SPI_WriteData(0x00);
	SPI_WriteData(0xEF);
	
	SPI_WriteComm(0x2C);
	
	SPI_WriteComm(0x3a);
	SPI_WriteData(0x66);

	//--------------------------------ST7789V Frame rate setting----------------------------------//
	SPI_WriteComm(0xb2);
	SPI_WriteData(0x0c);
	SPI_WriteData(0x0c);
	SPI_WriteData(0x00);
	SPI_WriteData(0x33);
	SPI_WriteData(0x33);
	SPI_WriteComm(0xb7);
	SPI_WriteData(0x35);
	//---------------------------------ST7789V Power setting--------------------------------------//
	SPI_WriteComm(0xbb);
	SPI_WriteData(0x20);
	SPI_WriteComm(0xc0);
	SPI_WriteData(0x2c);
	SPI_WriteComm(0xc2);
	SPI_WriteData(0x01);
	SPI_WriteComm(0xc3);
	SPI_WriteData(0x0b);
	SPI_WriteComm(0xc4);
	SPI_WriteData(0x20);
	SPI_WriteComm(0xc6);
	SPI_WriteData(0x0f);
	SPI_WriteComm(0xd0);
	SPI_WriteData(0xa4);
	SPI_WriteData(0xa1);
	//--------------------------------ST7789V gamma setting---------------------------------------//
	SPI_WriteComm(0xe0);
	SPI_WriteData(0xd0);
	SPI_WriteData(0x00);
	SPI_WriteData(0x03);
	SPI_WriteData(0x08);
	SPI_WriteData(0x0a);
	SPI_WriteData(0x17);
	SPI_WriteData(0x2e);
	SPI_WriteData(0x44);
	SPI_WriteData(0x3f);
	SPI_WriteData(0x29);
	SPI_WriteData(0x10);
	SPI_WriteData(0x0e);
	SPI_WriteData(0x14);
	SPI_WriteData(0x18);
	SPI_WriteComm(0xe1);
	SPI_WriteData(0xd0);
	SPI_WriteData(0x00);
	SPI_WriteData(0x03);
	SPI_WriteData(0x08);
	SPI_WriteData(0x07);
	SPI_WriteData(0x27);
	SPI_WriteData(0x2b);
	SPI_WriteData(0x44);
	SPI_WriteData(0x41);
	SPI_WriteData(0x3c);
	SPI_WriteData(0x1b);
	SPI_WriteData(0x1d);
	SPI_WriteData(0x14);
	SPI_WriteData(0x18);

	SPI_WriteComm(0xb0);
	SPI_WriteData(0x11);


	SPI_WriteComm(0x21);
	SPI_WriteComm(0x11);
	for(i=0;i<200;i++) udelay(1000); //Delay 200ms

	SPI_WriteComm(0x29);
	SPI_WriteComm(0x2C);
	

	return 0;
}


// Initialize SPI interface...
static int abe_lcd_probe(struct spi_device *spi)
{
    int err;
    const struct of_device_id *match;
    int devData = 0;

    struct device *dev = &spi->dev;
	int i;


	if(fb_info==NULL) return -EPROBE_DEFER;

	console_lock();
	fb_blank(fb_info, FB_BLANK_NORMAL);
	console_unlock();


    printk("mySPI_slave::abe_lcd_probe called.\n");

    // check and read data from of_device_id...
    match = of_match_device(abe_lcd_of_match, &spi->dev);
    if(!match) {
        printk("mySPI_slave::abe_lcd_probe drvice not found in device tree...\n");
    }
    else {
        devData = (int)match->data;
        printk("mySPI_slave::abe_lcd_probe data is: %d\n", devData);
    }

    spi->bits_per_word = 8;
    spi->mode = (0);

    err = spi_setup(spi);
    if (err < 0) {
        printk("mySPI_slave::abe_lcd_probe spi_setup failed!\n");
        return err;
    }

    reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(reset_gpio)) {
            dev_err(dev, "Failed to get reset-gpios\n");
            return -EINVAL;
    }

    wr_gpio = devm_gpiod_get(dev, "wr", GPIOD_OUT_LOW);
    if (IS_ERR(wr_gpio)) {
            dev_err(dev, "Failed to get wr-gpios\n");
            return -EINVAL;
    }

    cs_gpio = devm_gpiod_get(dev, "cs", GPIOD_OUT_LOW);
    if (IS_ERR(cs_gpio)) {
            dev_err(dev, "Failed to get cs-gpios\n");
            return -EINVAL;
    }
    gpiod_set_value(cs_gpio, 1);

    gpiod_set_value_cansleep(reset_gpio, 0);
	for(i=0;i<200;i++) udelay(1000); //Delay 10ms
    /* The minimum delay between power supplies and reset rising can be 0 */
    gpiod_set_value_cansleep(reset_gpio, 1);
	for(i=0;i<200;i++) udelay(1000); //Delay 10ms

    abe_lcd_spi = spi;
    err = setup_lcd(spi);


    if (err < 0) {
        printk("mySPI_slave::abe_lcd_probe spi_sync_transfer failed!\n");
        return err;
    }

    // define a device class
    abe_lcd_class = class_create(THIS_MODULE, SPI_CLASS_NAME);
    if (abe_lcd_class == NULL) {
        printk("mySPI_slave::abe_lcd_probe class_create failed!\n");
        return -1;
    }

    // create char device entry in sysfs...
    if (devData == 0) {
        err = alloc_chrdev_region(&abe_lcd_dev_t, 0, ABE_LCD_MAX_DEVICES, ABE_LCD_NAME);
    } else {

    }
    if (err < 0) {
        printk("mySPI_slave::abe_lcd_probe alloc_chrdev_region failed!\n");
        class_destroy(abe_lcd_class);
        return err;
    }

    abe_lcd_cdev = cdev_alloc();
    if (!(abe_lcd_cdev)) {
        printk("mySPI_slave::abe_lcd_probe cdev_alloc failed!\n");
        unregister_chrdev_region(abe_lcd_dev_t, ABE_LCD_MAX_DEVICES);
        class_destroy(abe_lcd_class);
        return -1;
    }

    cdev_init(abe_lcd_cdev, &abe_lcd_fops);

    err = cdev_add(abe_lcd_cdev, abe_lcd_dev_t, ABE_LCD_MAX_DEVICES);
    if(err < 0) {
        printk("mySPI_slave::abe_lcd_probe cdev_add failed!\n");
        cdev_del(abe_lcd_cdev);
        unregister_chrdev_region(abe_lcd_dev_t, ABE_LCD_MAX_DEVICES);
        class_destroy(abe_lcd_class);
        return err;
    }

    if (devData == 0) {
        abe_lcd_dev = device_create(abe_lcd_class, NULL, abe_lcd_dev_t, NULL, "%s", ABE_LCD_NAME);
    } else {

    }

	console_lock();
	fb_blank(fb_info, FB_BLANK_UNBLANK);
	console_unlock();

    return 0;
}

static int abe_lcd_remove(struct spi_device *spi)
{
    printk("abe_lcd_remove() called.\n");
    device_destroy(abe_lcd_class, abe_lcd_dev_t);
    if(abe_lcd_cdev) {
        cdev_del(abe_lcd_cdev);
    }
    unregister_chrdev_region(abe_lcd_dev_t, ABE_LCD_MAX_DEVICES);
    class_destroy(abe_lcd_class);
    return 0;
}

static struct spi_driver abe_lcd_spi_driver = {
    .driver = {
        .owner =    THIS_MODULE,
        .name =        "abe_lcd_spi",
        .of_match_table = of_match_ptr(abe_lcd_of_match),
    },
    .id_table =    abe_lcd_id,
    .probe =    abe_lcd_probe,
    .remove =    abe_lcd_remove
};
module_spi_driver(abe_lcd_spi_driver);
