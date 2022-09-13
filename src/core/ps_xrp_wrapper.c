/*
 * Copyright (c) 2021 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtensa/corebits.h>
#include <xtensa/xtruntime.h>
#if HAVE_THREADS_XOS
#include <xtensa/xos.h>
#endif
#include "dsp_ps_debug.h"
#include "xrp_api.h"
#include "xrp_dsp_hw.h"
#include "xrp_dsp_user.h"
#include "dsp_ps_ns.h"
//#include "isp_api.h"
#include "dsp_ps_ns.h"
#include "ps_sched.h"
#include "idma_if.h"

typedef void (*kernel_func) (void* info);

static struct xrp_device *device;

struct xrp_device * ps_xrp_device_int(int idx)
{
	enum xrp_status status;
	device = xrp_open_device(idx, &status);
	if (status != XRP_STATUS_SUCCESS)
	{
		return NULL;
	}
	return device;
}

int ps_register_cmd_handler(unsigned char *nsid,xrp_command_handler *cmd_handler,void * context)
{
	enum xrp_status status;
	if(device==NULL)
	{
		ps_err("device not valid\n");
		return -1;
	}
	xrp_device_register_namespace(device,nsid,cmd_handler, context, &status);
	if(status != XRP_STATUS_SUCCESS)
	{
		ps_err("register name space fail%s\n ",nsid);
		return -1;
	}
	return 0;
}

int ps_wait_host_sync()
{
	enum xrp_status status;
	ps_debug("wait to sync\n");
    for(;;){
		status=xrp_device_sync(device);
		if(status==XRP_STATUS_SUCCESS)
		{
			break;
		}
		else if(status == XRP_STATUS_PENDING)
		{
			continue;
		}
		else
		{
			return -1;
		}
    }
	ps_debug("sync done!\n");
    return 0;
}

int ps_send_report(uint32_t id)
{

	return xrp_send_report(device,id)==XRP_STATUS_SUCCESS? 1:0;
}
int ps_report_avaiable()
{
	return xrp_get_report_status(device)==XRP_STATUS_SUCCESS? 0:-1;
}
int ps_get_xrp_irq()
{
	return xrp_get_irq();
}


enum task_result  ps_process_xrp_task(void* context)
{

	if(XRP_STATUS_SUCCESS!=xrp_device_command_dispatch(device)){
        ps_err("xrp command handler fail\n");
		return PS_TASK_FAILURE;
	}
     return PS_TASK_SUCCESS;
}


