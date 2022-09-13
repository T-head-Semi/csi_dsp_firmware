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

#ifndef  _LIB_LOADER_H
#define  _LIB_LOADER_H
#include <stdint.h>
#include <xt_library_loader.h>
#include "ialgo.h"

typedef struct lib_loader_info{

	union
	{
		xtlib_ovl_info ovl_info;
		xtlib_pil_info pil_info;
	};

	uint8_t code_memory_num;
	uint8_t data_memory_num;
	void* code_memory[4];
	int code_size[4];
	void* data_memory[4];
	int data_size[4];

}lib_loader_info_t;

extern wrapper_func csi_dsp_load_flo(void* obj,lib_loader_info_t *info);
extern void csi_dsp_unload_flo(lib_loader_info_t *info);
extern wrapper_func csi_dsp_load_pilsl(void* obj,lib_loader_info_t *info);
extern void  csi_dsp_unload_pilsl(lib_loader_info_t *info);

#endif /*  */
