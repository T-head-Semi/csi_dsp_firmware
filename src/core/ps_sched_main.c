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
#include "xrp_api.h"
#include "xrp_dsp_hw.h"
#include "xrp_dsp_user.h"
#include "dsp_ps_ns.h"
#include "ps_sched.h"

extern enum xrp_status ps_run_command(void *context,
        const void *in_data, size_t in_data_size,
        void *out_data, size_t out_data_size,
        struct xrp_buffer_group *buffer_group);



void abort(void)
{
	printf("abort() is called; halting\n");
	hang();
}



#ifdef DSP_TEST
int main_ignre(void)
#else
int main(void)
#endif
{
	enum xrp_status status;
	struct xrp_device *device =NULL;

	ps_exception_init();

	xrp_hw_init();

	device = ps_xrp_device_int(0);
	if (NULL == device) {

		fprintf(stderr, "xrp_open_device failed\n");
		abort();
	}

/**************wait sync from host***************************/
    if(0!=ps_wait_host_sync())
    {
		fprintf(stderr, "sync failed\n");
		abort();
    }

    if(0 != init_scheduler(device)){
		fprintf(stderr, "scheduler init failed\n");
		abort();
    }

    ps_task_enable_common_task();
    ps_task_shedule(device);

	return 0;
}
