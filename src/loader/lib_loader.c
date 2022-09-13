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

#include <xt_library_loader.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dsp_ps_debug.h"
#include "lib_loader.h"
#include "ialgo.h"


wrapper_func csi_dsp_load_flo(void* obj,lib_loader_info_t *info)
{
	wrapper_func flo_start = NULL;
	flo_start= xtlib_load_overlay_ext((xtlib_packaged_library * )obj,&info->ovl_info);
	if(flo_start==NULL)
	{
		ps_debug("failed with error code: %d\n", xtlib_error());
	}
	return flo_start;
}

void csi_dsp_unload_flo(lib_loader_info_t *info)
{
	xtlib_unload_overlay_ext(&info->ovl_info);
}

wrapper_func csi_dsp_load_pilsl(void* obj,lib_loader_info_t *info)
{
    unsigned int code_bytes, data_bytes;
    wrapper_func plo_start = NULL;
    if (xtlib_split_pi_library_size ((xtlib_packaged_library * )obj, &code_bytes, &data_bytes) != XTLIB_NO_ERR) {
    	ps_err("xtlib_split_pi_library_size failed with error: %d\n", xtlib_error ());
    	return 0;
    }
    void * data_memory = malloc (data_bytes);
    void * code_memory = malloc (code_bytes);

    if (!code_memory || !data_memory){
    	ps_err ("malloc failed\n");
    	return 0;
    }
	info->code_memory_num = 1;
    info->code_memory[0]= code_memory;
	info->code_size[0] = code_bytes;
	info->data_memory_num = 1;
	info->data_memory[0] = data_memory;
	info->data_size[0] = data_bytes;

	plo_start= xtlib_load_split_pi_library((xtlib_packaged_library * )obj, code_memory, data_memory, &info->pil_info);
	if(plo_start==NULL)
	{
		ps_debug("failed with error code: %d\n", xtlib_error());
	}
	return plo_start;
}

void  csi_dsp_unload_pilsl(lib_loader_info_t *info)
{
	int i;
	xtlib_unload_pi_library(&info->pil_info);
	for(i=0;i<info->code_memory_num;i++)
	{
		free(info->code_memory[i]);
	}

	for(i=0;i<info->data_memory_num;i++)
	{
		free(info->data_memory[i]);
	}

}
