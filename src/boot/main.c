/* ~.~ *-c-*
 *
 * Copyright (c) 2013, John Lee <furious_tauren@163.com>
 * Wed Jun 11 17:58:47 CST 2014
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/regs.h>
#include <asm/io.h>
#include <delay.h>
#include <mux.h>
#include <ddr.h>
#include <gpmc.h>
#include <clock.h>
#include <sys_info.h>
#include <tps65910.h>
#include <nand.h>

static const struct ddr_data ddr3_evm_data = {
	.datardsratio0	= MT41J512M8RH125_RD_DQS,
	.datawdsratio0	= MT41J512M8RH125_WR_DQS,
	.datafwsratio0	= MT41J512M8RH125_PHY_FIFO_WE,
	.datawrsratio0	= MT41J512M8RH125_PHY_WR_DATA,
};

static const struct cmd_control ddr3_evm_cmd_ctrl_data = {
	.cmd0csratio	= MT41J512M8RH125_RATIO,
	.cmd0iclkout	= MT41J512M8RH125_INVERT_CLKOUT,

	.cmd1csratio	= MT41J512M8RH125_RATIO,
	.cmd1iclkout	= MT41J512M8RH125_INVERT_CLKOUT,

	.cmd2csratio	= MT41J512M8RH125_RATIO,
	.cmd2iclkout	= MT41J512M8RH125_INVERT_CLKOUT,
};

static struct emif_regs ddr3_evm_emif_reg_data = {
	.sdram_config	= MT41J512M8RH125_EMIF_SDCFG,
	.ref_ctrl	= MT41J512M8RH125_EMIF_SDREF,
	.sdram_tim1	= MT41J512M8RH125_EMIF_TIM1,
	.sdram_tim2	= MT41J512M8RH125_EMIF_TIM2,
	.sdram_tim3	= MT41J512M8RH125_EMIF_TIM3,
	.zq_config	= MT41J512M8RH125_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41J512M8RH125_EMIF_READ_LATENCY
					| PHY_EN_DYN_PWRDN,
};

static struct dpll_params dpll_mpu_opp100 = {
	550, OSC - 1, {1, -1, -1, -1, -1 }
};
static const struct dpll_params dpll_core_opp100 = {
	1000, OSC - 1, {-1, -1, 10, 8, 4 }
};

extern struct nand_chip __nand_chip;

extern int omap_nand_init(struct nand_chip *chip);
extern int nand_load(struct nand_chip *, u32, u32, void *);

static void watchdog_disable(void)
{
	void *wdt_base = (void *)WDT_BASE;

	writel(0xaaaa, wdt_base + WDTREG_WSPR);
	while (readl(wdt_base + WDTREG_WWPS) != 0x0)
		;

	writel(0x5555, wdt_base + WDTREG_WSPR);
	while (readl(wdt_base + WDTREG_WWPS) != 0x0)
		;
}

/* Modify it according to your board's configuration */
static void board_pin_mux(void)
{
	configure_module_pin_mux(rgmii1_pin_mux);
	configure_module_pin_mux(mmc0_pin_mux);
	configure_module_pin_mux(i2c0_pin_mux);
	configure_module_pin_mux(i2c1_pin_mux);
	configure_module_pin_mux(nand_pin_mux);
}


static void voltage_update(void)
{
	int mpu_vdd;

	/* Get the frequency */
	dpll_mpu_opp100.m = get_efuse_mpu_max_freq();
	mpu_vdd = get_tps65910_mpu_vdd(dpll_mpu_opp100.m);

	/* tell the tps65910 to use i2c */
	tps65910_set_i2c_control();

	/* first update mpu voltage. */
	tps65910_voltage_update(MPU, mpu_vdd);

	/* second, update the core voltage. */
	tps65910_voltage_update(CORE, TPS65910_OP_REG_SEL_1_1_3);

	/* set core frequencies to opp100 */
	do_setup_dpll(&dpll_core_regs, &dpll_core_opp100);

	/* set mpu frequency */
	do_setup_dpll(&dpll_mpu_regs, &dpll_mpu_opp100);
}

void lowlevel_init(void)
{
	struct nand_chip *chip = &__nand_chip;

	/*s_init*/
	watchdog_disable();
	timer_init();
	prcm_init();
	board_pin_mux();

	config_ddr(303, MT41J512M8RH125_IOCTRL_VALUE,
			&ddr3_evm_data,
			&ddr3_evm_cmd_ctrl_data,
			&ddr3_evm_emif_reg_data);
	/*s_init end*/

	/* spl_board_init */
	gpmc_init(0);
	tps65910_init();
	voltage_update();
	/* spl_board_init end */

	omap_nand_init(chip);


#define IMG_OFFS	(512 * 1024)	/* the 3rd block */
#define IMG_SIZE	(32 * 1024)	/* 32KiB */
#define LOADADDR	((void *)0x80200000)
	nand_load(chip, IMG_OFFS, IMG_SIZE, LOADADDR);

	int (*jump_2_OS1)(void) = LOADADDR;
	(*jump_2_OS1)();
}

