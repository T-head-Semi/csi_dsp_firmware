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

#ifndef IDMA_UTIL_WQ_H
#define IDMA_UTIL_WQ_H

#if defined (LIBIDMA_USE_CHANNEL0)
# ifndef IDMA_USE_MULTICHANNEL
# define IDMA_USE_MULTICHANNEL
# endif
# define IDMA_CHANNEL   0
#endif
#if defined (LIBIDMA_USE_CHANNEL1)
# ifndef IDMA_USE_MULTICHANNEL
# define IDMA_USE_MULTICHANNEL
# endif
# define IDMA_CHANNEL   1
#endif

#if defined (LIBIDMA_USE_CHANNEL0) || defined (LIBIDMA_USE_CHANNEL1) || defined (IDMA_USE_MULTICHANNEL)
# ifndef IDMA_CHANNEL
# define IDMA_CHANNEL           0
# endif
# define IDMA_CH_ARG            IDMA_CHANNEL,
# define IDMA_CH_ARG_SINGLE     IDMA_CHANNEL
#else
# define IDMA_CH_ARG
# define IDMA_CH_ARG_SINGLE
#endif

#include <xtensa/idma.h>
#include <xtensa/hal.h>
#ifdef _XOS
#include <xtensa/xos.h>
#else
#include <xtensa/xtruntime.h>
#endif
#include <xtensa/xtutil.h>



#ifdef IDMA_DEBUG
#define DPRINT(fmt...)	do { printf(fmt);} while (0);
#else
#define DPRINT(fmt...)	do {} while (0);
#endif

void idmaLogHander (const char* str)
{
  DPRINT("libidma: %s", str);
}

void
idmaErrCB_ch0(const idma_error_details_t* data)
{
  DPRINT("ERROR CALLBACK: iDMA ch0 in Error\n");
  //idma_error_details_t* error = idma_error_details(0);
  //printf("CH0 COPY FAILED, Error 0x%x at desc:%p, PIF src/dst=%x/%x\n", error->err_type, (void*)error->currDesc, error->srcAddr, error->dstAddr);
}

void
idmaErrCB_ch1(const idma_error_details_t* data)
{
  DPRINT("ERROR CALLBACK: iDMA ch1 in Error\n");
  //idma_error_details_t* error = idma_error_details(1);
  //printf("CH1 COPY FAILED, Error 0x%x at desc:%p, PIF src/dst=%x/%x\n", error->err_type, (void*)error->currDesc, error->srcAddr, error->dstAddr);
}

void
idmaErrCB_ch2(const idma_error_details_t* data)
{
  DPRINT("ERROR CALLBACK: iDMA ch2 in Error\n");
  //idma_error_details_t* error = idma_error_details(2);
  //printf("CH1 COPY FAILED, Error 0x%x at desc:%p, PIF src/dst=%x/%x\n", error->err_type, (void*)error->currDesc, error->srcAddr, error->dstAddr);
}

void
idma_cb_func (void* data)
{
  DPRINT("CALLBACK: Copy request for task @ %p is done\n", data);
}


#endif /*  */
