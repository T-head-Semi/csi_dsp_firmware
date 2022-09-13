/*
 * Copyright (c) 2020 Cadence Design Systems Inc. ALL RIGHTS RESERVED.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__XTENSA__)
#include <xtensa/tie/xt_ivpn.h>
#include "tileManager.h"

idma_cntrl_t idma_cntrl __attribute__ ((section(".dram0.data"))) __attribute__ ((weak));
idma_buf_t* _idma_buf_ptr __attribute__ ((section(".dram0.data"))) __attribute__ ((weak));

void
_idma_reset()
{
 idma_cntrl.num_outstanding = 0;
 idma_cntrl.newest_task = 0;
 idma_cntrl.oldest_task = 0;
}

idma_status_t
idma_init(int32_t ch,
          unsigned init_flags,
          idma_max_block_t block_sz,
          unsigned max_pif_req,
          idma_ticks_cyc_t ticks_per_cyc,
          unsigned timeout_ticks,
          idma_err_callback_fn err_cb_func)
{

#if 0
  unsigned _haltable;

  if(max_pif_req > 64  || max_pif_req < 0) {
    return IDMA_ERR_BAD_INIT;
  }

  IDMA_DISABLE_INTS();

  _idma_reset();

  _haltable = (init_flags & IDMA_OCD_HALT_ON) ? 1 : 0;

  if(max_pif_req == 64)
    max_pif_req = 0;

  idma_cntrl.settings =  (block_sz    & IDMA_MAX_BLOCK_SIZE_MASK)  << IDMA_MAX_BLOCK_SIZE_SHIFT;
  idma_cntrl.settings |= (max_pif_req & IDMA_OUTSTANDING_REG_MASK) << IDMA_OUTSTANDING_REG_SHIFT;
  idma_cntrl.settings |= (_haltable   & IDMA_HALT_ON_OCD_MASK)     << IDMA_HALT_ON_OCD_SHIFT;

  idma_cntrl.timeout   = (ticks_per_cyc & IDMA_TIMEOUT_CLOCK_MASK) << IDMA_TIMEOUT_CLOCK_SHIFT;
  idma_cntrl.timeout  |= (timeout_ticks & IDMA_TIMEOUT_THRESHOLD_MASK) << IDMA_TIMEOUT_THRESHOLD_SHIFT;

  idma_cntrl.reg_base         = idmareg_base;
  idma_cntrl.num_outstanding  = 0;

  idma_cntrl.err_cb_func      = err_cb_func;

  IDMA_WER(idma_cntrl.settings, idmareg_base + IDMA_REG_SETTINGS);
  IDMA_WER(idma_cntrl.timeout,  idmareg_base + IDMA_REG_TIMEOUT);

  /* Enable the uDMA */
  //_idma_enable();

#if XCHAL_HAVE_INTERRUPTS
//  idma_register_interrupts(idma_sync_intr_handler, idma_sync_err_handler);
#endif


  idma_cntrl.oldest_task = NULL;
  idma_cntrl.newest_task = NULL;
  idma_cntrl.initialized = 1;

  IDMA_ENABLE_INTS();
#endif  
  return (idma_status_t) IDMA_OK;

}



idma_status_t
idma_init_loop(int32_t ch,
               idma_buffer_t *bufh,
               idma_type_t type,
               int32_t num_descs,
               void *cb_data,
               idma_callback_fn cb_func) 
{

#if 0  
  idma_buf_t *buf = (idma_buf_t*) bufh;
  idma_desc_t* desc  = (idma_desc_t*)&buf->desc;
  buf->last_desc    = (desc + num_descs*type);

  *(unsigned*)(buf->last_desc) = (unsigned)desc; //JUMP
  //_idma_disable();
  //_idma_set_start_address(desc);

  buf->cur_desc_i = 0;
  buf->num_descs = num_descs;
  buf->next_desc = desc;   //FIXME - CAN'T HAVE ALL THESE !!!
  buf->next_add_desc = desc;
  buf->type = type;

  _idma_buf_ptr = buf;

  /* Num outstanding descs */
  buf->status = IDMA_TASK_EMPTY; // FIXME - out !!



#if XCHAL_HAVE_INTERRUPTS
  if(cb_func) {
    buf->cb_data = cb_data;
    buf->cb_func = cb_func;
  }

//  idma_register_interrupts(idma_intr_loop_handler, idma_err_loop_handler);
#endif
#endif

  return (idma_status_t) IDMA_OK;
}



int32_t copy2d(int32_t ch,
               void *dst,
               void *src,
               size_t row_size,
               unsigned flags,
               unsigned num_rows,
               unsigned src_pitch,
               unsigned dst_pitch) 
{
  int32_t indx;
  
  uint8_t *srcPtr, *dstPtr;

  srcPtr = (uint8_t*) ((int32_t*)src)[0];
  dstPtr = (uint8_t*) ((int32_t*)dst)[0];

  for (indx = 0; indx < num_rows; indx++)
  {
    memcpy(dstPtr, srcPtr, row_size);
    dstPtr += dst_pitch;
    srcPtr += src_pitch;
  }

  return 0;
}


int32_t idma_copy_2d_pred_desc64_wide(int32_t ch,
                  void *dst,
                  void *src,
                  size_t row_size,
                  uint32_t flags,
                  void* pred_mask,
                  uint32_t num_rows,
                  uint32_t src_pitch,
                  uint32_t dst_pitch)

{

  int32_t indx;
  
  uint8_t *srcPtr, *dstPtr;

  uint32_t* MaskPtr = (uint32_t* ) pred_mask;

  srcPtr = (uint8_t*) ((int32_t*)src)[0];
  dstPtr = (uint8_t*) ((int32_t*)dst)[0];

  for (indx = 0; indx < num_rows; indx++)
  {
    if (MaskPtr[indx/32] & (1 << (indx & 0x01F)))
    {
        memcpy(dstPtr, srcPtr, row_size);
        dstPtr += dst_pitch;
    }
    srcPtr += src_pitch;
  }

  return 0;


}


int32_t idma_copy_3d_desc64_wide(int32_t ch,
                       void *dst,
                       void *src,
                       uint32_t flags,
                       size_t   row_sz,
                       uint32_t nrows,
                       uint32_t ntiles,
                       uint32_t src_row_pitch,
                       uint32_t dst_row_pitch,
                       uint32_t src_tile_pitch,
                       uint32_t dst_tile_pitch)
{

  uint8_t *srcPtr, *dstPtr;
  int32_t indx;

  srcPtr = (uint8_t*) ((int32_t*)src)[0];
  dstPtr = (uint8_t*) ((int32_t*)dst)[0];

  for (indx = 0; indx < ntiles; indx++)
  {
    copy2d(ch, &dstPtr, &srcPtr, row_sz, flags, nrows, src_row_pitch, dst_row_pitch) ;

    dstPtr += dst_tile_pitch;
    srcPtr += src_tile_pitch;
  }
  return 0;

}


void dma_sleep(int32_t ch) 
{


}

int32_t idma_desc_done(int32_t ch, int32_t index) 
{
  //To improbe coverage. alternately return 0/1
  static int32_t Flag = 1;
  Flag = Flag ^ 1;
  return Flag;
}





#endif







