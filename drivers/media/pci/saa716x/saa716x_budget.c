#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/mutex.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/device.h>

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <linux/i2c.h>

#include "saa716x_mod.h"

#include "saa716x_gpio_reg.h"
#include "saa716x_greg_reg.h"
#include "saa716x_msi_reg.h"

#include "saa716x_adap.h"
#include "saa716x_i2c.h"
#include "saa716x_msi.h"
#include "saa716x_budget.h"
#include "saa716x_gpio.h"
#include "saa716x_rom.h"
#include "saa716x_spi.h"
#include "saa716x_priv.h"

#include "tas2101.h"
#include "av201x.h"
#include "cx24117.h"
#include "isl6422.h"
#include "si2168.h"
#include "si2157.h"

unsigned int verbose;
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "verbose startup messages, default is 1 (yes)");

unsigned int int_type;
module_param(int_type, int, 0644);
MODULE_PARM_DESC(int_type, "force Interrupt Handler type: 0=INT-A, 1=MSI, 2=MSI-X. default INT-A mode");

#define DRIVER_NAME	"SAA716x Budget"

static int saa716x_budget_pci_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
{
	struct saa716x_dev *saa716x;
	int err = 0;

	saa716x = kzalloc(sizeof (struct saa716x_dev), GFP_KERNEL);
	if (saa716x == NULL) {
		printk(KERN_ERR "saa716x_budget_pci_probe ERROR: out of memory\n");
		err = -ENOMEM;
		goto fail0;
	}

	saa716x->verbose	= verbose;
	saa716x->int_type	= int_type;
	saa716x->pdev		= pdev;
	saa716x->module		= THIS_MODULE;
	saa716x->config		= (struct saa716x_config *) pci_id->driver_data;

	err = saa716x_pci_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x PCI Initialization failed");
		goto fail1;
	}

	err = saa716x_cgu_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x CGU Init failed");
		goto fail1;
	}

	err = saa716x_core_boot(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x Core Boot failed");
		goto fail2;
	}
	dprintk(SAA716x_DEBUG, 1, "SAA716x Core Boot Success");

	err = saa716x_msi_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x MSI Init failed");
		goto fail2;
	}

	err = saa716x_jetpack_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x Jetpack core initialization failed");
		goto fail2;
	}

	err = saa716x_i2c_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x I2C Initialization failed");
		goto fail2;
	}

	saa716x_gpio_init(saa716x);
#if 0
	err = saa716x_dump_eeprom(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x EEPROM dump failed");
	}

	err = saa716x_eeprom_data(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x EEPROM read failed");
	}

	/* set default port mapping */
	SAA716x_EPWR(GREG, GREG_VI_CTRL, 0x04080FA9);
	/* enable FGPI3 and FGPI1 for TS input from Port 2 and 6 */
	SAA716x_EPWR(GREG, GREG_FGPI_CTRL, 0x321);
#endif

	/* set default port mapping */
	SAA716x_EPWR(GREG, GREG_VI_CTRL, 0x2C688F0A);
	/* enable FGPI3, FGPI2, FGPI1 and FGPI0 for TS input from Port 2 and 6 */
	SAA716x_EPWR(GREG, GREG_FGPI_CTRL, 0x322);

	err = saa716x_dvb_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x DVB initialization failed");
		goto fail3;
	}

	return 0;

fail3:
	saa716x_dvb_exit(saa716x);
	saa716x_i2c_exit(saa716x);
fail2:
	saa716x_pci_exit(saa716x);
fail1:
	kfree(saa716x);
fail0:
	return err;
}

static void saa716x_budget_pci_remove(struct pci_dev *pdev)
{
	struct saa716x_dev *saa716x = pci_get_drvdata(pdev);

	saa716x_dvb_exit(saa716x);
	saa716x_i2c_exit(saa716x);
	saa716x_pci_exit(saa716x);
	kfree(saa716x);
}

static irqreturn_t saa716x_budget_pci_irq(int irq, void *dev_id)
{
	struct saa716x_dev *saa716x	= (struct saa716x_dev *) dev_id;

	u32 stat_h, stat_l, mask_h, mask_l;

	if (unlikely(saa716x == NULL)) {
		printk("%s: saa716x=NULL", __func__);
		return IRQ_NONE;
	}

	stat_l = SAA716x_EPRD(MSI, MSI_INT_STATUS_L);
	stat_h = SAA716x_EPRD(MSI, MSI_INT_STATUS_H);
	mask_l = SAA716x_EPRD(MSI, MSI_INT_ENA_L);
	mask_h = SAA716x_EPRD(MSI, MSI_INT_ENA_H);

	dprintk(SAA716x_DEBUG, 1, "MSI STAT L=<%02x> H=<%02x>, CTL L=<%02x> H=<%02x>",
		stat_l, stat_h, mask_l, mask_h);

	if (!((stat_l & mask_l) || (stat_h & mask_h)))
		return IRQ_NONE;

	if (stat_l)
		SAA716x_EPWR(MSI, MSI_INT_STATUS_CLR_L, stat_l);

	if (stat_h)
		SAA716x_EPWR(MSI, MSI_INT_STATUS_CLR_H, stat_h);

	saa716x_msi_event(saa716x, stat_l, stat_h);
#if 0
	dprintk(SAA716x_DEBUG, 1, "VI STAT 0=<%02x> 1=<%02x>, CTL 1=<%02x> 2=<%02x>",
		SAA716x_EPRD(VI0, INT_STATUS),
		SAA716x_EPRD(VI1, INT_STATUS),
		SAA716x_EPRD(VI0, INT_ENABLE),
		SAA716x_EPRD(VI1, INT_ENABLE));

	dprintk(SAA716x_DEBUG, 1, "FGPI STAT 0=<%02x> 1=<%02x>, CTL 1=<%02x> 2=<%02x>",
		SAA716x_EPRD(FGPI0, INT_STATUS),
		SAA716x_EPRD(FGPI1, INT_STATUS),
		SAA716x_EPRD(FGPI0, INT_ENABLE),
		SAA716x_EPRD(FGPI0, INT_ENABLE));

	dprintk(SAA716x_DEBUG, 1, "FGPI STAT 2=<%02x> 3=<%02x>, CTL 2=<%02x> 3=<%02x>",
		SAA716x_EPRD(FGPI2, INT_STATUS),
		SAA716x_EPRD(FGPI3, INT_STATUS),
		SAA716x_EPRD(FGPI2, INT_ENABLE),
		SAA716x_EPRD(FGPI3, INT_ENABLE));

	dprintk(SAA716x_DEBUG, 1, "AI STAT 0=<%02x> 1=<%02x>, CTL 0=<%02x> 1=<%02x>",
		SAA716x_EPRD(AI0, AI_STATUS),
		SAA716x_EPRD(AI1, AI_STATUS),
		SAA716x_EPRD(AI0, AI_CTL),
		SAA716x_EPRD(AI1, AI_CTL));

	dprintk(SAA716x_DEBUG, 1, "I2C STAT 0=<%02x> 1=<%02x>, CTL 0=<%02x> 1=<%02x>",
		SAA716x_EPRD(I2C_A, INT_STATUS),
		SAA716x_EPRD(I2C_B, INT_STATUS),
		SAA716x_EPRD(I2C_A, INT_ENABLE),
		SAA716x_EPRD(I2C_B, INT_ENABLE));

	dprintk(SAA716x_DEBUG, 1, "DCS STAT=<%02x>, CTL=<%02x>",
		SAA716x_EPRD(DCS, DCSC_INT_STATUS),
		SAA716x_EPRD(DCS, DCSC_INT_ENABLE));
#endif

	if (stat_l) {
		if (stat_l & MSI_INT_TAGACK_FGPI_0) {
			tasklet_schedule(&saa716x->fgpi[0].tasklet);
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_1) {
			tasklet_schedule(&saa716x->fgpi[1].tasklet);
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_2) {
			tasklet_schedule(&saa716x->fgpi[2].tasklet);
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_3) {
			tasklet_schedule(&saa716x->fgpi[3].tasklet);
		}
	}

	return IRQ_HANDLED;
}

static void demux_worker(unsigned long data)
{
	struct saa716x_fgpi_stream_port *fgpi_entry = (struct saa716x_fgpi_stream_port *)data;
	struct saa716x_dev *saa716x = fgpi_entry->saa716x;
	struct dvb_demux *demux;
	u32 fgpi_index;
	u32 i;
	u32 write_index;

	fgpi_index = fgpi_entry->dma_channel - 6;
	demux = NULL;
	for (i = 0; i < saa716x->config->adapters; i++) {
		if (saa716x->config->adap_config[i].ts_port == fgpi_index) {
			demux = &saa716x->saa716x_adap[i].demux;
			break;
		}
	}
	if (demux == NULL) {
		printk(KERN_ERR "%s: unexpected channel %u\n",
		       __func__, fgpi_entry->dma_channel);
		return;
	}

	write_index = saa716x_fgpi_get_write_index(saa716x, fgpi_index);
	if (write_index < 0)
		return;

	dprintk(SAA716x_DEBUG, 1, "dma buffer = %d", write_index);

	if (write_index == fgpi_entry->read_index) {
		printk(KERN_DEBUG "%s: called but nothing to do\n", __func__);
		return;
	}

	do {
		u8 *data = (u8 *)fgpi_entry->dma_buf[fgpi_entry->read_index].mem_virt;

		pci_dma_sync_sg_for_cpu(saa716x->pdev,
			fgpi_entry->dma_buf[fgpi_entry->read_index].sg_list,
			fgpi_entry->dma_buf[fgpi_entry->read_index].list_len,
			PCI_DMA_FROMDEVICE);

		dvb_dmx_swfilter(demux, data, 348 * 188);

		fgpi_entry->read_index = (fgpi_entry->read_index + 1) & 7;
	} while (write_index != fgpi_entry->read_index);
}

#define SAA716x_MODEL_TBS6281		"TurboSight TBS 6281"
#define SAA716x_DEV_TBS6281		"DVB-T/T2/C"

static int saa716x_tbs6281_frontend_attach(struct saa716x_adapter *adapter, int count)
{
	struct saa716x_dev *dev = adapter->saa716x;
	struct i2c_adapter *i2cadapter;
	struct i2c_client *client;
	struct i2c_board_info info;
	struct si2168_config si2168_config;
	struct si2157_config si2157_config;

	if (count > 1)
		goto err;

	/* reset */
	saa716x_gpio_set_output(dev, count ? 2 : 16);
	saa716x_gpio_write(dev, count ? 2 : 16, 0);
	msleep(50);
	saa716x_gpio_write(dev, count ? 2 : 16, 1);
	msleep(100);

	/* attach demod */
	memset(&si2168_config, 0, sizeof(si2168_config));
	si2168_config.i2c_adapter = &i2cadapter;
	si2168_config.fe = &adapter->fe;
	si2168_config.ts_mode = SI2168_TS_PARALLEL;
	memset(&info, 0, sizeof(struct i2c_board_info));
	strlcpy(info.type, "si2168", I2C_NAME_SIZE);
	info.addr = 0x64;
	info.platform_data = &si2168_config;
	request_module(info.type);
	client = i2c_new_device(&dev->i2c[1 - count].i2c_adapter, &info);
	if (client == NULL || client->dev.driver == NULL) {
		goto err;
	}
	if (!try_module_get(client->dev.driver->owner)) {
		i2c_unregister_device(client);
		goto err;
	}
	adapter->i2c_client_demod = client;

	/* attach tuner */
	memset(&si2157_config, 0, sizeof(si2157_config));
	si2157_config.fe = adapter->fe;
	memset(&info, 0, sizeof(struct i2c_board_info));
	strlcpy(info.type, "si2157", I2C_NAME_SIZE);
	info.addr = 0x60;
	info.platform_data = &si2157_config;
	request_module(info.type);
	client = i2c_new_device(i2cadapter, &info);
	if (client == NULL || client->dev.driver == NULL) {
		module_put(adapter->i2c_client_demod->dev.driver->owner);
		i2c_unregister_device(adapter->i2c_client_demod);
		goto err;
	}
	if (!try_module_get(client->dev.driver->owner)) {
		i2c_unregister_device(client);
		module_put(adapter->i2c_client_demod->dev.driver->owner);
		i2c_unregister_device(adapter->i2c_client_demod);
		goto err;
	}
	adapter->i2c_client_tuner = client;

	dev_dbg(&dev->pdev->dev, "%s frontend %d attached\n",
		dev->config->model_name, count);

	return 0;
err:
	dev_err(&dev->pdev->dev, "%s frontend %d attach failed\n",
		dev->config->model_name, count);
	return -ENODEV;
}

static struct saa716x_config saa716x_tbs6281_config = {
	.model_name		= SAA716x_MODEL_TBS6281,
	.dev_type		= SAA716x_DEV_TBS6281,
	.boot_mode		= SAA716x_EXT_BOOT,
	.adapters		= 2,
	.frontend_attach	= saa716x_tbs6281_frontend_attach,
	.irq_handler		= saa716x_budget_pci_irq,
	.i2c_rate		= SAA716x_I2C_RATE_400,
	.i2c_mode		= SAA716x_I2C_MODE_POLLING,
	.adap_config		= {
		{
			/* adapter 0 */
			.ts_port = 1, /* using FGPI 1 */
			.worker = demux_worker
		},
		{
			/* adapter 1 */
			.ts_port = 3, /* using FGPI 3 */
			.worker = demux_worker
		},
	},
};


#define SAA716x_MODEL_TBS6285		"TurboSight TBS 6285"
#define SAA716x_DEV_TBS6285		"DVB-T/T2/C"

static int saa716x_tbs6285_frontend_attach(struct saa716x_adapter *adapter, int count)
{
	struct saa716x_dev *dev = adapter->saa716x;
	struct i2c_adapter *i2cadapter;
	struct i2c_client *client;
	struct i2c_board_info info;
	struct si2168_config si2168_config;
	struct si2157_config si2157_config;

	if (count > 3)
		goto err;

	/* attach demod */
	memset(&si2168_config, 0, sizeof(si2168_config));
	si2168_config.i2c_adapter = &i2cadapter;
	si2168_config.fe = &adapter->fe;
	si2168_config.ts_mode = SI2168_TS_SERIAL;
	memset(&info, 0, sizeof(struct i2c_board_info));
	strlcpy(info.type, "si2168", I2C_NAME_SIZE);
	info.addr = ((count == 0) || (count == 2)) ? 0x64 : 0x66;
	info.platform_data = &si2168_config;
	request_module(info.type);
	client = i2c_new_device( ((count == 0) || (count == 1)) ? 
		&dev->i2c[1].i2c_adapter : &dev->i2c[0].i2c_adapter,
		&info);
	if (client == NULL || client->dev.driver == NULL) {
		goto err;
	}

	if (!try_module_get(client->dev.driver->owner)) {
		i2c_unregister_device(client);
		goto err;
	}
	adapter->i2c_client_demod = client;

	/* attach tuner */
	memset(&si2157_config, 0, sizeof(si2157_config));
	si2157_config.fe = adapter->fe;
	memset(&info, 0, sizeof(struct i2c_board_info));
	strlcpy(info.type, "si2157", I2C_NAME_SIZE);
	info.addr = ((count == 0) || (count == 2)) ? 0x62 : 0x60;
	info.platform_data = &si2157_config;
	request_module(info.type);
	client = i2c_new_device(i2cadapter, &info);
	if (client == NULL || client->dev.driver == NULL) {
		module_put(adapter->i2c_client_demod->dev.driver->owner);
		i2c_unregister_device(adapter->i2c_client_demod);
		goto err;
	}
	if (!try_module_get(client->dev.driver->owner)) {
		i2c_unregister_device(client);
		module_put(adapter->i2c_client_demod->dev.driver->owner);
		i2c_unregister_device(adapter->i2c_client_demod);
		goto err;
	}
	adapter->i2c_client_tuner = client;

	dev_dbg(&dev->pdev->dev, "%s frontend %d attached\n",
		dev->config->model_name, count);

	return 0;
err:
	dev_err(&dev->pdev->dev, "%s frontend %d attach failed\n",
		dev->config->model_name, count);
	return -ENODEV;
}

static struct saa716x_config saa716x_tbs6285_config = {
	.model_name		= SAA716x_MODEL_TBS6285,
	.dev_type		= SAA716x_DEV_TBS6285,
	.boot_mode		= SAA716x_EXT_BOOT,
	.adapters		= 4,
	.frontend_attach	= saa716x_tbs6285_frontend_attach,
	.irq_handler		= saa716x_budget_pci_irq,
	.i2c_rate		= SAA716x_I2C_RATE_400,
	.i2c_mode		= SAA716x_I2C_MODE_POLLING,
	.adap_config		= {
		{
			/* adapter 0 */
			.ts_port = 3,
			.worker = demux_worker
		},
		{
			/* adapter 1 */
			.ts_port = 2,
			.worker = demux_worker
		},
		{
			/* adapter 1 */
			.ts_port = 1,
			.worker = demux_worker
		},
		{
			/* adapter 1 */
			.ts_port = 0,
			.worker = demux_worker
		},
	},
};

#define SAA716x_MODEL_TBS6984		"TurboSight TBS 6984"
#define SAA716x_DEV_TBS6984		"DVB-S/S2"

static void saa716x_tbs6984_init(struct saa716x_dev *saa716x)
{
	int i;
	const u8 buf[] = {
		0xe0, 0x06, 0x66, 0x33, 0x65,
		0x01, 0x17, 0x06, 0xde};

#define TBS_CK 7
#define TBS_CS 8
#define TBS_DT 11

	/* send init bitstream through a bitbanged spi */
	/* set pins as output */
	saa716x_gpio_set_output(saa716x, TBS_CK);
	saa716x_gpio_set_output(saa716x, TBS_CS);
	saa716x_gpio_set_output(saa716x, TBS_DT);

	/* set all pins high */
	saa716x_gpio_write(saa716x, TBS_CK, 1);
	saa716x_gpio_write(saa716x, TBS_CS, 1);
	saa716x_gpio_write(saa716x, TBS_DT, 1);
	msleep(20);

	/* CS low */
	saa716x_gpio_write(saa716x, TBS_CS, 0);
	msleep(20);
	/* send bitstream */
	for (i = 0; i < 9 * 8; i++) {
		/* clock low */
		saa716x_gpio_write(saa716x, TBS_CK, 0);
		msleep(20);
		/* set data pin */
		saa716x_gpio_write(saa716x, TBS_DT, 
			((buf[i >> 3] >> (7 - (i & 7))) & 1));
		/* clock high */
		saa716x_gpio_write(saa716x, TBS_CK, 1);
		msleep(20);
	}
	/* raise cs, clk and data */
	saa716x_gpio_write(saa716x, TBS_CS, 1);
	saa716x_gpio_write(saa716x, TBS_CK, 1);
	saa716x_gpio_write(saa716x, TBS_DT, 1);

	/* power up LNB supply and control chips */
	saa716x_gpio_set_output(saa716x, 19);	/* a0 */
	saa716x_gpio_set_output(saa716x, 2);	/* a1 */
	saa716x_gpio_set_output(saa716x, 5);	/* a2 */
	saa716x_gpio_set_output(saa716x, 3);	/* a3 */

	/* power off */
	saa716x_gpio_write(saa716x, 19, 1); /* a0 */
	saa716x_gpio_write(saa716x, 2, 1); /* a1 */
	saa716x_gpio_write(saa716x, 5, 1); /* a2 */
	saa716x_gpio_write(saa716x, 3, 1); /* a3 */
}


static void tbs6984_lnb_pwr(struct dvb_frontend *fe, int pin, int onoff)
{
	struct i2c_adapter *adapter = cx24117_get_i2c_adapter(fe);
        struct saa716x_i2c *i2c = i2c_get_adapdata(adapter);
        struct saa716x_dev *dev = i2c->saa716x;

	/* lnb power, active low */
	if (onoff)
		saa716x_gpio_write(dev, pin , 0);
	else
		saa716x_gpio_write(dev, pin, 1);
}

void tbs6984_lnb_pwr0(struct dvb_frontend *fe, int demod, int onoff)
{
	tbs6984_lnb_pwr(fe, (demod == 0) ? 19 : 2, onoff);
}

void tbs6984_lnb_pwr1(struct dvb_frontend *fe, int demod, int onoff)
{
	tbs6984_lnb_pwr(fe, (demod == 0) ? 5 : 3, onoff);
}

static struct cx24117_config tbs6984_cx24117_cfg[] = {
	{
		.demod_address = 0x55,
		.lnb_power = tbs6984_lnb_pwr0,
	},
	{
		.demod_address = 0x05,
		.lnb_power = tbs6984_lnb_pwr1,
	},
};

static struct isl6422_config tbs6984_isl6422_cfg[] = {
	{
		.current_max		= SEC_CURRENT_570m,
		.curlim			= SEC_CURRENT_LIM_ON,
		.mod_extern		= 1,
		.addr			= 0x08,
		.id			= 0,
	},
	{
		.current_max		= SEC_CURRENT_570m,
		.curlim			= SEC_CURRENT_LIM_ON,
		.mod_extern		= 1,
		.addr			= 0x08,
		.id			= 1,
	}

};

static int saa716x_tbs6984_frontend_attach(
	struct saa716x_adapter *adapter, int count)
{
	struct saa716x_dev *dev = adapter->saa716x;
	struct saa716x_i2c *i2c = &dev->i2c[1 - (count >> 1)];

	dev_dbg(&dev->pdev->dev, "%s frontend %d attaching\n",
		dev->config->model_name, count);

	if (count > 3)
		goto err;

	if (count == 0)
		saa716x_tbs6984_init(dev);

	adapter->fe = dvb_attach(cx24117_attach, &tbs6984_cx24117_cfg[count >> 1],
			&i2c->i2c_adapter);
	if (adapter->fe == NULL)
		goto err;

	if (dvb_attach(isl6422_attach, adapter->fe, &i2c->i2c_adapter,
			&tbs6984_isl6422_cfg[count & 0x01]) == NULL)
		dev_info(&dev->pdev->dev,
			"%s frontend %d doesn't seem to have a isl6422b on the i2c bus.\n",
			dev->config->model_name, count);

	return 0;
err:
	dev_err(&dev->pdev->dev, "%s frontend %d attach failed\n",
		dev->config->model_name, count);
	return -ENODEV;
}

static struct saa716x_config saa716x_tbs6984_config = {
	.model_name		= SAA716x_MODEL_TBS6984,
	.dev_type		= SAA716x_DEV_TBS6984,
	.boot_mode		= SAA716x_EXT_BOOT,
	.adapters		= 4,
	.frontend_attach	= saa716x_tbs6984_frontend_attach,
	.irq_handler		= saa716x_budget_pci_irq,
	.i2c_rate		= SAA716x_I2C_RATE_400,
	.i2c_mode		= SAA716x_I2C_MODE_POLLING,
	.adap_config		= {
		{
			/* adapter 0 */
			.ts_port = 2,
			.worker = demux_worker
		},
		{
			/* adapter 1 */
			.ts_port = 3,
			.worker = demux_worker
		},
		{
			/* adapter 2 */
			.ts_port = 0,
			.worker = demux_worker
		},
		{
			/* adapter 3 */
			.ts_port = 1,
			.worker = demux_worker
		},
	},
};


#define SAA716x_MODEL_TBS6985 "TurboSight TBS 6985"
#define SAA716x_DEV_TBS6985   "DVB-S/S2"

static void tbs6985_reset_fe(struct dvb_frontend *fe, int reset_pin)
{
	struct i2c_adapter *adapter = tas2101_get_i2c_adapter(fe, 0);
        struct saa716x_i2c *i2c = i2c_get_adapdata(adapter);
        struct saa716x_dev *dev = i2c->saa716x;

	/* reset frontend, active low */
	saa716x_gpio_set_output(dev, reset_pin);
	saa716x_gpio_write(dev, reset_pin, 0);
	msleep(60);
	saa716x_gpio_write(dev, reset_pin, 1);
	msleep(120);
}

static void tbs6985_reset_fe0(struct dvb_frontend *fe)
{
	tbs6985_reset_fe(fe, 5);
}

static void tbs6985_reset_fe1(struct dvb_frontend *fe)
{
	tbs6985_reset_fe(fe, 2);
}

static void tbs6985_reset_fe2(struct dvb_frontend *fe)
{
	tbs6985_reset_fe(fe, 13);
}

static void tbs6985_reset_fe3(struct dvb_frontend *fe)
{
	tbs6985_reset_fe(fe, 3);
}

static void tbs6985_lnb_power(struct dvb_frontend *fe,
	int enpwr_pin, int onoff)
{
	struct i2c_adapter *adapter = tas2101_get_i2c_adapter(fe, 0);
        struct saa716x_i2c *i2c = i2c_get_adapdata(adapter);
        struct saa716x_dev *dev = i2c->saa716x;

	/* lnb power, active low */
	saa716x_gpio_set_output(dev, enpwr_pin);
	if (onoff)
		saa716x_gpio_write(dev, enpwr_pin, 0);
	else
		saa716x_gpio_write(dev, enpwr_pin, 1);
}

static void tbs6985_lnb0_power(struct dvb_frontend *fe, int onoff)
{
	tbs6985_lnb_power(fe, 27, onoff);
}

static void tbs6985_lnb1_power(struct dvb_frontend *fe, int onoff)
{
	tbs6985_lnb_power(fe, 22, onoff);
}

static void tbs6985_lnb2_power(struct dvb_frontend *fe, int onoff)
{
	tbs6985_lnb_power(fe, 19, onoff);
}

static void tbs6985_lnb3_power(struct dvb_frontend *fe, int onoff)
{
	tbs6985_lnb_power(fe, 15, onoff);
}

#undef TBS6985_TSMODE0
static struct tas2101_config tbs6985_cfg[] = {
	{
		.i2c_address   = 0x60,
		.id            = ID_TAS2101,
		.reset_demod   = tbs6985_reset_fe0,
		.lnb_power     = tbs6985_lnb0_power,
#ifdef TBS6985_TSMODE0
		.init          = {0x01, 0x32, 0x65, 0x74, 0xab, 0x98, 0x33},
#else
		.init          = {0x0b, 0x8a, 0x65, 0x74, 0xab, 0x98, 0xb1},
#endif
	},
	{
		.i2c_address   = 0x68,
		.id            = ID_TAS2101,
		.reset_demod   = tbs6985_reset_fe1,
		.lnb_power     = tbs6985_lnb1_power,
#ifdef TBS6985_TSMODE0
		.init          = {0x10, 0x32, 0x54, 0xb7, 0x86, 0x9a, 0x33},
#else
		.init          = {0x0a, 0x8b, 0x54, 0xb7, 0x86, 0x9a, 0xb1},
#endif
	},
	{
		.i2c_address   = 0x60,
		.id            = ID_TAS2101,
		.reset_demod   = tbs6985_reset_fe2,
		.lnb_power     = tbs6985_lnb2_power,
#ifdef TBS6985_TSMODE0
		.init          = {0x25, 0x36, 0x40, 0xb1, 0x87, 0x9a, 0x33},
#else
		.init          = {0xba, 0x80, 0x40, 0xb1, 0x87, 0x9a, 0xb1},
#endif
	},
	{
		.i2c_address   = 0x68,
		.id            = ID_TAS2101,
		.reset_demod   = tbs6985_reset_fe3,
		.lnb_power     = tbs6985_lnb3_power,
#ifdef TBS6985_TSMODE0
		.init          = {0x80, 0xba, 0x21, 0x53, 0x74, 0x96, 0x33},
#else
		.init          = {0xba, 0x80, 0x21, 0x53, 0x74, 0x96, 0xb1},
#endif
	}
};

static struct av201x_config tbs6985_av201x_cfg = {
	.i2c_address = 0x63,
	.id          = ID_AV2012,
	.xtal_freq   = 27000,		/* kHz */
};

static int saa716x_tbs6985_frontend_attach(struct saa716x_adapter *adapter, int count)
{
	struct saa716x_dev *dev = adapter->saa716x;

	if (count > 3)
		goto err;

	adapter->fe = dvb_attach(tas2101_attach, &tbs6985_cfg[count],
				&dev->i2c[1 - (count >> 1)].i2c_adapter);
	if (adapter->fe == NULL)
		goto err;

	if (dvb_attach(av201x_attach, adapter->fe, &tbs6985_av201x_cfg,
			tas2101_get_i2c_adapter(adapter->fe, 2)) == NULL) {
		dvb_frontend_detach(adapter->fe);
		adapter->fe = NULL;
		dev_dbg(&dev->pdev->dev,
			"%s frontend %d tuner attach failed\n",
			dev->config->model_name, count);
		goto err;
	}

	return 0;
err:
	dev_err(&dev->pdev->dev, "%s frontend %d attach failed\n",
		dev->config->model_name, count);
	return -ENODEV;
}

static struct saa716x_config saa716x_tbs6985_config = {
	.model_name		= SAA716x_MODEL_TBS6985,
	.dev_type		= SAA716x_DEV_TBS6985,
	.boot_mode		= SAA716x_EXT_BOOT,
	.adapters		= 4,
	.frontend_attach	= saa716x_tbs6985_frontend_attach,
	.irq_handler		= saa716x_budget_pci_irq,
	.i2c_rate		= SAA716x_I2C_RATE_400,
	.i2c_mode		= SAA716x_I2C_MODE_POLLING,
	.adap_config		= {
		{
			/* adapter 0 */
			.ts_port = 2,
			.worker = demux_worker
		},
		{
			/* adapter 1 */
			.ts_port = 3,
			.worker = demux_worker
		},
		{
			/* adapter 2 */
			.ts_port = 0,
			.worker = demux_worker
		},
		{
			/* adapter 3 */
			.ts_port = 1,
			.worker = demux_worker
		}
	},
};

static struct pci_device_id saa716x_budget_pci_table[] = {
	MAKE_ENTRY(TURBOSIGHT_TBS6281, TBS6281,   SAA7160, &saa716x_tbs6281_config),
	MAKE_ENTRY(TURBOSIGHT_TBS6285, TBS6285,   SAA7160, &saa716x_tbs6285_config),
	MAKE_ENTRY(TURBOSIGHT_TBS6984, TBS6984,   SAA7160, &saa716x_tbs6984_config),
	MAKE_ENTRY(TURBOSIGHT_TBS6985, TBS6985,   SAA7160, &saa716x_tbs6985_config),
	MAKE_ENTRY(TURBOSIGHT_TBS6985, TBS6985+1, SAA7160, &saa716x_tbs6985_config),
	{ }
};
MODULE_DEVICE_TABLE(pci, saa716x_budget_pci_table);

static struct pci_driver saa716x_budget_pci_driver = {
	.name			= DRIVER_NAME,
	.id_table		= saa716x_budget_pci_table,
	.probe			= saa716x_budget_pci_probe,
	.remove			= saa716x_budget_pci_remove,
};

static int __init saa716x_budget_init(void)
{
	return pci_register_driver(&saa716x_budget_pci_driver);
}

static void __exit saa716x_budget_exit(void)
{
	return pci_unregister_driver(&saa716x_budget_pci_driver);
}

module_init(saa716x_budget_init);
module_exit(saa716x_budget_exit);

MODULE_DESCRIPTION("SAA716x Budget driver");
MODULE_AUTHOR("Manu Abraham");
MODULE_LICENSE("GPL");
