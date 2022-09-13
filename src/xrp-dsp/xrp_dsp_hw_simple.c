/*
 * Copyright (c) 2016 - 2017 Cadence Design Systems Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <xtensa/hal.h>
#include <xtensa/xtruntime.h>

#include "xrp_debug.h"
#include "xrp_dsp_hw.h"
#include "xrp_dsp_interrupt.h"
#include "xrp_dsp_sync.h"
#include "xrp_types.h"
#include "xrp_hw_simple_dsp_interface.h"
#include "xrp_rb_file.h"

static uint32_t mmio_base;
static char panic_buffer[4*1024];
static volatile struct xrp_hw_panic *panic = (struct xrp_hw_panic *)panic_buffer;

#define device_mmio(off) ((volatile void *)mmio_base + off)
#define host_mmio(off) ((volatile void *)mmio_base + off)
#define BIT(n)   (0x1u<<n)
enum xrp_irq_mode {
	XRP_IRQ_NONE,
	XRP_IRQ_LEVEL,
	XRP_IRQ_EDGE,
};
static enum xrp_irq_mode host_irq_mode;
static enum xrp_irq_mode device_irq_mode;

static uint32_t device_irq_offset;
static uint32_t device_irq_bit;
static uint32_t device_irq;

static uint32_t host_irq_offset;
static uint32_t host_irq_bit;
void hang(void) __attribute__((noreturn));

#if XCHAL_HAVE_INTERRUPTS
void xrp_irq_handler(void)
{
//	pr_debug("%s\n", __func__);
	if (device_irq_mode == XRP_IRQ_LEVEL)
		xrp_s32ri(BIT(device_irq_bit), device_mmio(device_irq_offset));
}
#endif

void xrp_hw_send_host_irq(void)
{
	switch (host_irq_mode) {
	case XRP_IRQ_EDGE:
		xrp_s32ri(0, host_mmio(host_irq_offset));
		/* fall through */
	case XRP_IRQ_LEVEL:
		xrp_s32ri(BIT(host_irq_bit), host_mmio(host_irq_offset));
		break;
	default:
		break;
	}
}

void xrp_hw_wait_device_irq(void)
{
#if XCHAL_HAVE_INTERRUPTS
	unsigned intstate;

	if (device_irq_mode == XRP_IRQ_NONE)
		return;

	pr_debug("%s: waiting for device IRQ...\n", __func__);
#if XCHAL_HAVE_XEA3
	intstate = xthal_disable_interrupts();
#else
	intstate = XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS - 1);
#endif
	xrp_interrupt_enable(device_irq);
	XT_WAITI(0);
	xrp_interrupt_disable(device_irq);
#if XCHAL_HAVE_XEA3
	xthal_restore_interrupts(intstate);
#else
	XTOS_RESTORE_INTLEVEL(intstate);
#endif
#endif
}

void xrp_hw_set_sync_data(void *p)
{
	static const enum xrp_irq_mode irq_mode[] = {
		[XRP_DSP_SYNC_IRQ_MODE_NONE] = XRP_IRQ_NONE,
		[XRP_DSP_SYNC_IRQ_MODE_LEVEL] = XRP_IRQ_LEVEL,
		[XRP_DSP_SYNC_IRQ_MODE_EDGE] = XRP_IRQ_EDGE,
	};
	struct xrp_hw_simple_sync_data *hw_sync = p;
//	xrp_hw_panic_init(hw_sync->panic_base);
	mmio_base = hw_sync->device_mmio_base;
	pr_debug("%s: mmio_base: 0x%08x\n", __func__, mmio_base);

	if (hw_sync->device_irq_mode < sizeof(irq_mode) / sizeof(*irq_mode)) {
		device_irq_mode = irq_mode[hw_sync->device_irq_mode];
		device_irq_offset = hw_sync->device_irq_offset;
		device_irq_bit = hw_sync->device_irq_bit;
		device_irq = hw_sync->device_irq;
		pr_debug("%s: device_irq_mode = %d, device_irq_offset = %d, device_irq_bit = %d, device_irq = %d\n",
			__func__, device_irq_mode,
			device_irq_offset, device_irq_bit, device_irq);
	} else {
		device_irq_mode = XRP_IRQ_NONE;
	}

	if (hw_sync->host_irq_mode < sizeof(irq_mode) / sizeof(*irq_mode)) {
		host_irq_mode = irq_mode[hw_sync->host_irq_mode];
		host_irq_offset = hw_sync->host_irq_offset;
		host_irq_bit = hw_sync->host_irq_bit;
		pr_debug("%s: host_irq_mode = %d, host_irq_offset = %d, host_irq_bit = %d\n",
			__func__, host_irq_mode, host_irq_offset, host_irq_bit);
	} else {
		host_irq_mode = XRP_IRQ_NONE;
	}

	if (device_irq_mode != XRP_IRQ_NONE) {
#if XCHAL_HAVE_INTERRUPTS
#if XCHAL_HAVE_XEA3
		xthal_interrupt_sens_set(device_irq,
					 device_irq_mode == XRP_IRQ_LEVEL);
#endif
		xrp_interrupt_disable(device_irq);
		xrp_set_interrupt_handler(device_irq, xrp_irq_handler);
#endif
	}
}
int xrp_get_irq()
{
	 return device_irq;
}
void xrp_interrupt_on()
{
	 xrp_interrupt_enable(device_irq);
}
void xrp_interrupt_off()
{
	xrp_interrupt_disable(device_irq);
}


void xrp_hw_panic(void)
{
	if(panic)
	{
		panic->panic = 0xdeadbabe;
		panic->ccount = XT_RSR_CCOUNT();
	}

}


void hang(void)
{
	for (;;)
		xrp_hw_panic();
}

int xrp_hw_panic_init(void * base)
{
	if(base == NULL)
	{
		return -1;
	}
	else
	{
		panic = base;
	}
	return 0;
}
void xrp_hw_init()
{


}

void board_init(void)
{
    /* Nothing. */
}

void outbyte(int c)
{
	char b = (unsigned char)c;
	if(panic)
		xrp_rb_write((void *)&panic->rb, &b, 1);
}

int inbyte(void)
{
	return -1;
}
