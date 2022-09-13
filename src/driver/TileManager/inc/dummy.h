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

#ifndef DUMMY_H__
#define DUMMY_H__

#define IDMA_PSO_SAVE_SIZE  5

/* iDMA registers offsets */
#define IDMA_REG_SETTINGS       0x00
#define IDMA_REG_TIMEOUT        0x04
#define IDMA_REG_DESC_START     0x08
#define IDMA_REG_NUM_DESC       0x0C
#define IDMA_REG_DESC_INC       0x10
#define IDMA_REG_CONTROL        0x14
#define IDMA_REG_USERPRIV       0x18
#define IDMA_REG_STATUS         0x40
#define IDMA_REG_CURR_DESC      0x44
#define IDMA_REG_DESC_TYPE      0x48
#define IDMA_REG_SRC_ADDR       0x4C
#define IDMA_REG_DST_ADDR       0x50
#define IDMA_CHANNEL_0          0
#define IDMA_CHANNEL_1          1
#define IDMA_CHANNEL_2          2
#define IDMA_CHANNEL_3          3


#define idmareg_base            0x110000

#define IDMA_1D_DESC_SIZE     16
#define IDMA_2D_DESC_SIZE     32

#if !defined(IDMA_1D_DESC) || !defined(IDMA_2D_DESC) || !defined(IDMA_64B_DESC)
#define IDMA_1D_DESC  1
#define IDMA_2D_DESC  2
#define IDMA_64B_DESC  3
#endif

#if !defined(MAX_BLOCK_2) || !defined(MAX_BLOCK_4) || !defined(MAX_BLOCK_8) || !defined(MAX_BLOCK_16)
#define MAX_BLOCK_2   0
#define MAX_BLOCK_4   1
#define MAX_BLOCK_8   2
#define MAX_BLOCK_16  3
#endif

#define idma_type_t       int32_t
#define idma_max_block_t  int32_t

#if defined(WIN32)
#define aligned(x)
#define __attribute__(x)
#endif

#if !defined(TICK_CYCLES_1) || !defined(TICK_CYCLES_2)
#define TICK_CYCLES_1  1
#define TICK_CYCLES_2  2
#endif

#if !defined(DESC_NOTIFY_W_INT)
#define DESC_NOTIFY_W_INT  0
#endif

#if !defined(XT_ISS_CYCLE_ACCURATE) || !defined(XT_ISS_FUNCTIONAL)
#define XT_ISS_CYCLE_ACCURATE  0
#define XT_ISS_FUNCTIONAL      1
#endif

/* Settings reg */
#define IDMA_MAX_BLOCK_SIZE_SHIFT 2
#define IDMA_MAX_BLOCK_SIZE_MASK  0x3
#define IDMA_OUTSTANDING_REG_SHIFT  8
#define IDMA_OUTSTANDING_REG_MASK 0x3F
#define IDMA_HALT_ON_OCD_SHIFT    6
#define IDMA_HALT_ON_OCD_MASK   0x1
#define IDMA_FETCH_START    7
#define IDMA_FETCH_START_MASK   0x1

/* Timeout reg fields */
#define IDMA_TIMEOUT_CLOCK_SHIFT       0
#define IDMA_TIMEOUT_CLOCK_MASK        0x7
#define IDMA_TIMEOUT_THRESHOLD_SHIFT   3
#define IDMA_TIMEOUT_THRESHOLD_MASK    0x1FFFFFFF

#define IDMA_HAVE_TRIG_SHIFT          1
#define IDMA_HAVE_TRIG_MASK           0x2

#define IDMA_ERRCODES_SHIFT           18
#define IDMA_ERRORS_MASK              0xFFFC0000


#define IDMA_OCD_HALT_ON       0x001  /* Enable iDMA halt on OCD interrupt. */

#define xt_iss_switch_mode(a)

#define xt_profile_enable(void)

#define xt_profile_disable(void)

#define IDMA_DISABLE_INTS(void)

#define IDMA_ENABLE_INTS(void)

#define XT_RSR_CCOUNT()  0

#define no_reorder

/* allocate space for n descriptors of type idma_type_t. */
#define IDMA_BUFFER_SIZE(n,type) \
                        (((n)*((type == IDMA_1D_DESC) ? IDMA_1D_DESC_SIZE : IDMA_2D_DESC_SIZE)) + sizeof(idma_buf_t))

/* Define a buffer of containing n descriptors of type n */
#define IDMA_BUFFER_DEFINE(name, n, type) \
                           idma_buffer_t name[IDMA_BUFFER_SIZE((n), type)/sizeof(idma_buffer_t)]

#define idma_copy_2d_desc64_wide  copy2d

#define idma_desc_done     dma_desc_done

#define idma_sleep         dma_sleep

#ifndef idma_max_pif_t
#define idma_max_pif_t  int32_t
#endif

#ifndef idma_error_details_t
#define idma_error_details_t  int32_t
#endif

#ifndef idma_buffer_t
#define idma_buffer_t  int32_t
#endif

//TODO
//#define IDMA_RER(x)      XT_RER(x)
//#define IDMA_WER(x, y)   XT_WER(x,y)
#define IDMA_RER(x)
#define IDMA_WER(x, y)

/**
 * iDMA API return values
 * For most of the iDMAlib API calls
 */
typedef enum
{
    IDMA_ERR_BAD_DESC = -20,        /* Descriptor not correct */
    IDMA_ERR_NOT_INIT,              /* iDMAlib and HW not initialized  */
    IDMA_ERR_TASK_NOT_INIT,         /* Cannot scheduled uninit. task  */
    IDMA_ERR_BAD_TASK,              /* Task not correct  */
    IDMA_ERR_BUSY,                  /* iDMA busy when not expected */
    IDMA_ERR_IN_SPEC_MODE,          /* iDMAlib in unexpected mode */
    IDMA_ERR_NOT_SPEC_MODE,         /* iDMAlib in unexpected mode */
    IDMA_ERR_TASK_EMPTY,            /* No descs in the task/buffer */
    IDMA_ERR_TASK_OUTSTAND_NEG,     /* Number of outstanding descs is a negative value  */
    IDMA_ERR_TASK_IN_ERROR,         /* Task in error */
    IDMA_ERR_BUFFER_IN_ERROR,       /* Buffer in error */
    IDMA_ERR_NO_NEXT_TASK,          /* Next task to process is missing  */
    IDMA_ERR_BUF_OVFL,              /* Attempt to schedule too many descriptors */
    IDMA_ERR_HW_ERROR,              /* HW error detected */
    IDMA_ERR_BAD_INIT,              /* Bad idma_init args */
    IDMA_OK = 0                     /* No error */
} idma_status_t;

typedef void (*idma_callback_fn)(void*);

typedef void (*idma_err_callback_fn)(const idma_error_details_t*);

/* log handler typedef */
typedef void (*idma_log_h)( const char* );

/* 1D descriptor structure */
typedef struct idma_desc_struct
{
    unsigned  control;
    void      *src;
    void      *dst;
    unsigned  size;
} idma_desc_t;

/* 2D descriptor structure */
typedef struct idma_2d_desc_struct
{
    unsigned  control;
    void      *src;
    void      *dst;
    unsigned  size;
    unsigned  src_pitch;
    unsigned  dst_pitch;
    unsigned  num_rows;
    unsigned  dummy;
} idma_2d_desc_t;

/* Real buffer struct - NOT TO BE USED BY APPLICATION */

typedef struct idma_buf_struct
{
    idma_desc_t*   next_desc;     // points past the last scheduled desc. Used by functions
    // needing to update the desc that is just to be scheduled.
    idma_desc_t*   next_add_desc; // points past the last added desc. Used by functions that
    // populate a new task, or populate a buffer where later
    // schedule call will schedule them all at once.

    unsigned       num_descs;     // total num of descs, assigned on init. In fixed buffer mode
    // it sets the size of the circular buffer by setting JUMP
    // command between the last desc and the 1st one. In TASK mode
    // it is used to tell the lib how many descs to schedule.
    idma_desc_t*   last_desc;     // points past the last desc. Assigned to sped-up wrap-arround
    // calculation, not to use num_descs and multiplication.

    unsigned       type;          // descs type, assigned on init.

    struct idma_buf_struct*  next_task; // TASK: points to next task, assigned on schedule
    unsigned       cur_desc_i;    // FIXED-BUFFER: tracks next desc to execute.
    int            status;        // TASK: # of remaining descs after schedule.
    // BOTH modes: error type, if in error.
    idma_callback_fn  cb_func;       // Callback on completion
    void*             cb_data;       // Completion callback argument
    idma_desc_t       desc    __attribute__ ((aligned(16)));
} idma_buf_t;

typedef struct idma_cntrl_t
{
    idma_buf_t*       oldest_task;
    idma_buf_t*       newest_task;
    uintptr_t         reg_base;
    unsigned          timeout;
    unsigned          settings;
    idma_err_callback_fn    err_cb_func;

    int16_t           num_outstanding;
    idma_log_h        xlogh;
    unsigned char     initialized;
    idma_error_details_t  error;
} idma_cntrl_t;


/**
 * iDMA task status API return values
 * NOTE: Valid only after task is scheduled
 *
 * N (>=0)            - Number of outstanding scheduled descriptors.
 * 0 (IDMA_TASK_DONE) - Whole task has completed the execution.
 * IDMA_TASK_ERROR    - iDMA HW error happened due to a descriptor executed
 *                      from this task. Error details are available (idma_hw_error_t).
 * IDMA_TASK_ABORTED  - Task forcefully aborted.
 */
typedef enum
{
    IDMA_TASK_ABORTED = -3,
    IDMA_TASK_ERROR   = -2,
    IDMA_TASK_NOINIT  = -1,
    IDMA_TASK_DONE    =  0,
    IDMA_TASK_EMPTY   =  0
} task_status_t;


#define idma_buffer_error_details  dma_buffer_error_details


#ifndef idma_ticks_cyc_t
#define idma_ticks_cyc_t  int32_t
#endif

#ifndef IDMA_OK
#define IDMA_OK  0
#endif

#ifndef TICK_CYCLES_2
#define TICK_CYCLES_2  0
#endif

#ifndef IDMA_2D_DESC
#define IDMA_2D_DESC  0
#endif

#if !defined(XCHAL_HAVE_VISION)
/****************************************************************************
            Parameters Useful for Any Code, USER or PRIVILEGED
****************************************************************************/

/*
 *  Note:  Macros of the form XCHAL_HAVE_*** have a value of 1 if the option is
 *  configured, and a value of 0 otherwise.  These macros are always defined.
 */

#define XCHAL_HAVE_VISION  1                  /* For Vision processors */

/*----------------------------------------------------------------------
                        INTERNAL I/D RAM/ROMs and XLMI
   ----------------------------------------------------------------------*/
#define XCHAL_NUM_INSTROM  0                    /* number of core instr. ROMs */
#define XCHAL_NUM_INSTRAM  0                    /* number of core instr. RAMs */
#define XCHAL_NUM_DATAROM  0                    /* number of core data ROMs */
#define XCHAL_NUM_DATARAM  2                    /* number of core data RAMs */
#define XCHAL_NUM_URAM     0                    /* number of core unified RAMs*/
#define XCHAL_NUM_XLMI     0                    /* number of core XLMI ports */

/*  Data RAM 0:  */
//Use a margin of 4K from top and bottom so that we can pass
//Memory allocator unit tests for "pointer outside valid DRAM range"
//on GCC
#define XCHAL_DATARAM0_VADDR       0x00001000   /* virtual address */
#define XCHAL_DATARAM0_PADDR       0x00001000   /* physical address */
#define XCHAL_DATARAM0_SIZE        (0x7FFFFFFF - 0x2000)  /* size in bytes */
#define XCHAL_DATARAM0_ECC_PARITY  0            /* ECC/parity type, 0=none */
#define XCHAL_DATARAM0_BANKS       2            /* number of banks */
#define XCHAL_HAVE_DATARAM0        1
#define XCHAL_DATARAM0_HAVE_IDMA   1            /* idma supported by this local memory */

/*  Data RAM 1:  */
//Use a margin of 4K from top and bottom so that we can pass
//Memory allocator unit tests for "pointer outside valid DRAM range"
//on GCC
#define XCHAL_DATARAM1_VADDR       0x00001000   /* virtual address */
#define XCHAL_DATARAM1_PADDR       0x00001000   /* physical address */
#define XCHAL_DATARAM1_SIZE        (0x7FFFFFFF  -0x2000) /* size in bytes */
#define XCHAL_DATARAM1_ECC_PARITY  0            /* ECC/parity type, 0=none */
#define XCHAL_DATARAM1_BANKS       2            /* number of banks */
#define XCHAL_HAVE_DATARAM1        1
#define XCHAL_DATARAM1_HAVE_IDMA   1            /* idma supported by this local memory */

//#define XCHAL_IDMA_NUM_CHANNELS 4

#endif
#if defined(XV_EMULATE_DMA) || (!defined(XCHAL_HAVE_VISION))
#define DECLARE_PS()
#endif
idma_status_t
idma_init(int32_t ch,
          unsigned init_flags,
          idma_max_block_t max_block_sz,
          unsigned max_pif_req,
          idma_ticks_cyc_t ticks_per_cyc,
          unsigned timeout_ticks,
          idma_err_callback_fn err_cb_func);



idma_status_t
idma_init_loop(int32_t ch,
               idma_buffer_t *buf,
               idma_type_t type,
               int32_t num_descs,
               void *cb_data,
               idma_callback_fn cb_func);

int32_t copy2d(int32_t ch,
               void *dst,
               void *src,
               size_t row_size,
               unsigned flags,
               unsigned num_rows,
               unsigned src_pitch,
               unsigned dst_pitch);

void dma_sleep(int32_t ch);
int32_t dma_desc_done(int32_t ch, int32_t index);


int32_t idma_copy_2d_pred_desc64_wide(int32_t ch,
                                      void *dst,
                                      void *src,
                                      size_t row_size,
                                      uint32_t flags,
                                      void* pred_mask,
                                      uint32_t num_rows,
                                      uint32_t src_pitch,
                                      uint32_t dst_pitch);


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
                                 uint32_t dst_tile_pitch);


#endif
