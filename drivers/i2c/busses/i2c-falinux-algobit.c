/*
 * drivers/i2c/busses/i2c-falinux-algobit.c
 *
 *  Copy( XScale IXP420 )  and Modify
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <linux/falinux-common.h>


#ifdef gpio_direction_output
static void __bit_setscl(void *data, int val)
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->scl_pin;

	if (val)
		gpio_direction_input( gpnr );
	else 
		gpio_direction_output( gpnr, val );
}
static void __bit_setsda(void *data, int val)
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->sda_pin;

	if (val)
		gpio_direction_input( gpnr );
	else 
		gpio_direction_output( gpnr, val );
}
static int __bit_getscl(void *data )
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->scl_pin;
	
	gpio_direction_input( gpnr );
	return (0 == gpio_get_value( gpnr )) ? 0:1;
}
static int __bit_getsda(void *data )
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->sda_pin;

	gpio_direction_input( gpnr );
	return (0 == gpio_get_value( gpnr )) ? 0:1;
}
#endif


static int falinux_i2c_remove(struct platform_device *plat_dev)
{
	int idx, devcnt;
	struct falinux_i2c_platform_data *plf_data = plat_dev->dev.platform_data;
	struct falinux_i2c_drv_data      *drv_data, *svd;
	
	drv_data = svd = platform_get_drvdata(plat_dev);
	devcnt   = plf_data->devcnt;
	
	for (idx=0; idx<devcnt; idx++ )
	{	
		i2c_del_adapter(&svd->adapter);
		svd ++;
	}

	platform_set_drvdata(plat_dev, NULL);
	kfree(drv_data);
	
	return 0;
}

static int falinux_i2c_probe(struct platform_device *plat_dev)
{
	int err, idx, devcnt;
	char   buf_name[256];
	
	struct falinux_i2c_platform_data *plf_data;
	struct falinux_i2c_drv_data      *drv_data;
	
	plf_data = plat_dev->dev.platform_data;
	devcnt   = plf_data->devcnt;
	drv_data = kzalloc(sizeof(struct falinux_i2c_drv_data)*devcnt, GFP_KERNEL);
	
	platform_set_drvdata(plat_dev, drv_data);
	
	for ( idx=0; idx<devcnt; idx++ )
	{
		drv_data->info = plf_data;

		drv_data->algo_data.data 	= plf_data;
		drv_data->algo_data.udelay 	= plf_data->udelay;
		drv_data->algo_data.timeout = plf_data->timeout;
		
#ifdef gpio_direction_output		
        drv_data->algo_data.setsda = __bit_setsda;
        drv_data->algo_data.setscl = __bit_setscl;
        drv_data->algo_data.getsda = __bit_getsda;
        drv_data->algo_data.getscl = __bit_getscl;
#endif        
		if (plf_data->bit_setsda) drv_data->algo_data.setsda = plf_data->bit_setsda;
		if (plf_data->bit_setscl) drv_data->algo_data.setscl = plf_data->bit_setscl;
		if (plf_data->bit_getsda) drv_data->algo_data.getsda = plf_data->bit_getsda;
		if (plf_data->bit_getscl) drv_data->algo_data.getscl = plf_data->bit_getscl;

		drv_data->adapter.id 		= plf_data->id;
		drv_data->adapter.class 	= I2C_CLASS_HWMON;
		drv_data->adapter.algo_data = &drv_data->algo_data;
		drv_data->adapter.dev.parent= &plat_dev->dev;
		sprintf( buf_name, "%s:%d", plat_dev->dev.driver->name, idx ); 	// 이름을 수정한다.
		strlcpy(drv_data->adapter.name, buf_name, I2C_NAME_SIZE);
	
		// default SCL=1, SDA=1
		plf_data->bit_setsda( (void *)plf_data, 1 );
		plf_data->bit_setscl( (void *)plf_data, 1 );
	
		err = i2c_bit_add_bus(&drv_data->adapter);
		if (err) 
		{
			printk(KERN_ERR "ERROR: Could not install %s (%s)\n", plat_dev->dev.bus_id, buf_name );
			return err;
		}
	
		printk("add i2c-bus %s\n", drv_data->adapter.name );
		
		plf_data ++;
		drv_data ++;
	}
		
	return 0;
}

static struct platform_driver falinux_i2c_driver = {
	.probe		= falinux_i2c_probe,
	.remove		= falinux_i2c_remove,
	.driver		= {
		.name	= "i2c-bit",
		.owner	= THIS_MODULE,
	},
};

static int __init falinux_i2c_init(void)
{
	return platform_driver_register(&falinux_i2c_driver);
}

static void __exit falinux_i2c_exit(void)
{
	platform_driver_unregister(&falinux_i2c_driver);
}

module_init(falinux_i2c_init);
module_exit(falinux_i2c_exit);

MODULE_DESCRIPTION("GPIO-based I2C adapter for falinux systems");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("JaeKyong,Oh <freefrug@falinux.com>");

