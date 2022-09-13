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
#include <stdlib.h>
#include <stdio.h>
#include "dsp_utils.h"
#include "xrp_rb_file.h"

static profiler* profil_hanlder;
int dsp_profile_init(void* buf,size_t buf_size)
{
	if(buf == NULL)
	{
		return -1;
	}
	profil_hanlder =buf;
	profil_hanlder->read = 0;
	profil_hanlder->write = 0;
	profil_hanlder->size = buf_size - sizeof(profiler);
	return 0;
}

void dsp_profile_entry_add(void*entry_ptr,size_t entry_size)
{
	 xrp_rb_write((void*)profil_hanlder,entry_ptr,entry_size);
	 return;
}

void dsp_profile_entry_add_ring(void*entry_ptr,size_t entry_size)
{
	size_t wirte_size = xrp_rb_write((void*)profil_hanlder,entry_ptr,entry_size);
	if(wirte_size <entry_size)
	{
		profil_hanlder->read = 0;
		profil_hanlder->write = 0;
	}
	return;

}
