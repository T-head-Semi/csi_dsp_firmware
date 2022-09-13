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
#include "csi_dsp_post_process_defs.h"
#include "ialgo.h"
#include "dsp_ps_debug.h"
#include "algo.h"
#include <xt_library_loader.h>
#include "lib_loader.h"

//#define USE_SUBFUNC_OBJ

#if defined ( USE_FL_OBJ )
	#define _loader  csi_dsp_load_flo
	#define _unloader  csi_dsp_unload_flo
#elif defined ( USE_PI_OBJ)
	#define _loader  csi_dsp_load_pilsl
	#define _unloader csi_dsp_unload_pilsl
#endif


#if (defined ( USE_FL_OBJ ) || defined ( USE_PI_OBJ)) && (!defined (LOAD_LIB_OTF))
/* This library will be provided by the object created by the
 * xt-pkg-loadlib tool. Observe that the name declared here must
 * match the name used with the "-e" option to the tool.
 */
//extern xtlib_packaged_library Overlay_kernel;
extern xtlib_packaged_library dsp_dummy_algo_lib;

#endif


algo_func dsp_algo_load(uint16_t  algo_id,uint64_t algo_ptr,csi_dsp_algo_param_t *info)
{
	wrapper_func func = NULL;
#if defined ( USE_SUBFUNC_OBJ )
	switch((csi_dsp_algo_lib_id_e)algo_id)
	{
		case CSI_DSP_ALGO_LIB_COPY:
			 func = dsp_dummy_algo_wrapper;
				break;
		default:
			ps_err("No valid algo to load\n");
			return NULL;
	}

#else
	xtlib_packaged_library *library=NULL;
	lib_loader_info_t lib_info;
	#if defined (LOAD_LIB_OTF)

	library = (xtlib_packaged_library *)algo_ptr;

	#else

	switch((csi_dsp_algo_lib_id_e)algo_id)
	{
		case CSI_DSP_ALGO_LIB_COPY:
				library = &dsp_dummy_algo_lib;
				break;
		default:
			  ps_err("No valid algo to load\n");
			  return NULL;
	}
	#endif

	func = _loader(library,&lib_info);
	if(func == NULL)
	{
		return NULL;
	}


#endif

	return func(info);
}
