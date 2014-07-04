/* ~.~ *-c-*
 *
 * Copyright (c) 2013, John Lee <furious_tauren@163.com>
 * Fri Jun 27 10:46:33 CST 2014
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
#include <i2c.h>
#include <tps65910.h>


int tps65910_init(void)
{
	i2c_init(&__i2c_adap[0]);

	return 0;
}

static void __outb(unsigned char reg, unsigned char val)
{
	unsigned char buf[2] = { reg, val };
	struct i2c_msg msg = {
		.addr	= TPS65910_CTRL_I2C_ADDR,
		.flags	= 0,
		.buf	= buf,
		.len	= 2,
	};

	i2c_transfer(&__i2c_adap[0], &msg, 1);
}

static unsigned char __inb(unsigned char reg)
{
	unsigned char buf;
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= TPS65910_CTRL_I2C_ADDR,
			.flags	= 0,
			.buf	= &reg,
			.len	= 1,
		},
		[1] = {
			.addr	= TPS65910_CTRL_I2C_ADDR,
			.flags	= I2C_M_RD,
			.buf	= &buf,
			.len	= 1,
		},
	};

	i2c_transfer(&__i2c_adap[0], msg, 2);
	return buf;
}

void tps65910_set_i2c_control(void)
{
	unsigned char val;

	/* VDD1/2 voltage selection valister access by control i/f */
	val = __inb(TPS65910_DEVCTRL_REG);
	val |= TPS65910_DEVCTRL_REG_SR_CTL_I2C_SEL_CTL_I2C;
	__outb(TPS65910_DEVCTRL_REG, val);
}

/*
 * Voltage switching for MPU frequency switching.
 * @module:		       mpu - 0, core - 1
 * @vddx_op_vol_sel:	       vdd voltage to set
 */
void tps65910_voltage_update(u32 mod, u8 vddx_op_vol_sel)
{
	unsigned char reg, val;

	if (mod == MPU)
		reg = TPS65910_VDD1_OP_REG;
	else
		reg = TPS65910_VDD2_OP_REG;

	/* Select VDDx OP   */
	val = __inb(reg);
	val &= ~TPS65910_OP_REG_CMD_MASK;
	__outb(reg, val);

	/* Configure VDDx OP  Voltage */
	val = __inb(reg);
	val &= ~TPS65910_OP_REG_SEL_MASK;
	val |= vddx_op_vol_sel;
	__outb(reg, val);
}
