/* Copyright 2011-2016 Tyler Gilbert; 
 * This file is part of Stratify OS.
 *
 * Stratify OS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stratify OS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stratify OS.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 */

#include <errno.h>
#include <fcntl.h>
#include "mcu/cortexm.h"
#include "mcu/dac.h"
#include "mcu/debug.h"
#include "mcu/core.h"
#include "mcu/dma.h"


#if MCU_DAC_PORTS > 0


#define DAC_CTRL_DBUF_ENA (1<<1)
#define DAC_CTRL_CNT_ENA (1<<2)
#define DAC_CTRL_DMA_ENA (1<<3)

#define DAC_DMA_CHAN 5

typedef struct {
	mcu_event_handler_t handler;
	volatile int len;
	void * volatile bufp;
	uint8_t enabled_channels;
	uint8_t ref_count;
} dac_local_t;

static dac_local_t dac_local[MCU_DAC_PORTS] MCU_SYS_MEM;

//DMA functions
static int dac_dma_transfer(const devfs_handle_t * cfg);
static int dma_write_complete(void * context, mcu_event_t * data);

#if defined __lpc43xx
static int write_complete(void * context, mcu_event_t data);
#endif

static void exec_callback(int port, u32 o_events);

void _mcu_dac_dev_power_on(int port){
	if ( dac_local[port].ref_count == 0 ){
		dac_local[port].ref_count++;
		memset(&dac_local, 0, sizeof(dac_local_t));
	}

	dac_local[port].ref_count++;
}

void _mcu_dac_dev_power_off(int port){
	if ( dac_local[port].ref_count > 0 ){
		if ( dac_local[port].ref_count == 1 ){
		_mcu_core_set_pinsel_func(0,26,CORE_PERIPH_PIO,0);
		}
		dac_local[port].ref_count--;
	}
}

int _mcu_dac_dev_powered_on(int port){
	return dac_local[port].ref_count != 0;
}

int mcu_dac_getinfo(int port, void * ctl){
	dac_info_t * info = ctl;
	info->o_flags = 0;
	info->freq = DAC_MAX_FREQ;
	return 0;
}

int mcu_dac_dma_setattr(int port, void * ctl){
	int ret;
	ret = mcu_dac_setattr(port, ctl);
	if( ret < 0 ){
		return ret;
	}
	_mcu_dma_init(0);

	return 0;
}

int mcu_dac_setattr(int port, void * ctl){
	dac_attr_t * attr = ctl;
	int clkdiv;

	if ( attr->freq == 0 ){
		errno = EINVAL;
		return -1 - offsetof(dac_attr_t, freq);
	}

	if ( attr->freq > DAC_MAX_FREQ ){
		attr->freq = DAC_MAX_FREQ;
	}

	if( mcu_core_set_pin_assignment(attr->pin_assignment, DAC_PIN_ASSIGNMENT_COUNT, CORE_PERIPH_DAC, port) < 0 ){
		return -1;
	}

	clkdiv = mcu_board_config.core_periph_freq / attr->freq;
	if ( clkdiv > ((1<<16)-1) ){
		clkdiv = ((1<<16)-1);
	} else if ( clkdiv < 1 ){
		clkdiv = 1;
	}

#ifdef LPCXX7X_8X
		LPC_IOCON->P0_26 = 0x02 | (1<<16); //Enable the DAC pin
#endif

	LPC_DAC->CNTVAL = clkdiv;
	return 0;
}

int mcu_dac_setaction(int port, void * ctl){
	mcu_action_t * action = (mcu_action_t*)ctl;
	if( action->handler.callback == 0 ){
		if ( LPC_GPDMA->ENBLDCHNS & (1<<DAC_DMA_CHAN) ){
			exec_callback(port, MCU_EVENT_FLAG_CANCELED);
		}
	}

	if( _mcu_cortexm_priv_validate_callback(action->handler.callback) < 0 ){
		return -1;
	}

	dac_local[port].handler.callback = action->handler.callback;
	dac_local[port].handler.context = action->handler.context;

	return 0;
}


int mcu_dac_dma_setaction(int port, void * ctl){
	mcu_action_t * action = (mcu_action_t*)ctl;
	if( action->handler.callback == 0 ){
		if ( LPC_GPDMA->ENBLDCHNS & (1<<DAC_DMA_CHAN) ){
			exec_callback(port, MCU_EVENT_FLAG_CANCELED);
		}
	}

	if( _mcu_cortexm_priv_validate_callback(action->handler.callback) < 0 ){
		return -1;
	}

	dac_local[port].handler.callback = action->handler.callback;
	dac_local[port].handler.context = action->handler.context;

	return 0;
}

int mcu_dac_get(int port, void * ctl){
	mcu_channel_t * channel = ctl;
	channel->value = LPC_DAC->CR;
	return 0;
}

int mcu_dac_set(int port, void * ctl){
	mcu_channel_t * attr = ctl;
	LPC_DAC->CR = attr->loc;
	return 0;
}

int _mcu_dac_dma_dev_write(const devfs_handle_t * cfg, devfs_async_t * wop){
	//Check to see if the DAC is busy
	const int port = cfg->port;
	if ( wop->loc != 0 ){
		errno = EINVAL;
		return -1;
	}

	if ( LPC_GPDMA->ENBLDCHNS & (1<<DAC_DMA_CHAN) ){
		errno = EAGAIN;
		return -1;
	}

	if( wop->nbyte == 0 ){
		errno = EINVAL;
		return -1;
	}

	dac_local[port].bufp = (void * volatile)wop->buf;
	dac_local[port].len = (wop->nbyte) >> 2;
	dac_dma_transfer(cfg);
	LPC_DAC->CTRL = DAC_CTRL_DMA_ENA|DAC_CTRL_CNT_ENA|DAC_CTRL_DBUF_ENA;
	wop->nbyte = (wop->nbyte) & ~0x3;

	if( _mcu_cortexm_priv_validate_callback(wop->handler.callback) < 0 ){
		return -1;
	}

	dac_local[port].handler.callback = wop->handler.callback;
	dac_local[port].handler.context = wop->handler.context;

	return 0;
}

int _mcu_dac_dev_write(const devfs_handle_t * cfg, devfs_async_t * wop){
	//Check to see if the DAC is busy
#if defined __lpc43xx
	const int port = cfg->port;
	if ( wop->loc != 0 ){
		errno = EINVAL;
		return -1;
	}

	if( wop->nbyte == 0 ){
		errno = EINVAL;
		return -1;
	}

	//LPC43xx had a dedicated DAC interrupt
	dac_local[port].bufp = (void * volatile)wop->buf;
	dac_local[port].len = (wop->nbyte) >> 2;
	//dac_dma_transfer(cfg);
	//LPC_DAC->CTRL = DAC_CTRL_DMA_ENA|DAC_CTRL_CNT_ENA|DAC_CTRL_DBUF_ENA;
	wop->nbyte = (wop->nbyte) & ~0x3;

	if( _mcu_cortexm_priv_validate_callback(wop->handler.callback) < 0 ){
		return -1;
	}

	dac_local[port].handler.callback = wop->handler.callback;
	dac_local[port].handler.context = wop->handler.context;

	return 0;
#else
	errno = ENOTSUP;
	return -1;
#endif
}


int dac_dma_transfer(const devfs_handle_t * cfg){
	int page_size;
	int ctrl;
	int err;
	const int port = cfg->port;

	page_size = dac_local[port].len > (DMA_MAX_TRANSFER_SIZE >> 2) ? (DMA_MAX_TRANSFER_SIZE >> 2) : dac_local[port].len;

	ctrl = DMA_CTRL_TRANSFER_SIZE(page_size)|
			DMA_CTRL_SRC_WIDTH32|
			DMA_CTRL_DEST_WIDTH32|
			DMA_CTRL_ENABLE_TERMINAL_COUNT_INT|
			DMA_CTRL_SRC_INC;

	err = _mcu_dma_transfer(DMA_MEM_TO_PERIPH,
			DAC_DMA_CHAN,
			(void*)&(LPC_DAC->CR),
			(void*)dac_local[port].bufp,
			ctrl,
			dma_write_complete,
			(void*)cfg,
			DMA_REQ_DAC,
			0);


	if ( err < 0 ){
		return -1;
	}

	dac_local[port].len -= page_size;
	dac_local[port].bufp += (page_size << 2);
	return 0;
}

void exec_callback(int port, u32 o_events){
	dac_local[port].bufp = NULL;
	//call the signal callback
	LPC_DAC->CTRL = 0;
	mcu_execute_event_handler(&(dac_local[port].handler), o_events, 0);
}

int dma_write_complete(void * context, mcu_event_t * data){
	const devfs_handle_t * cfg = context;
	const int port = cfg->port;
	if ( dac_local[port].len ){
		dac_dma_transfer(cfg);
		return 1; //keep interrupt in place
	} else {
		exec_callback(port, MCU_EVENT_FLAG_WRITE_COMPLETE);
	}
	return 0;
}


#if defined __lpc43xx

int write_complete(void * context, mcu_event_t data){
	const devfs_handle_t * cfg = context;
	const int port = cfg->port;
	if ( dac_local[port].len ){
		//dac_dma_transfer(cfg);
		return 1; //keep interrupt in place
	} else {
		exec_callback(port, 0);
	}
	return 0;
}

//when not using DMA -- use dedicated interrupt
void _mcu_core_dac0_isr(){
	mcu_event_t data = 0;
	write_complete(dac_local[0].handler.context, data);
}


#endif




#endif
