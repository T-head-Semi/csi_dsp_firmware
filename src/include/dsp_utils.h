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
/*!
 * \file
 * \brief This section defines DSP shared strcut for CPU&DSP&APP.
 *
 * General  properties:
 * 1. Post porcess define data and strcut, visiable for APP
 * 2. user define data shared bedtween DSP ans host
 */

#ifndef  DSP_UTILS_H
#define  DSP_UTILS_H
#include <stdio.h>
#include "xrp_ring_buffer.h"
#define MAX_PROFILE_GROUP    16
#define RING_MODE
//typedef struct profiler{
//
//	struct xrp_ring_buffer;
//}profiler_t;

typedef struct xrp_ring_buffer profiler;

typedef struct profile_group{
	int  status;
	char group_name[32];

}profile_group_t;


typedef struct profile_entry{
	char entry_name[8];
	uint32_t begin_cycle;
	uint32_t end_cycle;
	char reserve[12];
	uint32_t total_cycle;
}profile_entry_t;

typedef struct profile_head{
	profile_group_t group[MAX_PROFILE_GROUP];
	profile_entry_t entry[0];
}profile_head_t;

#ifdef DSP_PROFILE_ENBALE
int dsp_profile_init(void* buf,size_t buf_size);
void dsp_profile_entry_add(void*entry_ptr,size_t entry_size);
void dsp_profile_entry_add_ring(void*entry_ptr,size_t entry_size);
#define DSP_PROFILE_FUNC_START(cookie)  (					\
								static profile_entry_t entry;   \
								memcpy(entry.entry_name,___FUNCTION__,16);\
								entry.begin_cycle = XT_RSR_CCOUNT();\
								)

#define DSP_PROFILE_FUNC_END(cookie)   (					\
								entry.end_cycle = XT_RSR_CCOUNT();\
								xrp_rb_write(cookie,&entry,sizeof(profile_entry_t));\
								)

#define DSP_PROFILE_CUSTOM_DECLATR(name)   static profile_entry_t  entry_##name ={ \
																	.entry_name = #name \
																	};
#define DSP_PROFILE_CUSTOM_START(name) entry_##name.begin_cycle = XT_RSR_CCOUNT();
#define DSP_PROFILE_CUSTOM_ID_ADD(name,id)  *((int *)&entry_##name.entry_name[4])=id;
#ifdef RING_MODE
#define DSP_PROFILE_CUSTOM_END(name)   {entry_##name.end_cycle = XT_RSR_CCOUNT();\
										entry_##name.total_cycle = entry_##name.end_cycle- entry_##name.begin_cycle;\
										dsp_profile_entry_add_ring(&entry_##name,sizeof(profile_entry_t));}
#else
#define DSP_PROFILE_CUSTOM_END(name)   {entry_##name.end_cycle = XT_RSR_CCOUNT();\
										entry_##name.total_cycle = entry_##name.end_cycle- entry_##name.begin_cycle;\
										dsp_profile_entry_add(&entry_##name,sizeof(profile_entry_t));}
#endif
#else
#define DSP_PROFILE_FUNC_START(cookie)
#define DSP_PROFILE_FUNC_END(cookie)
#define DSP_PROFILE_CUSTOM_DECLATR(name)
#define DSP_PROFILE_CUSTOM_ID_ADD(name,id)
#define DSP_PROFILE_CUSTOM_START(name)
#define DSP_PROFILE_CUSTOM_END(name)
#endif
#endif /*  */
