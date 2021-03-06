/*
 * Sunxi A31 Power Management Unit
 *
 * (C) Copyright 2013 Oliver Schinagl <oliver@schinagl.nl>
 * http://linux-sunxi.org
 *
 * Based on sun6i sources and earlier U-Boot Allwiner A10 SPL work
 *
 * (C) Copyright 2006-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Berg Xing <bergxing@allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gpio.h>
#include <asm/arch/p2wi.h>
#include <asm/arch/prcm.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

void p2wi_init(void)
{
	struct sunxi_p2wi_reg *p2wi = (struct sunxi_p2wi_reg *)SUNXI_P2WI_BASE;

	/* Enable p2wi and PIO clk, and de-assert their resets */
	prcm_init_apb0();

	sunxi_gpio_set_cfgpin(SUNXI_GPL(0), SUNXI_GPL0_R_P2WI_SCK);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(1), SUNXI_GPL1_R_P2WI_SDA);

	/* Reset p2wi controller and set clock to CLKIN(12)/8 = 1.5 MHz */
	writel(P2WI_CTRL_RESET, &p2wi->ctrl);
	sdelay(0x100);
	writel(P2WI_CC_SDA_OUT_DELAY(1) | P2WI_CC_CLK_DIV(8),
	       &p2wi->cc);
}

int p2wi_set_pmu_address(u8 slave_addr, u8 ctrl_reg, u8 init_data)
{
	struct sunxi_p2wi_reg *p2wi = (struct sunxi_p2wi_reg *)SUNXI_P2WI_BASE;
	int i;

	writel(P2WI_PM_DEV_ADDR(slave_addr) |
	       P2WI_PM_CTRL_ADDR(ctrl_reg) |
	       P2WI_PM_INIT_DATA(init_data) |
	       P2WI_PM_INIT_SEND,
	       &p2wi->pm);
	for (i = 0xffffff; i != 0; i--)
		if (!(readl(&p2wi->pm) & P2WI_PM_INIT_SEND))
			break;
	if (readl(&p2wi->pm) & P2WI_PM_INIT_SEND)
		return -EFAULT;

	return 0;
}

int p2wi_read(const u8 addr, u8 *data)
{
	struct sunxi_p2wi_reg *p2wi = (struct sunxi_p2wi_reg *)SUNXI_P2WI_BASE;
	int i, ret = 0;
	u8 reg;

	writel(P2WI_DATADDR_BYTE_1(addr), &p2wi->dataddr0);
	writel(P2WI_DATA_NUM_BYTES(1) |
	       P2WI_DATA_NUM_BYTES_READ, &p2wi->numbytes);
	writel(P2WI_STAT_TRANS_DONE, &p2wi->status);
	writel(P2WI_CTRL_TRANS_START, &p2wi->ctrl);

	for (i = 0xffffff; i != 0; i--) {
	        reg = readl(&p2wi->status);
	        if (reg & P2WI_STAT_TRANS_ERR) {
	                ret = -EIO;
	                break;
                }
		if (reg & P2WI_STAT_TRANS_DONE)
			break;
        }

	if (i == 0)
		ret = -ETIME;

	*data = readl(&p2wi->data0) & P2WI_DATA_BYTE_1_MASK;
	writel(reg, &p2wi->status); /* Clear status bits */
	return ret;
}

int p2wi_write(const u8 addr, u8 data)
{
	struct sunxi_p2wi_reg *p2wi = (struct sunxi_p2wi_reg *)SUNXI_P2WI_BASE;
	int i, ret = 0;
	u8 reg;

	writel(P2WI_DATADDR_BYTE_1(addr), &p2wi->dataddr0);
	writel(P2WI_DATA_BYTE_1(data), &p2wi->data0);
	writel(P2WI_DATA_NUM_BYTES(1), &p2wi->numbytes);
	writel(P2WI_STAT_TRANS_DONE, &p2wi->status);
	writel(P2WI_CTRL_TRANS_START, &p2wi->ctrl);

	for (i = 0xffffff; i != 0; i--) {
	        reg = readl(&p2wi->status);
	        if (reg & P2WI_STAT_TRANS_ERR) {
	                ret = -EIO;
	                break;
                }
		if (reg & P2WI_STAT_TRANS_DONE)
			break;
        }

	if (i == 0)
		ret = -ETIME;

	writel(reg, &p2wi->status); /* Clear status bits */
	return ret;
}
