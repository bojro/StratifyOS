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

/*! \addtogroup STFY
 * @{
 */

/*! \file */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "sos/sos.h"
#include "sos/fs/sysfs.h"
#include "config.h"
#include "mcu/debug.h"
#include "mcu/pio.h"
#include "sos/link.h"

extern void * link_update(void * args);

static int init_fs();
static int startup_fs();
static void start_filesystem(void);

static void priv_core_getinfo(void * args) MCU_PRIV_EXEC_CODE;

void priv_core_getinfo(void * args){
	mcu_core_getinfo(0, args);
}

void check_reset_source(void){
	core_info_t info;
	cortexm_svcall(priv_core_getinfo, &info);
	if( info.o_flags & CORE_FLAG_IS_RESET_SOFTWARE ){
		SOS_TRACE_MESSAGE("soft reset");
	} else if( info.o_flags & CORE_FLAG_IS_RESET_POR ){
		SOS_TRACE_MESSAGE("por reset");
	} else if( info.o_flags & CORE_FLAG_IS_RESET_EXTERNAL ){
		SOS_TRACE_MESSAGE("ext reset");
	} else if( info.o_flags & CORE_FLAG_IS_RESET_WDT ){
		SOS_TRACE_MESSAGE("wdt reset");
	} else if( info.o_flags & CORE_FLAG_IS_RESET_BOR ){
		SOS_TRACE_MESSAGE("bor reset");
	} else if( info.o_flags & CORE_FLAG_IS_RESET_SYSTEM ){
		SOS_TRACE_MESSAGE("sys reset");
	}
	mcu_board_execute_event_handler(MCU_BOARD_CONFIG_EVENT_START_INIT, &info);
}

void start_filesystem(void){
	u32 started;
	started = startup_fs();
	mcu_debug_user_printf("Started Filesystem Apps %ld\n", started);
	mcu_board_execute_event_handler(MCU_BOARD_CONFIG_EVENT_START_FILESYSTEM, &started);
}

void * sos_default_thread(void * arg){

	mcu_debug_user_printf("Enter default thread\n");

	check_reset_source();

	//Initialize the file systems
	if ( init_fs() < 0 ){
		mcu_board_execute_event_handler(MCU_BOARD_CONFIG_EVENT_FATAL, (void*)"init_fs");
	}

	start_filesystem();

	mcu_debug_user_printf("Open RTC\n");
	open("/dev/rtc", O_RDWR);


	mcu_debug_user_printf("Board Event\n");
	mcu_board_execute_event_handler(MCU_BOARD_CONFIG_EVENT_START_LINK, 0);

	mcu_debug_user_printf("Start Link\n");
	link_update(arg); 	//Run the link update thread--never returns

	return NULL;
}


int init_fs(){
	int i;
	i = 0;
	while( sysfs_isterminator(&sysfs_list[i]) == false ){
		SOS_TRACE_MESSAGE(sysfs_list[i].mount_path);
		mcu_debug_user_printf("init %s\n", sysfs_list[i].mount_path);
		if ( sysfs_list[i].mount( sysfs_list[i].config ) < 0 ){
			mcu_debug_user_printf("failed to init\n");
			SOS_TRACE_CRITICAL(sysfs_list[i].mount_path);
		}
		i++;
	}
	return 0;
}

int startup_fs(){
	int i;
	i = 0;
	int started;
	int ret;
	started = 0;
	while( sysfs_isterminator(&sysfs_list[i]) == false ){
		ret = sysfs_list[i].startup( sysfs_list[i].config );
		if ( ret > 0){
			started += ret;
		}
		i++;
	}
	//the number of processes that have been started
	return started;
}


