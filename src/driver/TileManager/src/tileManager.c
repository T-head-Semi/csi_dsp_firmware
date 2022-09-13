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

/**********************************************************************************
 * FILE:  tileManager.c
 *
 * DESCRIPTION:
 *
 *    This file contains Tile Manager implementation. It uses xvmem utility for
 *    buffer allocation and idma library for 2D data transfer.
 *
 *
 ********************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtensa/tie/xt_ivpn.h>
#ifdef __XTENSA__
#include <xtensa/xtensa-versions.h>
#endif
#include "tileManager.h"
#ifdef TM_LOG
xvTileManager * __pxvTM;
#endif


#define COPY_PADDING_DATA(pBuff, paddingVal, pixWidth, numBytes)              \
    {                                                                         \
        xb_vec2Nx8U dvec1, *pdvecDst = (xb_vec2Nx8U*) (pBuff);                  \
        valign vas1;                                                            \
        int wb;                                                                 \
        if (!(pixWidth & 1))                                                    \
        {                                                                       \
            xb_vecNx16U vec1 = paddingVal;                                        \
            dvec1 = IVP_MOV2NX8_FROMNX16(vec1);                                   \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            dvec1 = paddingVal;                                                   \
        }                                                                       \
        vas1 = IVP_ZALIGN();                                                    \
                                                                              \
        for (wb = numBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH))                 \
        {                                                                       \
            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);                           \
        }                                                                       \
        IVP_SAPOS2NX8U_FP(vas1, pdvecDst);                                      \
    }


/**********************************************************************************
 * FUNCTION: xvInitIdmaMultiChannel4CH()
 *
 * DESCRIPTION:
 *     Function to initialize iDMA library. Tile Manager uses iDMA library
 *     in fixed buffer mode. DMA transfer is scheduled as soon as the descriptor
 *     is added.
 *
 *
 * INPUTS:
 *     xvTileManager        *pxvTM              Tile Manager object
 *     idma_buffer_t        *buf0               iDMA library handle; contains descriptors and idma library object for channel0
 *     idma_buffer_t        *buf1               iDMA library handle; contains descriptors and idma library object for channel1. For single channel mode. buf1 can be NULL;.
 *     idma_buffer_t        *buf2               iDMA library handle; contains descriptors and idma library object for channel2  For single channel mode. buf2 can be NULL;.
 *     idma_buffer_t        *buf3               iDMA library handle; contains descriptors and idma library object for channel3  For single channel mode. buf3 can be NULL;.
 *     int32_t              numDescs            Number of descriptors that can be added in buffer
 *     int32_t              maxBlock            Maximum block size allowed
 *     int32_t              maxPifReq           Maximum number of outstanding pif requests
 *     idma_err_callback_fn errCallbackFunc0    Callback for dma transfer error for channel 0
 *     idma_err_callback_fn errCallbackFunc1    Callback for dma transfer error for channel 1
 *     idma_err_callback_fn errCallbackFunc2    Callback for dma transfer error for channel 2
 *     idma_err_callback_fn errCallbackFunc3    Callback for dma transfer error for channel 3
 *     idma_callback_fn     cbFunc0             Callback for dma transfer completion for channel 0
 *     void                 *cbData0            Data needed for completion callback function for channel 0
 *     idma_callback_fn     cbFunc1             Callback for dma transfer completion for channel 1
 *     void                 *cbData1            Data needed for completion callback function for channel 1
 *     idma_callback_fn     cbFunc2             Callback for dma transfer completion for channel 2
 *     void                 *cbData2            Data needed for completion callback function for channel 2
 *     idma_callback_fn     cbFunc3             Callback for dma transfer completion for channel 3
 *     void                 *cbData3            Data needed for completion callback function for channel 3
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvInitIdmaMultiChannel4CH(xvTileManager *pxvTM, idma_buffer_t *buf0, idma_buffer_t *buf1, idma_buffer_t *buf2, idma_buffer_t *buf3,
                                  int32_t numDescs, int32_t maxBlock, int32_t maxPifReq, idma_err_callback_fn errCallbackFunc0,
                                  idma_err_callback_fn errCallbackFunc1, idma_err_callback_fn errCallbackFunc2,  idma_err_callback_fn errCallbackFunc3,
                                  idma_callback_fn cbFunc0, void * cbData0, idma_callback_fn cbFunc1, void * cbData1,
                                  idma_callback_fn cbFunc2, void * cbData2, idma_callback_fn cbFunc3, void * cbData3)
{
    int32_t retVal;
    XV_CHECK_ERROR(pxvTM == NULL, ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((buf0 == NULL) && (buf1 == NULL) && (buf2 == NULL) && (buf3 == NULL), pxvTM->errFlag = XV_ERROR_BUFFER_NULL ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR(numDescs < 1, pxvTM->errFlag = XV_ERROR_BAD_ARG ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((maxBlock < MAX_BLOCK_2) || (maxBlock > MAX_BLOCK_16), pxvTM->errFlag = XV_ERROR_BAD_ARG ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");

    idma_ticks_cyc_t ticksPerCyc = TICK_CYCLES_2;
    int32_t timeoutTicks         = 0;
    int32_t initFlags            = 0;
    idma_type_t type             = IDMA_64B_DESC;

    if (buf0 != NULL)
    {
        retVal = idma_init(TM_IDMA_CH0, initFlags, maxBlock, maxPifReq, ticksPerCyc, timeoutTicks, errCallbackFunc0);
        XV_CHECK_ERROR(retVal != IDMA_OK, pxvTM->errFlag = XV_ERROR_DMA_INIT ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
        retVal = idma_init_loop(TM_IDMA_CH0, buf0, type, numDescs, cbData0, cbFunc0);
        // No error check on retVal as idma_init_loop() never return a non IDMA_OK value
    }

    if (buf1 != NULL)
    {
        retVal = idma_init(TM_IDMA_CH1, initFlags, maxBlock, maxPifReq, ticksPerCyc, timeoutTicks, errCallbackFunc1);
        XV_CHECK_ERROR(retVal != IDMA_OK, pxvTM->errFlag = XV_ERROR_DMA_INIT ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
        retVal = idma_init_loop(TM_IDMA_CH1, buf1, type, numDescs, cbData1, cbFunc1);
        // No error check on retVal as idma_init_loop() never return a non IDMA_OK value
    }

    if (buf2 != NULL)
    {
        retVal = idma_init(TM_IDMA_CH2, initFlags, maxBlock, maxPifReq, ticksPerCyc, timeoutTicks, errCallbackFunc2);
        XV_CHECK_ERROR(retVal != IDMA_OK, pxvTM->errFlag = XV_ERROR_DMA_INIT ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
        retVal = idma_init_loop(TM_IDMA_CH2, buf2, type, numDescs, cbData2, cbFunc2);
        // No error check on retVal as idma_init_loop() never return a non IDMA_OK value
    }

    if (buf3 != NULL)
    {
        retVal = idma_init(TM_IDMA_CH3, initFlags, maxBlock, maxPifReq, ticksPerCyc, timeoutTicks, errCallbackFunc3);
        XV_CHECK_ERROR(retVal != IDMA_OK, pxvTM->errFlag = XV_ERROR_DMA_INIT ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
        retVal = idma_init_loop(TM_IDMA_CH3, buf3, type, numDescs, cbData3, cbFunc3);
        // No error check on retVal as idma_init_loop() never return a non IDMA_OK value
    }
    return(XVTM_SUCCESS);
}



/**********************************************************************************
 * FUNCTION: xvInitTileManagerMultiChannel4CH()
 *
 * DESCRIPTION:
 *     Function to initialize Tile Manager. Resets Tile Manager's internal structure elements.
 *     Initializes Tile Manager's iDMA object.
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     idma_buffer_t *buf0, *buf1    iDMA object. It should be initialized before calling this function
 *                                   In case of singleChannel DMA, buf1 can be NULL
 *     idma_buffer_t *buf2, *buf3    iDMA object. It should be initialized before calling this function
 *                                   In case of singleChannel DMA, buf1 can be NULL
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvInitTileManagerMultiChannel4CH(xvTileManager *pxvTM, idma_buffer_t *buf0, idma_buffer_t *buf1,
                                         idma_buffer_t *buf2, idma_buffer_t *buf3)
{
    int32_t index;

    XV_CHECK_ERROR(pxvTM == NULL, ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pxvTM->errFlag       = XV_ERROR_SUCCESS;
    pxvTM->idmaErrorFlag[0] = XV_ERROR_SUCCESS;
    pxvTM->idmaErrorFlag[1] = XV_ERROR_SUCCESS;
    pxvTM->idmaErrorFlag[2] = XV_ERROR_SUCCESS;
    pxvTM->idmaErrorFlag[3] = XV_ERROR_SUCCESS;

    XV_CHECK_ERROR(buf0 == NULL, pxvTM->errFlag = XV_ERROR_BUFFER_NULL ; ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");

    // Initialize DMA related elements
    pxvTM->pdmaObj0            = buf0;
    pxvTM->pdmaObj1            = buf1;
    pxvTM->pdmaObj2            = buf2;
    pxvTM->pdmaObj3            = buf3;

    // Initialize IDMA state variables
    for (index = 0; index < MAX_NUM_CHANNEL; index++)
    {
        pxvTM->tileDMApendingCount[index] = 0;
        pxvTM->tileDMAstartIndex[index]   = 0;
    }

    // Initialize Memory banks related elements
    for (index = 0; index < MAX_NUM_MEM_BANKS; index++)
    {
        pxvTM->pMemBankStart[index] = NULL;
        pxvTM->memBankSize[index]   = 0;
    }

    // Reset tile related elements
    pxvTM->tileCount = 0;
    for (index = 0; index < ((MAX_NUM_TILES + 31) / 32); index++)
    {
        pxvTM->tileAllocFlags[index] = 0x00000000;
    }

    // Reset frame related elements
    pxvTM->frameCount = 0;
    for (index = 0; index < ((MAX_NUM_FRAMES + 31) / 32); index++)
    {
        pxvTM->frameAllocFlags[index] = 0x00000000;
    }


#if (SUPPORT_3D_TILES == 1)
    // Reset tile3D related elements
    pxvTM->tile3DCount = 0;
    for (index = 0; index < ((MAX_NUM_TILES3D + 31) / 32); index++)
    {
        pxvTM->tile3DAllocFlags[index] = 0x00000000;
    }

    // Reset frame3D related elements
    pxvTM->frame3DCount = 0;
    for (index = 0; index < ((MAX_NUM_FRAMES3D + 31) / 32); index++)
    {
        pxvTM->frame3DAllocFlags[index] = 0x00000000;
    }
    // Initialize IDMA state variables
    for (index = 0; index < MAX_NUM_CHANNEL; index++)
    {
        pxvTM->tile3DDMApendingCount[index] = 0;
        pxvTM->tile3DDMAstartIndex[index]   = 0;
    }

#endif // (SUPPORT_3D_TILES == 1)

#ifdef TM_LOG
  __pxvTM = pxvTM;
  __pxvTM->tm_log_fp = fopen(TILE_MANAGER_LOG_FILE_NAME, "w");
  if (__pxvTM->tm_log_fp == NULL)
  {
    __pxvTM->errFlag = XV_ERROR_FILE_OPEN;
    return(XVTM_ERROR);
  }
#endif



    return(XVTM_SUCCESS);
}


/**********************************************************************************
 * FUNCTION: xvResetTileManager()
 *
 * DESCRIPTION:
 *     Function to reset Tile Manager. It closes the log file,
 *     releases the buffers and resets the Tile Manager object
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvResetTileManager(xvTileManager *pxvTM)
{
	XV_CHECK_ERROR(pxvTM == NULL, ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
  // Close the Tile Manager logging file
#ifdef TM_LOG
  if (__pxvTM != NULL)
  {
    if (__pxvTM->tm_log_fp)
    {
      fclose(__pxvTM->tm_log_fp);
    }
  }
#endif
    
    // Free all the xvmem allocated buffers
    //Type cast to void to avoid MISRA 17.7 violation
    (void) xvFreeAllBuffers(pxvTM);

    // Resetting the Tile Manager pointer.
    // This will free all allocated tiles and buffers.
    // It will not reset dma object.
    //Type cast to void to avoid MISRA 17.7 violation
    (void) memset(pxvTM, 0, sizeof(xvTileManager));
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvInitMemAllocator()
 *
 * DESCRIPTION:
 *     Function to initialize memory allocator. Tile Manager uses xvmem utility as memory allocator.
 *     It takes array of pointers to memory pool's start addresses and respective sizes as input and
 *     uses the memory pools when memory is allocated.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     int32_t numMemBanks       Number of memory pools
 *     void **pBankBuffPool      Array of memory pool start address
 *     int32_t* buffPoolSize     Array of memory pool sizes
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvInitMemAllocator(xvTileManager *pxvTM, int32_t numMemBanks, void **pBankBuffPool, int32_t* buffPoolSize)
{
    int32_t indx, retVal;
    XV_CHECK_ERROR(pxvTM == NULL, ,  XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pxvTM->errFlag = XV_ERROR_SUCCESS;

    XV_CHECK_ERROR((numMemBanks <= 0) || (numMemBanks > MAX_NUM_MEM_BANKS), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((pBankBuffPool == NULL) || (buffPoolSize == NULL), pxvTM->errFlag = XV_ERROR_POINTER_NULL;,  XVTM_ERROR, "XV_ERROR_BAD_ARG");

    xvmem_mgr_t *pmemBankMgr;
    pxvTM->numMemBanks = numMemBanks;
#ifdef FIK_FRAMEWORK
	pxvTM->allocationsIdx = 0;
#endif
    for (indx = 0; indx < numMemBanks; indx++)
    {
        if ((pBankBuffPool[indx] == NULL) || (buffPoolSize[indx] <= 0))
        {
            pxvTM->errFlag = XV_ERROR_BAD_ARG;
            return(XVTM_ERROR);
        }
        if (!((XV_PTR_START_IN_DRAM(pBankBuffPool[indx])) && (XV_PTR_END_IN_DRAM((((uint8_t *) pBankBuffPool[indx]) + buffPoolSize[indx])))))
        {
            pxvTM->errFlag = XV_ERROR_BAD_ARG;
            return(XVTM_ERROR);
        }
        pmemBankMgr                = &(pxvTM->memBankMgr[indx]);
        pxvTM->pMemBankStart[indx] = pBankBuffPool[indx];
        pxvTM->memBankSize[indx]   = buffPoolSize[indx];
        retVal                     = xvmem_init(pmemBankMgr, pBankBuffPool[indx], buffPoolSize[indx], 0, NULL);
        if (retVal != XVMEM_OK)
        {
            pxvTM->errFlag = XV_ERROR_XVMEM_INIT;
            return(XVTM_ERROR);
        }
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvAllocateBuffer()
 *
 * DESCRIPTION:
 *     Allocates buffer from the given buffer pool. It returns aligned buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     int32_t buffSize          Size of the requested buffer
 *     int32_t buffColor         Color/index of requested bufffer
 *     int32_t buffAlignment     Alignment of requested buffer
 *
 * OUTPUTS:
 *     Returns the buffer with requested parameters. If an error occurs, returns ((void *)(XVTM_ERROR))
 *
 ********************************************************************************** */

void *xvAllocateBuffer(xvTileManager *pxvTM, int32_t buffSize, int32_t buffColor, int32_t buffAlignment)
{
    void *buffOut = NULL;
    int32_t currColor;
    xvmem_status_t errCode;

    XV_CHECK_ERROR(pxvTM == NULL,  , (void*)XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    XV_CHECK_ERROR((buffSize <= 0), pxvTM->errFlag = XV_ERROR_BAD_ARG ; , ((void *) XVTM_ERROR), "XV_ERROR_BAD_ARG");
	XV_CHECK_ERROR((buffAlignment <= 0), pxvTM->errFlag = XV_ERROR_ALLOC_FAILED ; , ((void *) XVTM_ERROR), "XV_ERROR_ALLOC_FAILED");
	XV_CHECK_ERROR((!(((buffColor >= 0) && (buffColor < pxvTM->numMemBanks)) || (buffColor == XV_MEM_BANK_COLOR_ANY))), pxvTM->errFlag = XV_ERROR_BAD_ARG ; , ((void *) XVTM_ERROR), "XV_ERROR_ALLOC_FAILED");

    // if the color is XV_MEM_BANK_COLOR_ANY, loop through all the buffers and pick one that meets all criteria
    if (buffColor == XV_MEM_BANK_COLOR_ANY)
    {
        currColor = 0;
        while ((buffOut == NULL) && (currColor < pxvTM->numMemBanks))
        {
            buffOut = xvmem_alloc(&(pxvTM->memBankMgr[currColor]), buffSize, buffAlignment, &errCode);
            currColor++;
        }
    }
    else
    {
        buffOut = xvmem_alloc(&(pxvTM->memBankMgr[buffColor]), buffSize, buffAlignment, &errCode);
    }

    if (buffOut == NULL)
    {
        pxvTM->errFlag = XV_ERROR_ALLOC_FAILED;
        return((void *) XVTM_ERROR);
    }
#ifdef FIK_FRAMEWORK
	pxvTM->allocatedList[pxvTM->allocationsIdx] = buffOut;
	pxvTM->allocationsIdx++;
	pxvTM->allocationsIdx = pxvTM->allocationsIdx % XV_MAX_ALLOCATIONS;
#endif
    return(buffOut);
}

/**********************************************************************************
 * FUNCTION: xvFreeBuffer()
 *
 * DESCRIPTION:
 *     Releases the given buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     void          *pBuff      Buffer that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeBuffer(xvTileManager *pxvTM, void const *pBuff)
{
    int32_t index;
	XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    pxvTM->errFlag = XV_ERROR_SUCCESS;
   	XV_CHECK_ERROR(pBuff == NULL, pxvTM->errFlag = XV_ERROR_BUFFER_NULL;  , XVTM_ERROR, "XV_ERROR_BUFFER_NULL");

    for (index = 0; index < pxvTM->numMemBanks; index++)
    {
        if ((pxvTM->pMemBankStart[index] <= pBuff) && (pBuff < (void *) (&((uint8_t *) pxvTM->pMemBankStart[index])[pxvTM->memBankSize[index]])))
        {
            xvmem_free(&(pxvTM->memBankMgr[index]), pBuff);
            return(XVTM_SUCCESS);
        }
    }
    pxvTM->errFlag = XV_ERROR_BAD_ARG;
    return(XVTM_ERROR);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllBuffers()
 *
 * DESCRIPTION:
 *     Releases all buffers. Reinitializes the memory allocator
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllBuffers(xvTileManager *pxvTM)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    int32_t bankIndex, buffPoolSize;
    xvmem_mgr_t *pmemBankMgr;
    void *pBankBuffPool;

    // Resetting all memory manager related elements
    for (bankIndex = 0; bankIndex < pxvTM->numMemBanks; bankIndex++)
    {
        pmemBankMgr   = &(pxvTM->memBankMgr[bankIndex]);
        pBankBuffPool = pxvTM->pMemBankStart[bankIndex];
        buffPoolSize  = pxvTM->memBankSize[bankIndex];
        //Type cast to void to avoid MISRA 17.7 violation
        (void) xvmem_init(pmemBankMgr, pBankBuffPool, buffPoolSize, 0, NULL);
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvAllocateFrame()
 *
 * DESCRIPTION:
 *     Allocates single frame. It does not allocate buffer required for frame data.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns the pointer to allocated frame.
 *     Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate frame data buffer.
 *
 ********************************************************************************** */

xvFrame *xvAllocateFrame(xvTileManager *pxvTM)
{
    int32_t indx, indxArr, indxShift, allocFlags;
    xvFrame *pFrame = NULL;

    XV_CHECK_ERROR(pxvTM == NULL,   , (xvFrame *) ((void *) (XVTM_ERROR)), "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    for (indx = 0; indx < MAX_NUM_FRAMES; indx++)
    {
        indxArr    = indx >> 5;
        indxShift  = indx & 0x1F;
        allocFlags = pxvTM->frameAllocFlags[indxArr];
        if (((allocFlags >> indxShift) & 0x1) == 0)
        {
            break;
        }
    }

    if (indx < MAX_NUM_FRAMES)
    {
        pFrame                          = &(pxvTM->frameArray[indx]);
        pxvTM->frameAllocFlags[indxArr] = allocFlags | (0x1 << indxShift);
        pxvTM->frameCount++;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_FRAME_BUFFER_FULL;
        return((xvFrame *) ((void *) XVTM_ERROR));
    }

    return(pFrame);
}

/**********************************************************************************
 * FUNCTION: xvFreeFrame()
 *
 * DESCRIPTION:
 *     Releases the given frame. Does not release associated frame data buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     xvFrame       *pFrame     Frame that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeFrame(xvTileManager *pxvTM, xvFrame const *pFrame)
{
    int32_t indx, indxArr, indxShift, allocFlags;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    pxvTM->errFlag = XV_ERROR_SUCCESS;
	XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL ;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    for (indx = 0; indx < MAX_NUM_FRAMES; indx++)
    {
        if (&(pxvTM->frameArray[indx]) == pFrame)
        {
            break;
        }
    }

    if (indx < MAX_NUM_FRAMES)
    {
        indxArr                         = indx >> 5;
        indxShift                       = indx & 0x1F;
        allocFlags                      = pxvTM->frameAllocFlags[indxArr];
        pxvTM->frameAllocFlags[indxArr] = allocFlags & ~(0x1 << indxShift);
        pxvTM->frameCount--;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_BAD_ARG;
        return(XVTM_ERROR);
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllFrames()
 *
 * DESCRIPTION:
 *     Releases all allocated frames.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllFrames(xvTileManager *pxvTM)
{
    int32_t index;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    pxvTM->frameCount = 0;
    for (index = 0; index < ((MAX_NUM_FRAMES + 31) / 32); index++)
    {
        pxvTM->frameAllocFlags[index] = 0x00000000;
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvAllocateTile()
 *
 * DESCRIPTION:
 *     Allocates single tile. It does not allocate buffer required for tile data.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns the pointer to allocated tile.
 *     Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate tile data buffer
 *
 ********************************************************************************** */

xvTile *xvAllocateTile(xvTileManager *pxvTM)
{
    int32_t indx, indxArr, indxShift, allocFlags;
    xvTile *pTile = NULL;
    XV_CHECK_ERROR(pxvTM == NULL,   , (xvTile *) ((void *) XVTM_ERROR), "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    for (indx = 0; indx < MAX_NUM_TILES; indx++)
    {
        indxArr    = indx >> 5;
        indxShift  = indx & 0x1F;
        allocFlags = pxvTM->tileAllocFlags[indxArr];
        if (((allocFlags >> indxShift) & 0x1) == 0)
        {
            break;
        }
    }

    if (indx < MAX_NUM_TILES)
    {
        pTile                          = &(pxvTM->tileArray[indx]);
        pxvTM->tileAllocFlags[indxArr] = allocFlags | (0x1 << indxShift);
        pxvTM->tileCount++;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_TILE_BUFFER_FULL;
        return((xvTile *) ((void *) XVTM_ERROR));
    }

    return(pTile);
}

/**********************************************************************************
 * FUNCTION: xvFreeTile()
 *
 * DESCRIPTION:
 *     Releases the given tile. Does not release associated tile data buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     xvTile        *pTile      Tile that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */
int32_t xvFreeTile(xvTileManager *pxvTM, xvTile const *pTile)
{
    int32_t indx, indxArr, indxShift, allocFlags;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");


    pxvTM->errFlag = XV_ERROR_SUCCESS;
    if (pTile == NULL)
    {
        pxvTM->errFlag = XV_ERROR_TILE_NULL;
        return(XVTM_ERROR);
    }

    for (indx = 0; indx < MAX_NUM_TILES; indx++)
    {
        if (&(pxvTM->tileArray[indx]) == pTile)
        {
            break;
        }
    }

    if (indx < MAX_NUM_TILES)
    {
        indxArr                        = indx >> 5;
        indxShift                      = indx & 0x1F;
        allocFlags                     = pxvTM->tileAllocFlags[indxArr];
        pxvTM->tileAllocFlags[indxArr] = allocFlags & ~(0x1 << indxShift);
        pxvTM->tileCount--;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_BAD_ARG;
        return(XVTM_ERROR);
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllTiles()
 *
 * DESCRIPTION:
 *     Releases all allocated tiles.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllTiles(xvTileManager *pxvTM)
{
    int32_t index;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    pxvTM->tileCount = 0;
    for (index = 0; index < ((MAX_NUM_TILES + 31) / 32); index++)
    {
        pxvTM->tileAllocFlags[index] = 0x00000000;
    }
    return(XVTM_SUCCESS);
}



/**********************************************************************************
 * FUNCTION: xvCreateTileManagerMultiChannel4CH()
 *
 * DESCRIPTION:
 *     Creates and initializes Tile Manager, Memory Allocator and iDMA.
 *
 * INPUTS:
 *     xvTileManager        *pxvTM              Tile Manager object
 *     idma_buffer_t        *buf0               iDMA object. It should be initialized before calling this function. Contains descriptors and idma library object for channel0
 *     idma_buffer_t        *buf1               iDMA object. It should be initialized before calling this function. Contains descriptors and idma library object for channel1. For single channel mode. buf1 can be NULL;.
 *     idma_buffer_t        *buf2               iDMA object. It should be initialized before calling this function. Contains descriptors and idma library object for channel2  For single channel mode. buf2 can be NULL;.
 *     idma_buffer_t        *buf3               iDMA object. It should be initialized before calling this function. Contains descriptors and idma library object for channel3. For single channel mode. buf3 can be NULL;.
 *     int32_t              numMemBanks         Number of memory pools
 *     void                 **pBankBuffPool     Array of memory pool start address
 *     int32_t              *buffPoolSize       Array of memory pool sizes
 *     int32_t              numDescs            Number of descriptors that can be added in buffer
 *     int32_t              maxBlock            Maximum block size allowed
 *     int32_t              maxPifReq           Maximum number of outstanding pif requests
 *     idma_err_callback_fn errCallbackFunc0    Callback for dma transfer error for channel 0
 *     idma_err_callback_fn errCallbackFunc1    Callback for dma transfer error for channel 1
 *     idma_err_callback_fn errCallbackFunc2    Callback for dma transfer error for channel 2
 *     idma_err_callback_fn errCallbackFunc3    Callback for dma transfer error for channel 3
 *     idma_callback_fn     cbFunc0             Callback for dma transfer completion for channel 0
 *     void                 *cbData0            Data needed for completion callback function for channel 0
 *     idma_callback_fn     cbFunc1             Callback for dma transfer completion for channel 1
 *     void                 *cbData1            Data needed for completion callback function for channel 1
 *     idma_callback_fn     cbFunc2             Callback for dma transfer completion for channel 2
 *     void                 *cbData2            Data needed for completion callback function for channel 2
 *     idma_callback_fn     cbFunc3             Callback for dma transfer completion for channel 3
 *     void                 *cbData3            Data needed for completion callback function for channel 3
 *
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvCreateTileManagerMultiChannel4CH(xvTileManager *pxvTM, void *buf0, void *buf1, void *buf2, void *buf3, int32_t numMemBanks, void **pBankBuffPool, int32_t* buffPoolSize,
                                           idma_err_callback_fn errCallbackFunc0, idma_err_callback_fn errCallbackFunc1,
                                           idma_err_callback_fn errCallbackFunc2, idma_err_callback_fn errCallbackFunc3,
                                           idma_callback_fn intrCallbackFunc0, void *cbData0, idma_callback_fn intrCallbackFunc1, void *cbData1,
                                           idma_callback_fn intrCallbackFunc2, void *cbData2, idma_callback_fn intrCallbackFunc3, void *cbData3,
                                           int32_t descCount, int32_t maxBlock, int32_t numOutReq)
{
    int32_t retVal;

#if (XCHAL_IDMA_NUM_CHANNELS==2)
    retVal = xvInitTileManagerMultiChannel(pxvTM, (idma_buffer_t *) buf0, (idma_buffer_t *) buf1);
#else
    retVal = xvInitTileManagerMultiChannel4CH(pxvTM, (idma_buffer_t *) buf0, (idma_buffer_t *) buf1,
                                              (idma_buffer_t *) buf2, (idma_buffer_t *) buf3);
#endif
    XV_CHECK_ERROR(retVal == XVTM_ERROR,   , XVTM_ERROR, "XVTM_ERROR");


    retVal = xvInitMemAllocator(pxvTM, numMemBanks, pBankBuffPool, buffPoolSize);
   	XV_CHECK_ERROR(retVal == XVTM_ERROR,   , XVTM_ERROR, "XVTM_ERROR");

#if (XCHAL_IDMA_NUM_CHANNELS==2)
    retVal = xvInitIdmaMultiChannel(pxvTM, (idma_buffer_t *) buf0, (idma_buffer_t *) buf1, descCount, maxBlock, numOutReq, errCallbackFunc0, errCallbackFunc1, intrCallbackFunc0, cbData0, intrCallbackFunc1, cbData1);
#else
    retVal = xvInitIdmaMultiChannel4CH(pxvTM, (idma_buffer_t *)buf0, (idma_buffer_t *)buf1, (idma_buffer_t *)buf2, (idma_buffer_t *)buf3,
                                       descCount, maxBlock, numOutReq, errCallbackFunc0, errCallbackFunc1,
                                       errCallbackFunc2, errCallbackFunc3, intrCallbackFunc0,  cbData0, intrCallbackFunc1, cbData1,
                                       intrCallbackFunc2, cbData2, intrCallbackFunc3, cbData3);

#endif
    XV_CHECK_ERROR(retVal == XVTM_ERROR,   , XVTM_ERROR, "XVTM_ERROR");

    return(retVal);
}

/**********************************************************************************
 * FUNCTION: xvCreateFrame()
 *
 * DESCRIPTION:
 *     Allocates single frame. It does not allocate buffer required for frame data.
 *     Initializes the frame elements
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     void          *imgBuff        Pointer to image buffer
 *     uint32_t       frameBuffSize   Size of allocated image buffer
 *     int32_t       width           Width of image
 *     int32_t       height          Height of image
 *     int32_t       pitch           Pitch of image
 *     uint8_t       pixRes          Pixel resolution of image in bytes
 *     uint8_t       numChannels     Number of channels in the image
 *     uint8_t       paddingtype     Supported padding type
 *     uint32_t       paddingVal      Padding value if padding type is edge extension
 *
 * OUTPUTS:
 *     Returns the pointer to allocated frame.
 *     Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate frame data buffer.
 *
 ********************************************************************************** */

xvFrame *xvCreateFrame(xvTileManager *pxvTM, uint64_t imgBuff, uint32_t frameBuffSize, int32_t width, int32_t height, int32_t pitch, uint8_t pixRes, uint8_t numChannels, uint8_t paddingtype, uint32_t paddingVal)
{
 	XV_CHECK_ERROR(((pxvTM == NULL) || (imgBuff == NULL)),  ,  ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR(((width < 0) || (height < 0) || (pitch < 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG ;,  ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR(((width * numChannels) > pitch), pxvTM->errFlag = XV_ERROR_BAD_ARG; ,  ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR((frameBuffSize < (pitch * height * pixRes)), pxvTM->errFlag = XV_ERROR_BAD_ARG; ,  ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR(((numChannels > MAX_NUM_CHANNEL) || (numChannels <= 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG; ,  ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR((paddingtype > FRAME_PADDING_MAX), pxvTM->errFlag = XV_ERROR_BAD_ARG; ,  ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");

    xvFrame *pFrame = xvAllocateFrame(pxvTM);

    if ((void *) pFrame == (void *) XVTM_ERROR)
    {
        pxvTM->errFlag = XV_ERROR_BAD_ARG;
        return((xvFrame *) ((void *) XVTM_ERROR));
    }

    SETUP_FRAME(pFrame, imgBuff, frameBuffSize, width, height, pitch, 0, 0, pixRes, numChannels, paddingtype, paddingVal);
    return(pFrame);
}

/**********************************************************************************
 * FUNCTION: xvCreateTile()
 *
 * DESCRIPTION:
 *     Allocates single tile and associated buffer data.
 *     Initializes the elements in tile
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     int32_t       tileBuffSize    Size of allocated tile buffer
 *     int32_t       width           Width of tile
 *     uint16_t       height          Height of tile
 *     int32_t       pitch           Pitch of tile
 *     uint16_t       edgeWidth       Edge width of tile
 *     uint16_t       edgeHeight      Edge height of tile
 *     int32_t       color           Memory pool from which the buffer should be allocated
 *     xvFrame       *pFrame         Frame associated with the tile
 *     uint16_t       tileType        Type of tile
 *     int32_t       alignType       Alignment tpye of tile. could be edge aligned of data aligned
 *
 * OUTPUTS:
 *     Returns the pointer to allocated tile.
 *     Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
 *
 ********************************************************************************** */

xvTile *xvCreateTile(xvTileManager *pxvTM, int32_t tileBuffSize, int32_t width, uint16_t height, int32_t pitch, uint16_t edgeWidth, uint16_t edgeHeight, int32_t color, xvFrame *pFrame, uint16_t xvTileType, int32_t alignType)
{
	XV_CHECK_ERROR(pxvTM == NULL, , (xvTile *) ((void *) XVTM_ERROR), "NULL TM Pointer");
	XV_CHECK_ERROR(((width < 0) || (pitch < 0) ), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR(((alignType != EDGE_ALIGNED_32) && (alignType != DATA_ALIGNED_32) && (alignType != EDGE_ALIGNED_64) && (alignType != DATA_ALIGNED_64)), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR((((color < 0) || (color >= pxvTM->numMemBanks)) && (color != XV_MEM_BANK_COLOR_ANY)), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");

    int32_t channel = XV_TYPE_CHANNELS(xvTileType);
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(xvTileType);
    int32_t bytePerPel;
	bytePerPel = bytesPerPix / channel;

	XV_CHECK_ERROR((((width + (2 * edgeWidth)) * channel) > pitch), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	XV_CHECK_ERROR((tileBuffSize < (pitch * (height + (2 * edgeHeight)) * bytePerPel)), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");

	if (pFrame != NULL) {
		XV_CHECK_ERROR((pFrame->pixelRes != bytePerPel), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
		XV_CHECK_ERROR((pFrame->numChannels != channel), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
	}
    void *tileBuff = NULL;
    tileBuff = xvAllocateBuffer(pxvTM, tileBuffSize, color, 64);

    if (tileBuff == (void *) XVTM_ERROR)
    {
        return((xvTile *) ((void *) XVTM_ERROR));
    }

    xvTile *pTile = xvAllocateTile(pxvTM);
    if ((void *) pTile == (void *) XVTM_ERROR)
    {
        //Type cast to void to avoid MISRA 17.7 violation
        (void) xvFreeBuffer(pxvTM, tileBuff);
        return((xvTile *) ((void *) XVTM_ERROR));
    }

    SETUP_TILE(pTile, tileBuff, tileBuffSize, pFrame, width, height, pitch, xvTileType, edgeWidth, edgeHeight, 0, 0, alignType);
    return(pTile);
}

/**********************************************************************************
 * FUNCTION: xvAddIdmaRequestMultiChannel_predicated_wide()
 *
 * DESCRIPTION:
 *     Add iDMA transfer request. Request is scheduled as soon as it is added
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     void          * dst                    Pointer to destination buffer
 *     void          *src                     Pointer to source buffer
 *     size_t        rowSize                  Number of bytes to transfer in a row
 *     int32_t       numRows                  Number of rows to transfer
 *     int32_t       srcPitch                 Source buffer's pitch in bytes
 *     int32_t       dstPitch                 Destination buffer's pitch in bytes
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *     int32_t       *pred_mask               predication mask pointer
 * OUTPUTS:
 *     Returns dmaIndex for this request. It returns XVTM_ERROR if it encounters an error
 *
 ********************************************************************************** */

int32_t xvAddIdmaRequestMultiChannel_predicated_wide(int32_t dmaChannel, xvTileManager *pxvTM, uint64_t pdst64, uint64_t psrc64, size_t rowSize,
                                                     int32_t numRows, int32_t srcPitch, int32_t dstPitch, int32_t interruptOnCompletion, uint32_t  *pred_mask)
{
	XV_CHECK_ERROR(pxvTM == NULL, , (XVTM_ERROR), "NULL TM Pointer");

    pxvTM->errFlag = XV_ERROR_SUCCESS;
	XV_CHECK_ERROR(((((void *) (uint32_t) pdst64) == NULL) || (((void *) (uint32_t) psrc64) == NULL)), pxvTM->errFlag = XV_ERROR_POINTER_NULL;,  XVTM_ERROR, "XVTM_ERROR");

  	XV_CHECK_ERROR(((rowSize <= 0) || (numRows <= 0) || (srcPitch < 0) || (dstPitch < 0) || (interruptOnCompletion < 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  XVTM_ERROR, "XVTM_ERROR");
	XV_CHECK_ERROR(( ((rowSize > srcPitch)&& (srcPitch != 0)) || ((rowSize > dstPitch) && (dstPitch != 0))), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  XVTM_ERROR, "XVTM_ERROR");
	XV_CHECK_ERROR((!(((psrc64 <= 0x00000000ffffffff) && XV_PTR_START_IN_DRAM((uint8_t *)((uint32_t)(psrc64 & 0x00000000ffffffff))) && XV_PTR_END_IN_DRAM(((uint8_t *)((uint32_t)(psrc64 & 0x00000000ffffffff))) + (srcPitch * numRows))) || \
          ((pdst64 <= 0x00000000ffffffff) && XV_PTR_START_IN_DRAM((uint8_t *) ((uint32_t) (pdst64 & 0x00000000ffffffff))) && XV_PTR_END_IN_DRAM(((uint8_t *) ((uint32_t) (pdst64 & 0x00000000ffffffff))) + (dstPitch * numRows))))), pxvTM->errFlag = XV_ERROR_BAD_ARG;,  XVTM_ERROR, "XVTM_ERROR");

    if (pred_mask != NULL)
    {
        //predicated buffer needs to be in local memory
        if (!( (XV_PTR_START_IN_DRAM((uint8_t*)pred_mask)) && (XV_PTR_END_IN_DRAM( ((uint8_t*)pred_mask) + ((numRows + 7) >> 3 )))))
        {
            pxvTM->errFlag = XV_ERROR_BAD_ARG;
            return(XVTM_ERROR);
        }
    }
    int32_t dmaIndex;
    uint32_t intrCompletionFlag;

    if (interruptOnCompletion != 0)
    {
        intrCompletionFlag = DESC_NOTIFY_W_INT;
    }
    else
    {
        intrCompletionFlag = 0;
    }

    if (pred_mask==NULL)
    {
        TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
            __LINE__, psrc64, pdst64, rowSize, numRows, srcPitch, dstPitch, intrCompletionFlag);
        dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &pdst64, &psrc64, rowSize, intrCompletionFlag, numRows, srcPitch, dstPitch);
        TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);

    }
    else
    {
        TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  pred_buffer: %8.8x, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                __LINE__, psrc64, pdst64, (uintptr_t) pred_mask, rowSize, numRows, srcPitch, dstPitch, intrCompletionFlag);        
        dmaIndex = idma_copy_2d_pred_desc64_wide(dmaChannel, &pdst64, &psrc64, rowSize, intrCompletionFlag, pred_mask, numRows, srcPitch, dstPitch);
        TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);

    }
    //dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, dst, src, rowSize, intrCompletionFlag, numRows, srcPitch, dstPitch);
    return(dmaIndex);
}


int32_t xvAddIdmaRequestMultiChannel_wide(int32_t dmaChannel, xvTileManager *pxvTM, uint64_t pdst64, uint64_t psrc64, size_t rowSize,
                                              int32_t numRows, int32_t srcPitchBytes, int32_t dstPitchBytes, int32_t interruptOnCompletion)
{
    int32_t dmaIndex;
    uint32_t intrCompletionFlag;

    if (interruptOnCompletion != 0)
    {
        intrCompletionFlag = DESC_NOTIFY_W_INT;
    }
    else
    {
        intrCompletionFlag = 0;
    }
        
    TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
        __LINE__, psrc64, pdst64, rowSize, numRows, srcPitchBytes, dstPitchBytes, intrCompletionFlag);
    dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &pdst64, &psrc64, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
    TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);
   
    //dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, dst, src, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
    return(dmaIndex);
}


// Made it inline to speed up xvReqTileTransferIn and xvReqTileTransferOut APIs
static inline int32_t addIdmaRequestInlineMultiChannel_wide(int32_t dmaChannel, xvTileManager *pxvTM, uint64_t pdst64, uint64_t psrc64, size_t rowSize,
                                              int32_t numRows, int32_t srcPitchBytes, int32_t dstPitchBytes, int32_t interruptOnCompletion)
{
    int32_t dmaIndex;
    uint32_t intrCompletionFlag;

    if (interruptOnCompletion != 0)
    {
        intrCompletionFlag = DESC_NOTIFY_W_INT;
    }
    else
    {
        intrCompletionFlag = 0;
    }

    TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
        __LINE__, psrc64, pdst64, rowSize, numRows, srcPitchBytes, dstPitchBytes, intrCompletionFlag);
    dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &pdst64, &psrc64, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
    TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);

    //dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, dst, src, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
    return(dmaIndex);
}

// Made it inline to speed up xvReqTileTransferIn and xvReqTileTransferOut APIs
int32_t addIdmaRequestMultiChannel_wide3D(int32_t dmaChannel, xvTileManager *pxvTM, uint64_t pdst64, uint64_t psrc64, size_t rowSize,
                                                int32_t numRows, int32_t srcPitchBytes, int32_t dstPitchBytes, int32_t interruptOnCompletion, int32_t srcTilePitchBytes,
                                                int32_t dstTilePitchBytes, int32_t numTiles)
{
    int32_t dmaIndex;
    uint32_t intrCompletionFlag;

    if (interruptOnCompletion != 0)
    {
        intrCompletionFlag = DESC_NOTIFY_W_INT;
    }
    else
    {
        intrCompletionFlag = 0;
    }

    TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, numTiles: %d, srcPitch: %d, dstPitch: %d, source2DTilePitch: %d, dst2DTilePitch: %d, flags: 0x%x, ",
        __LINE__, psrc64, pdst64, rowSize, numRows, numTiles, srcPitchBytes, dstPitchBytes, srcTilePitchBytes, dstTilePitchBytes, intrCompletionFlag);

    dmaIndex = idma_copy_3d_desc64_wide(dmaChannel, &pdst64, &psrc64, intrCompletionFlag, rowSize, numRows, numTiles, srcPitchBytes, dstPitchBytes,
                                        srcTilePitchBytes, dstTilePitchBytes);
    TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);

    return(dmaIndex);
}


// Made it inline to speed up xvReqTileTransferIn and xvReqTileTransferOut APIs
static inline int32_t addIdmaRequestInlineMultiChannel_predicated_wide(int32_t dmaChannel, xvTileManager *pxvTM, uint64_t pdst64, uint64_t psrc64, size_t rowSize,
                                                         int32_t numRows, int32_t srcPitchBytes, int32_t dstPitchBytes, int32_t interruptOnCompletion, uint32_t* pred_mask)
{
    int32_t dmaIndex;
    uint32_t intrCompletionFlag;

    if (interruptOnCompletion != 0)
    {
        intrCompletionFlag = DESC_NOTIFY_W_INT;
    }
    else
    {
        intrCompletionFlag = 0;
    }

    TM_LOG_PRINT("line=%d, src: %llx, dst: %llx,  %8.8x, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
        __LINE__, psrc64, pdst64, (uintptr_t ) pred_mask, rowSize, numRows, srcPitchBytes, dstPitchBytes, intrCompletionFlag);

    dmaIndex = idma_copy_2d_pred_desc64_wide(dmaChannel, &pdst64, &psrc64, rowSize, intrCompletionFlag, pred_mask, numRows, srcPitchBytes, dstPitchBytes);
    TM_LOG_PRINT("line=%d, dmaIndex: %d\n", __LINE__, dmaIndex);
    return(dmaIndex);
}


/**********************************************************************************
 * FUNCTION: xvAddIdmaRequestMultiChannel()
 *
 * DESCRIPTION:
 *     Add iDMA transfer request. Request is scheduled as soon as it is added
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     void          *dst                    Pointer to destination buffer
 *     void          *src                     Pointer to source buffer
 *     size_t        rowSize                  Number of bytes to transfer in a row
 *     int32_t       numRows                  Number of rows to transfer
 *     int32_t       srcPitch                 Source buffer's pitch in bytes
 *     int32_t       dstPitch                 Destination buffer's pitch in bytes
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 * OUTPUTS:
 *     Returns dmaIndex for this request. It returns XVTM_ERROR if it encounters an error
 *
 ********************************************************************************** */

int32_t xvAddIdmaRequestMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, void* pdst, void*  psrc, size_t rowSize,
                                     int32_t numRows, int32_t srcPitch, int32_t dstPitch, int32_t interruptOnCompletion)
{
    uint64_t psrc64, pdst64;
    psrc64 = (uint64_t) (uint32_t) psrc;
    pdst64 = (uint64_t) (uint32_t) pdst;
    int32_t dmaIndex;
  
    TM_LOG_PRINT("line=%d, src: %llx, dst: %llx, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
        __LINE__, psrc64, pdst64, rowSize, numRows, srcPitch, dstPitch, interruptOnCompletion);

    dmaIndex = xvAddIdmaRequestMultiChannel_wide(dmaChannel, pxvTM, pdst64, psrc64, rowSize, numRows, srcPitch, dstPitch, interruptOnCompletion);

    TM_LOG_PRINT("line=%d, dmaIndex: %d\n", __LINE__, dmaIndex);
    return(dmaIndex);

}


// Part of tile reuse. Checks X direction boundary condition and performs DMA transfers
uint32_t solveForX(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, uint8_t *pCurrBuff, uint8_t *pPrevBuff,
                   int32_t y1, int32_t y2, int32_t x1, int32_t x2, int32_t px1, int32_t px2, int32_t tp, int32_t ptp, int32_t interruptOnCompletion)
{
    uint8_t pixWidth;
    int32_t dmaIndex, framePitchBytes, bytes, bytesCopy;

    uint64_t pSrc64, pDst64;
    uint8_t *pDst;
    uint8_t *pSrcCopy, *pDstCopy;

    dmaIndex = 0;
    xvFrame *pFrame = pTile->pFrame;
    pixWidth        = pFrame->pixelRes * pFrame->numChannels;
    framePitchBytes = pFrame->framePitch * pFrame->pixelRes;

    // Case 1. Only left most part overlaps
    if ((px1 <= x1) && (px2 < x2))
    {
        pSrcCopy  = pPrevBuff + ((x1 - px1) * pixWidth);
        pDstCopy  = pCurrBuff;
        bytesCopy = ((px2 - x1) + 1) * pixWidth;
        pSrc64 = (uint64_t)((uint32_t)pSrcCopy);
        pDst64 = (uint64_t)((uint32_t)pDstCopy);
        dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytesCopy, (y2 - y1) + 1, ptp, tp, 0);

        //dmaIndex  = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDstCopy, pSrcCopy, bytesCopy, (y2 - y1) + 1, ptp, tp, 0);

        pSrc64     = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + ((px2 + 1) * pixWidth));
        pDst     = pCurrBuff + (((px2 - x1) + 1) * pixWidth);
        bytes    = (x2 - px2) * pixWidth;
        pDst64 = (uint64_t)((uint32_t)pDst);
        dmaIndex = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytes, (y2 - y1) + 1, framePitchBytes, tp, interruptOnCompletion);

        //dmaIndex = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDst, pSrc, bytes, (y2 - y1) + 1, framePitchBytes, tp, interruptOnCompletion);
    }

    // Case 2. Only mid part overlaps
    if ((x1 < px1) && (px2 < x2))
    {
        pSrc64     = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + (x1 * pixWidth));
        pDst     = pCurrBuff;
        bytes    = (px1 - x1) * pixWidth;
        pDst64 = (uint64_t)((uint32_t)pDst);
        dmaIndex = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytes, (y2 - y1) + 1, framePitchBytes, tp, 0);
        //dmaIndex = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDst, pSrc, bytes, (y2 - y1) + 1, framePitchBytes, tp, 0);

        pSrcCopy  = pPrevBuff;
        pDstCopy  = pCurrBuff + ((px1 - x1) * pixWidth);
        bytesCopy = ((px2 - px1) + 1) * pixWidth;
        pSrc64 = (uint64_t)((uint32_t)pSrcCopy);
        pDst64 = (uint64_t)((uint32_t)pDstCopy);
        dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytesCopy, (y2 - y1) + 1, ptp, tp, 0);
        //dmaIndex  = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDstCopy, pSrcCopy, bytesCopy, (y2 - y1) + 1, ptp, tp, 0);

        pSrc64     = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + ((px2 + 1) * pixWidth));
        pDst     = pCurrBuff + (((px2 - x1) + 1) * pixWidth);
        bytes    = (x2 - px2) * pixWidth;
        pDst64 = (uint64_t)((uint32_t)pDst);
        dmaIndex = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytes, (y2 - y1) + 1, framePitchBytes, tp, interruptOnCompletion);
        //dmaIndex = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDst, pSrc, bytes, (y2 - y1) + 1, framePitchBytes, tp, interruptOnCompletion);
    }

    // Case 3. Only right part overlaps
    if ((x1 < px1) && (x2 <= px2))
    {
        pSrc64     = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + (x1 * pixWidth));
        pDst     = pCurrBuff;
        bytes    = (px1 - x1) * pixWidth;
        pDst64 = (uint64_t)((uint32_t)pDst);
        dmaIndex = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytes, (y2 - y1) + 1, framePitchBytes, tp, 0);


        pSrcCopy  = pPrevBuff;
        pDstCopy  = pCurrBuff + ((px1 - x1) * pixWidth);
        bytesCopy = ((x2 - px1) + 1) * pixWidth;
        pSrc64 = (uint64_t)((uint32_t)pSrcCopy);
        pDst64 = (uint64_t)((uint32_t)pDstCopy);
        dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytesCopy, (y2 - y1) + 1, ptp, tp, interruptOnCompletion);
        //dmaIndex  = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDstCopy, pSrcCopy, bytesCopy, (y2 - y1) + 1, ptp, tp, interruptOnCompletion);
    }

    // Case 4. All three regions overlaps
    if ((px1 <= x1) && (x2 <= px2))
    {
        pSrcCopy  = pPrevBuff + ((x1 - px1) * pixWidth);
        pDstCopy  = pCurrBuff;
        bytesCopy = ((x2 - x1) + 1) * pixWidth;
        pSrc64 = (uint64_t)((uint32_t)pSrcCopy);
        pDst64 = (uint64_t)((uint32_t)pDstCopy);
        dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, pDst64, pSrc64, bytesCopy, (y2 - y1) + 1, ptp, tp, interruptOnCompletion);
        //dmaIndex  = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, pDstCopy, pSrcCopy, bytesCopy, (y2 - y1) + 1, ptp, tp, interruptOnCompletion);
    }

    return(dmaIndex);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferInMultiChannelPredicated()
 *
 * DESCRIPTION:
 *     Requests data transfer from frame present in system memory to local tile memory.
 *     If there is an overlap between previous tile and current tile, tile reuse
 *     functionality can be used.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInMultiChannelPredicated(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion, uint32_t* pred_mask)
{
    xvFrame *pFrame;
    int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tileHeight, tilePitchBytes;
    int32_t statusFlag, x1, y1, x2, y2, dmaHeight, dmaWidthBytes, dmaIndex;
    int32_t tileIndex;
    int8_t framePadLeft, framePadRight, framePadTop, framePadBottom;
    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
    int16_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
    int8_t pixWidth, pixRes;
    uint64_t srcPtr64, dstPtr64;
    uint8_t *dstPtr, *edgePtr;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((interruptOnCompletion < 0), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_TILE(pTile, pxvTM);
    pFrame = pTile->pFrame;
    XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
   	XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)) , pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
    int32_t bytePerPel;
	bytePerPel	= bytesPerPix / channel;

   	XV_CHECK_ERROR((pFrame->pixelRes != bytePerPel) , pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
	XV_CHECK_ERROR((pFrame->numChannels != channel) , pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pTile->pPrevTile  = NULL;
    pTile->reuseCount = 0;
    frameWidth        = pFrame->frameWidth;
    frameHeight       = pFrame->frameHeight;
    framePadLeft      = pFrame->leftEdgePadWidth;
    framePadRight     = pFrame->rightEdgePadWidth;
    framePadTop       = pFrame->topEdgePadHeight;
    framePadBottom    = pFrame->bottomEdgePadHeight;
    pixRes            = pFrame->pixelRes;
    pixWidth          = pFrame->pixelRes * pFrame->numChannels;
    framePitchBytes   = pFrame->framePitch * pixRes;

    tileWidth      = pTile->width;
    tileHeight     = pTile->height;
    tilePitchBytes = pTile->pitch * pixRes;
    tileEdgeLeft   = pTile->tileEdgeLeft;
    tileEdgeRight  = pTile->tileEdgeRight;
    tileEdgeTop    = pTile->tileEdgeTop;
    tileEdgeBottom = pTile->tileEdgeBottom;

    statusFlag = pTile->status;

	XV_CHECK_ERROR(pred_mask == NULL,  pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");

	//predicated buffer needs to be in local memory
	if (!( (XV_PTR_START_IN_DRAM((uint8_t*)pred_mask)) && (XV_PTR_END_IN_DRAM( ((uint8_t*)pred_mask) + ((tileEdgeTop  + tileEdgeBottom + tileHeight + 7) >> 3 )))))
	{
		pxvTM->errFlag = XV_ERROR_BAD_ARG;
		return(XVTM_ERROR);
	}

	XV_CHECK_ERROR((tileEdgeLeft || tileEdgeRight  || tileEdgeTop || tileEdgeBottom),  pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");

    // 1. CHECK IF EXTRA PADDING NEEDED
    // Check top and bottom borders
    y1 = pTile->y - tileEdgeTop;
    if (y1 > (frameHeight + framePadBottom))
    {
        y1 = frameHeight + framePadBottom;
    }
    if (y1 < -framePadTop)
    {
        extraEdgeTop = -framePadTop - y1;
        y1           = -framePadTop;
        //statusFlag  |= XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeTop = 0;
    }

    y2 = pTile->y + (tileHeight - 1) + tileEdgeBottom;
    if (y2 < -framePadTop)
    {
        y2 = (int32_t) (-framePadTop - 1);
    }
    if (y2 > ((frameHeight - 1) + framePadBottom))
    {
        extraEdgeBottom = y2 - ((frameHeight - 1) + framePadBottom);
        y2              = (frameHeight - 1) + framePadBottom;
        //statusFlag     |= XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeBottom = 0;
    }

    // Check left and right borders
    x1 = pTile->x - tileEdgeLeft;
    if (x1 > (frameWidth + framePadRight))
    {
        x1 = frameWidth + framePadRight;
    }
    if (x1 < -framePadLeft)
    {
        extraEdgeLeft = -framePadLeft - x1;
        x1            = -framePadLeft;
        //statusFlag   |= XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeLeft = 0;
    }

    x2 = pTile->x + (tileWidth - 1) + tileEdgeRight;
    if (x2 < -framePadLeft)
    {
        x2 = (int32_t) (-framePadLeft - 1);
    }
    if (x2 > ((frameWidth - 1) + framePadRight))
    {
        extraEdgeRight = x2 - ((frameWidth - 1) + framePadRight);
        x2             = (frameWidth - 1) + framePadRight;
        //statusFlag    |= XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeRight = 0;
    }

    // 2. FILL ALL TILE and DMA RELATED DATA
    // No Need to align srcPtr and dstPtr as DMA does not need aligned start
    srcPtr64        = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + (x1 * pixWidth));
    dmaHeight     = (y2 - y1) + 1;
    dmaWidthBytes = ((x2 - x1) + 1) * pixWidth;
    edgePtr       = ((uint8_t *) pTile->pData - (((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth)));
    dstPtr        = edgePtr + (extraEdgeTop * tilePitchBytes) + (extraEdgeLeft * pixWidth);// For DMA

    // 3. DATA REUSE FROM PREVIOUS TILE
    if ((dmaHeight > 0) && (dmaWidthBytes > 0))
    {
        {
            dstPtr64 = (uint64_t)((uint32_t)dstPtr);
            dmaIndex = addIdmaRequestInlineMultiChannel_predicated_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, interruptOnCompletion, pred_mask);
        }
    }
    else
    {
        dmaIndex = XVTM_DUMMY_DMA_INDEX;
        statusFlag |= XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
    }
    pTile->status = statusFlag | XV_TILE_STATUS_DMA_ONGOING;
    tileIndex = (pxvTM->tileDMAstartIndex[dmaChannel] + pxvTM->tileDMApendingCount[dmaChannel]) % MAX_NUM_DMA_QUEUE_LENGTH;
    pxvTM->tileDMApendingCount[dmaChannel]++;
    pxvTM->tileProcQueue[dmaChannel][tileIndex] = pTile;
    pTile->dmaIndex                 = dmaIndex;
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferInMultiChannel()
 *
 * DESCRIPTION:
 *     Requests data transfer from frame present in system memory to local tile memory.
 *     If there is an overlap between previous tile and current tile, tile reuse
 *     functionality can be used.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     xvTile        *pPrevTile               Data is copied from this tile to pTile if the buffer overlaps
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, xvTile *pPrevTile, int32_t interruptOnCompletion)
{
    xvFrame *pFrame;
    int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tileHeight, tilePitchBytes;
    int32_t statusFlag, x1, y1, x2, y2, dmaHeight, dmaWidthBytes, dmaIndex;
    int32_t tileIndex;
    int8_t framePadLeft, framePadRight, framePadTop, framePadBottom;
    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
    int16_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
    int8_t pixWidth, pixRes;
    uint64_t srcPtr64, dstPtr64;
    uint8_t *dstPtr, *pPrevBuff, *pCurrBuff, *edgePtr;
    int32_t px1, px2, py1, py2;

   	XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((interruptOnCompletion < 0), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_TILE(pTile, pxvTM);
    pFrame = pTile->pFrame;
    XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
    XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)) , pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
    int32_t bytePerPel;
	bytePerPel	= bytesPerPix / channel;

    XV_CHECK_ERROR((pFrame->pixelRes != bytePerPel) , pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
	XV_CHECK_ERROR((pFrame->numChannels != channel) , pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pTile->pPrevTile  = NULL;
    pTile->reuseCount = 0;
    frameWidth        = pFrame->frameWidth;
    frameHeight       = pFrame->frameHeight;
    framePadLeft      = pFrame->leftEdgePadWidth;
    framePadRight     = pFrame->rightEdgePadWidth;
    framePadTop       = pFrame->topEdgePadHeight;
    framePadBottom    = pFrame->bottomEdgePadHeight;
    pixRes            = pFrame->pixelRes;
    pixWidth          = pFrame->pixelRes * pFrame->numChannels;
    framePitchBytes   = pFrame->framePitch * pixRes;

    tileWidth      = pTile->width;
    tileHeight     = pTile->height;
    tilePitchBytes = pTile->pitch * pixRes;
    tileEdgeLeft   = pTile->tileEdgeLeft;
    tileEdgeRight  = pTile->tileEdgeRight;
    tileEdgeTop    = pTile->tileEdgeTop;
    tileEdgeBottom = pTile->tileEdgeBottom;

    statusFlag = pTile->status;

    // 1. CHECK IF EXTRA PADDING NEEDED
    // Check top and bottom borders
    y1 = pTile->y - tileEdgeTop;
    if (y1 > (frameHeight + framePadBottom))
    {
        y1 = frameHeight + framePadBottom;
    }
    if (y1 < -framePadTop)
    {
        extraEdgeTop = -framePadTop - y1;
        y1           = -framePadTop;
        statusFlag  |= XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeTop = 0;
    }

    y2 = pTile->y + (tileHeight - 1) + tileEdgeBottom;
    if (y2 < -framePadTop)
    {
        y2 = (int32_t) (-framePadTop - 1);
    }
    if (y2 > ((frameHeight - 1) + framePadBottom))
    {
        extraEdgeBottom = y2 - ((frameHeight - 1) + framePadBottom);
        y2              = (frameHeight - 1) + framePadBottom;
        statusFlag     |= XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeBottom = 0;
    }

    // Check left and right borders
    x1 = pTile->x - tileEdgeLeft;
    if (x1 > (frameWidth + framePadRight))
    {
        x1 = frameWidth + framePadRight;
    }
    if (x1 < -framePadLeft)
    {
        extraEdgeLeft = -framePadLeft - x1;
        x1            = -framePadLeft;
        statusFlag   |= XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeLeft = 0;
    }

    x2 = pTile->x + (tileWidth - 1) + tileEdgeRight;
    if (x2 < -framePadLeft)
    {
        x2 = (int32_t) (-framePadLeft - 1);
    }
    if (x2 > ((frameWidth - 1) + framePadRight))
    {
        extraEdgeRight = x2 - ((frameWidth - 1) + framePadRight);
        x2             = (frameWidth - 1) + framePadRight;
        statusFlag    |= XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeRight = 0;
    }

    // 2. FILL ALL TILE and DMA RELATED DATA
    // No Need to align srcPtr and dstPtr as DMA does not need aligned start
    srcPtr64        = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + (x1 * pixWidth));
    dmaHeight     = (y2 - y1) + 1;
    dmaWidthBytes = ((x2 - x1) + 1) * pixWidth;
    edgePtr       = ((uint8_t *) pTile->pData - (((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth)));
    dstPtr        = edgePtr + (extraEdgeTop * tilePitchBytes) + (extraEdgeLeft * pixWidth);// For DMA

    // 3. DATA REUSE FROM PREVIOUS TILE
    if ((dmaHeight > 0) && (dmaWidthBytes > 0))
    {
        if (pPrevTile != NULL)
        {
            XV_CHECK_ERROR(!(XV_TILE_IS_TILE(pPrevTile) > 0), (pxvTM)->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "The argument is not a tile");
            XV_CHECK_ERROR(!(XV_TILE_STARTS_IN_DRAM(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "Data buffer does not start in DRAM");
            XV_CHECK_ERROR(!(XV_TILE_ENDS_IN_DRAM(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "Data buffer does not fit in DRAM");
            XV_CHECK_ERROR(!(XV_TILE_GET_TYPE(pTile) == XV_TILE_GET_TYPE(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "Invalid tile types");
            XV_CHECK_ERROR(!(XV_TILE_IS_CONSISTENT(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "Invalid buffer");


            py1       = pPrevTile->y - ((int32_t) pPrevTile->tileEdgeTop);
            py2       = ((pPrevTile->y + pPrevTile->height) - 1) + ((int32_t) pPrevTile->tileEdgeBottom);
            px1       = pPrevTile->x - ((int32_t) pPrevTile->tileEdgeLeft);
            px2       = ((pPrevTile->x + pPrevTile->width) - 1) + ((int32_t) pPrevTile->tileEdgeRight);
            pPrevBuff = (uint8_t *) pPrevTile->pData - ((pPrevTile->tileEdgeTop * pPrevTile->pitch * pixRes) + (pPrevTile->tileEdgeLeft * pixWidth)); // Same pixRes for current and prev tile
            // Case 1. Two tiles are totally independent. DMA entire tile. No copying
            if ((py2 < y1) || (py1 > y2) || (px2 < x1) || (px1 > x2))
            {
                dstPtr64 = (uint64_t)((uint32_t)dstPtr);
                dmaIndex = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, interruptOnCompletion);
            }
            else
            {
                pTile->pPrevTile = pPrevTile;
                pPrevTile->reuseCount++;

                // Case 2. Only top part overlaps.
                if ((py1 <= y1) && (py2 < y2))
                {
                    // Top part
                    pCurrBuff  = dstPtr;
                    pPrevBuff += (y1 - py1) * pPrevTile->pitch * pixRes;
                    // Bottom part
                    srcPtr64    = pFrame->pFrameData + (uint64_t)(((py2 + 1) * framePitchBytes) + (x1 * pixWidth));
                    dstPtr   += ((py2 + 1) - y1) * tilePitchBytes;
                    dmaHeight = y2 - py2; // (y2+1) - (py2+1)
                    dstPtr64 = (uint64_t)((uint32_t)dstPtr);
                    dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);


                    dmaIndex = solveForX(dmaChannel, pxvTM, pTile, pCurrBuff, pPrevBuff, y1, py2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);
                }

                // Case 3. Only mid part overlaps
                if ((y1 < py1) && (py2 < y2))
                {
                    // Top part
                    dmaHeight = (py1 + 1) - y1;
                    dstPtr64 = (uint64_t)((uint32_t)dstPtr);

                    dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);
                    //dmaIndex  = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, dstPtr, srcPtr, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);
                    // Mid overlapping part
                    pCurrBuff = dstPtr + ((py1 - y1) * tilePitchBytes);
                    // Bottom part
                    srcPtr64    = pFrame->pFrameData + (uint64_t)(((py2 + 1) * framePitchBytes) + (x1 * pixWidth));
                    dstPtr   += ((py2 + 1) - y1) * tilePitchBytes;
                    dmaHeight = y2 - py2; // (y2+1) - (py2+1)
                    dstPtr64 = (uint64_t)((uint32_t)dstPtr);

                    dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);

                    dmaIndex  = solveForX(dmaChannel, pxvTM, pTile, pCurrBuff, pPrevBuff, py1, py2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);
                }

                // Case 4. Only bottom part overlaps
                if ((y1 < py1) && (y2 <= py2))
                {
                    dmaHeight = py1 - y1;
                    dstPtr64 = (uint64_t)((uint32_t)dstPtr);
                    dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);
                    //dmaIndex  = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, dstPtr, srcPtr, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);
                    pCurrBuff = dstPtr + (dmaHeight * tilePitchBytes);
                    dmaIndex  = solveForX(dmaChannel, pxvTM, pTile, pCurrBuff, pPrevBuff, py1, y2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);
                }

                // Case 5. All the parts overlaps
                if ((py1 <= y1) && (y2 <= py2))
                {
                    pCurrBuff  = dstPtr;
                    pPrevBuff += (y1 - py1) * pPrevTile->pitch * pixRes;
                    dmaIndex   = solveForX(dmaChannel, pxvTM, pTile, pCurrBuff, pPrevBuff, y1, y2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);
                }
            }
        }

        else
        {
            dstPtr64 = (uint64_t)((uint32_t)dstPtr);
            dmaIndex = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, interruptOnCompletion);
        }
    }
    else
    {
        dmaIndex = XVTM_DUMMY_DMA_INDEX;
        statusFlag |= XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
    }
    pTile->status = statusFlag | XV_TILE_STATUS_DMA_ONGOING;
    tileIndex = (pxvTM->tileDMAstartIndex[dmaChannel] + pxvTM->tileDMApendingCount[dmaChannel]) % MAX_NUM_DMA_QUEUE_LENGTH;
    pxvTM->tileDMApendingCount[dmaChannel]++;
    pxvTM->tileProcQueue[dmaChannel][tileIndex] = pTile;
    pTile->dmaIndex                 = dmaIndex;
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferInFastMultiChannel()
 *
 * DESCRIPTION:
 *     Requests 8b data transfer from frame present in system memory to local tile memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInFastMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
    xvFrame *pFrame;
    int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tileHeight, tilePitchBytes;
    int32_t statusFlag, x1, y1, x2, y2, dmaHeight, dmaWidthBytes, dmaIndex, copyRowBytes;
    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
    int16_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
    uint64_t srcPtr64, dstPtr64;
    uint8_t *srcPtr, *dstPtr, *edgePtr;
	int8_t pixWidth, pixRes;

    pFrame = pTile->pFrame;

    frameWidth      = pFrame->frameWidth;
    frameHeight     = pFrame->frameHeight;
	pixRes          = pFrame->pixelRes;
	pixWidth        = pFrame->pixelRes * pFrame->numChannels;
	framePitchBytes = pFrame->framePitch * pixRes;

    tileWidth      = pTile->width;
    tileHeight     = pTile->height;
	tilePitchBytes = pTile->pitch * pixRes;
    tileEdgeLeft   = pTile->tileEdgeLeft;
    tileEdgeRight  = pTile->tileEdgeRight;
    tileEdgeTop    = pTile->tileEdgeTop;
    tileEdgeBottom = pTile->tileEdgeBottom;

    statusFlag      = pTile->status;
    extraEdgeTop    = 0;
    extraEdgeBottom = 0;
    extraEdgeLeft   = 0;
    extraEdgeRight  = 0;

    // 1. CHECK IF EXTRA PADDING NEEDED
    // Check top and bottom borders
    y1 = pTile->y - tileEdgeTop;
    if (y1 > frameHeight)
    {
        y1 = frameHeight;
    }
    if (y1 < 0)
    {
        extraEdgeTop = -y1;
        y1           = 0;
        statusFlag  |= XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
    }

    y2 = pTile->y + (tileHeight - 1) + tileEdgeBottom;
    if (y2 < 0)
    {
        y2 = -1;
    }
    if (y2 > (frameHeight - 1))
    {
        extraEdgeBottom = (y2 - frameHeight) + 1;
        y2              = frameHeight - 1;
        statusFlag     |= XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
    }

    // Check left and right borders
    x1 = pTile->x - tileEdgeLeft;
    if (x1 > frameWidth)
    {
        x1 = frameWidth;
    }
    if (x1 < 0)
    {
        extraEdgeLeft = -x1;
        x1            = 0;
        statusFlag   |= XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED;
    }

    x2 = pTile->x + (tileWidth - 1) + tileEdgeRight;
    if (x2 < 0)
    {
        x2 = -1;
    }
    if (x2 > (frameWidth - 1))
    {
        extraEdgeRight = (x2 - frameWidth) + 1;
        x2             = frameWidth - 1;
        statusFlag    |= XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED;
    }

    // 2. FILL ALL TILE and DMA RELATED DATA
    // No Need to align srcPtr as DMA does not need aligned start
    // But dstPtr needs to be aligned
    dmaHeight     = (y2 - y1) + 1;
	dmaWidthBytes = ((x2 - x1) + 1) * pixWidth;

    interruptOnCompletion = (interruptOnCompletion != 0) ? DESC_NOTIFY_W_INT : 0;
    uint32_t intrCompletionFlag;

    if ((dmaHeight > 0) && (dmaWidthBytes > 0))
    {
        srcPtr64  = pFrame->pFrameData + (uint64_t)((y1 * framePitchBytes) + (x1 * pixWidth));
        edgePtr = (uint8_t *) pTile->pData - ((tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * pixWidth));
        dstPtr  = edgePtr + ((extraEdgeTop * tilePitchBytes) + ((int32_t) extraEdgeLeft * pixWidth));// For DMA

        // One interrupt per tile request. Interrupt only if it is last DMA request for this tile.
        intrCompletionFlag = interruptOnCompletion * !((statusFlag & (XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED | XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED)) && (pFrame->paddingType == FRAME_EDGE_PADDING));
        dstPtr64 = (uint64_t)((uint32_t)dstPtr);
        
        TM_LOG_PRINT("line=%d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
            __LINE__, srcPtr64, dstPtr64, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, intrCompletionFlag);
        dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &dstPtr64, &srcPtr64, dmaWidthBytes, intrCompletionFlag, dmaHeight, framePitchBytes, tilePitchBytes);
        TM_LOG_PRINT("line=%d, dmaIndex: %d\n", __LINE__, dmaIndex);

        //dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, dstPtr, srcPtr, dmaWidthBytes, intrCompletionFlag, dmaHeight, framePitchBytes, tilePitchBytes);
        if ((statusFlag & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) && (pFrame->paddingType == FRAME_EDGE_PADDING))
        {
            srcPtr       = ((uint8_t *) pTile->pData - (((int32_t) tileEdgeTop - (int32_t) extraEdgeTop) * tilePitchBytes) - ((int32_t) tileEdgeLeft * pixWidth));
            dstPtr       = srcPtr - (extraEdgeTop * tilePitchBytes);
			copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * pixWidth;

            // One interrupt per tile request. Interrupt only if it is last DMA request for this tile.
            intrCompletionFlag = interruptOnCompletion * !((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED));
            srcPtr64 = (uint64_t)((uint32_t)srcPtr);
            dstPtr64 = (uint64_t)((uint32_t)dstPtr);

            TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                __LINE__, srcPtr64, dstPtr64, copyRowBytes, extraEdgeTop, 0, tilePitchBytes, intrCompletionFlag);
            dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &dstPtr64, &srcPtr64, copyRowBytes, intrCompletionFlag, extraEdgeTop, 0, tilePitchBytes);
            TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);

            statusFlag = statusFlag & ~XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
        }

        if ((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) && (pFrame->paddingType == FRAME_EDGE_PADDING))
        {
			srcPtr       = ((uint8_t *) pTile->pData - (tileEdgeLeft * pixWidth)) + (((tileHeight + tileEdgeBottom) - extraEdgeBottom - 1) * tilePitchBytes);
            dstPtr       = srcPtr + tilePitchBytes;
			copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * pixWidth;
            srcPtr64 = (uint64_t)((uint32_t)srcPtr);
            dstPtr64 = (uint64_t)((uint32_t)dstPtr);

            TM_LOG_PRINT("line = %d, src: %llx: dst, %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                __LINE__, srcPtr64, dstPtr64, copyRowBytes, extraEdgeBottom, 0, tilePitchBytes, interruptOnCompletion);
            dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &dstPtr64, &srcPtr64, copyRowBytes, interruptOnCompletion, extraEdgeBottom, 0, tilePitchBytes);
            TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);
            
            //dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, dstPtr, srcPtr, copyRowBytes, interruptOnCompletion, extraEdgeBottom, 0, tilePitchBytes);
            statusFlag = statusFlag & ~XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
        }
    }
    else
    {
        dmaIndex = XVTM_DUMMY_DMA_INDEX;
        statusFlag |= XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
    }
    pTile->status   = statusFlag | XV_TILE_STATUS_DMA_ONGOING;
    pTile->dmaIndex = dmaIndex;
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferInFast16MultiChannel()
 *
 * DESCRIPTION:
 *     Requests 16b data transfer from frame present in system memory to local tile memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInFast16MultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{

    return(xvReqTileTransferInFastMultiChannel( dmaChannel, pxvTM, pTile, interruptOnCompletion));
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutMultiChannelPredicated()
 *
 * DESCRIPTION:
 *     Requests data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *     uint32_t*     pred_mask                Predication mask
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferOutMultiChannelPredicated(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion, uint32_t *pred_mask)
{
    xvFrame *pFrame;
    uint8_t *srcPtr;
    uint64_t srcPtr64, dstPtr64;
    int32_t pixWidth, tileIndex, dmaIndex;
    int32_t srcHeight, srcWidth, srcPitchBytes;
    int32_t dstPitchBytes, numRows, rowSize;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((interruptOnCompletion < 0), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_TILE(pTile, pxvTM);
    pFrame = pTile->pFrame;
    XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
    XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)), pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
    int32_t bytePerPel;
	bytePerPel	= bytesPerPix / channel;

    XV_CHECK_ERROR((pFrame->pixelRes != bytePerPel), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
	XV_CHECK_ERROR((pFrame->numChannels != channel), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pixWidth      = pFrame->pixelRes * pFrame->numChannels;
    srcHeight     = pTile->height;
    srcWidth      = pTile->width;
    srcPitchBytes = pTile->pitch * pFrame->pixelRes;
    dstPitchBytes = pFrame->framePitch * pFrame->pixelRes;
    numRows       = XVTM_MIN(srcHeight, pFrame->frameHeight - pTile->y);
    rowSize       = XVTM_MIN(srcWidth, (pFrame->frameWidth - pTile->x));
    XV_CHECK_ERROR(pred_mask == NULL,  pxvTM->errFlag = XV_ERROR_BAD_ARG; , XVTM_ERROR, "XV_ERROR_BAD_ARG");

	//predicated buffer needs to be in local memory
	if (!( (XV_PTR_START_IN_DRAM((uint8_t*)pred_mask)) && (XV_PTR_END_IN_DRAM( ((uint8_t*)pred_mask) + ((srcHeight + 7) >> 3 )))))
	{
		pxvTM->errFlag = XV_ERROR_BAD_ARG;
		return(XVTM_ERROR);
	}

    srcPtr = (uint8_t *) pTile->pData;
    dstPtr64 = pFrame->pFrameData + (uint64_t)((pTile->y * dstPitchBytes) + (pTile->x * pixWidth));

    if (pTile->x < 0)
    {
        rowSize += pTile->x; // x is negative;
        srcPtr  += (-pTile->x * pixWidth);
        dstPtr64  += (uint64_t)(-pTile->x * pixWidth);
    }

    if (pTile->y < 0)
    {
        numRows += pTile->y; // y is negative;
        srcPtr  += (-pTile->y * srcPitchBytes);
        dstPtr64  += (uint64_t)(-pTile->y * dstPitchBytes);
    }
    rowSize *= pixWidth;

    if ((rowSize != 0) && (numRows != 0))
    {
        pTile->status = pTile->status | XV_TILE_STATUS_DMA_ONGOING;
        tileIndex     = (pxvTM->tileDMAstartIndex[dmaChannel] + pxvTM->tileDMApendingCount[dmaChannel]) % MAX_NUM_DMA_QUEUE_LENGTH;
        pxvTM->tileDMApendingCount[dmaChannel]++;
        pxvTM->tileProcQueue[dmaChannel][tileIndex] = pTile;
        srcPtr64 = (uint64_t)((uint32_t)srcPtr);
        dmaIndex  = addIdmaRequestInlineMultiChannel_predicated_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, rowSize, numRows, srcPitchBytes, dstPitchBytes, interruptOnCompletion, pred_mask);
        //dmaIndex                        = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, dstPtr, srcPtr, rowSize, numRows, srcPitchBytes, dstPitchBytes, interruptOnCompletion);
        pTile->dmaIndex                 = dmaIndex;
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutMultiChannel()
 *
 * DESCRIPTION:
 *     Requests data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */
int32_t xvReqTileTransferOutMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
    xvFrame *pFrame;
    uint8_t *srcPtr;
    uint64_t srcPtr64, dstPtr64;
    int32_t pixWidth, tileIndex, dmaIndex;
    int32_t srcHeight, srcWidth, srcPitchBytes;
    int32_t dstPitchBytes, numRows, rowSize;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((interruptOnCompletion < 0), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_TILE(pTile, pxvTM);
    pFrame = pTile->pFrame;
    XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
   	XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)), pxvTM->errFlag = XV_ERROR_FRAME_NULL;, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
    int32_t bytePerPel;
	bytePerPel	= bytesPerPix / channel;

    XV_CHECK_ERROR((pFrame->pixelRes != bytePerPel), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");
	XV_CHECK_ERROR((pFrame->numChannels != channel), pxvTM->errFlag = XV_ERROR_BAD_ARG;, XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pixWidth      = pFrame->pixelRes * pFrame->numChannels;
    srcHeight     = pTile->height;
    srcWidth      = pTile->width;
    srcPitchBytes = pTile->pitch * pFrame->pixelRes;
    dstPitchBytes = pFrame->framePitch * pFrame->pixelRes;
    numRows       = XVTM_MIN(srcHeight, pFrame->frameHeight - pTile->y);
    rowSize       = XVTM_MIN(srcWidth, (pFrame->frameWidth - pTile->x));

    srcPtr = (uint8_t *) pTile->pData;
    dstPtr64 = pFrame->pFrameData + (uint64_t)((pTile->y * dstPitchBytes) + (pTile->x * pixWidth));

    if (pTile->x < 0)
    {
        rowSize += pTile->x; // x is negative;
        srcPtr  += (-pTile->x * pixWidth);
        dstPtr64  += (uint64_t)(-pTile->x * pixWidth);
    }

    if (pTile->y < 0)
    {
        numRows += pTile->y; // y is negative;
        srcPtr  += (-pTile->y * srcPitchBytes);
        dstPtr64  += (uint64_t)(-pTile->y * dstPitchBytes);
    }
    rowSize *= pixWidth;

    if ((rowSize != 0) && (numRows != 0))
    {
        pTile->status = pTile->status | XV_TILE_STATUS_DMA_ONGOING;
        tileIndex     = (pxvTM->tileDMAstartIndex[dmaChannel] + pxvTM->tileDMApendingCount[dmaChannel]) % MAX_NUM_DMA_QUEUE_LENGTH;
        pxvTM->tileDMApendingCount[dmaChannel]++;
        pxvTM->tileProcQueue[dmaChannel][tileIndex] = pTile;
        srcPtr64 = (uint64_t)((uint32_t)srcPtr);
        dmaIndex  = addIdmaRequestInlineMultiChannel_wide(dmaChannel, pxvTM, dstPtr64, srcPtr64, rowSize, numRows, srcPitchBytes, dstPitchBytes, interruptOnCompletion);
        //dmaIndex                        = addIdmaRequestInlineMultiChannel(dmaChannel, pxvTM, dstPtr, srcPtr, rowSize, numRows, srcPitchBytes, dstPitchBytes, interruptOnCompletion);
        pTile->dmaIndex                 = dmaIndex;
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutFastMultiChannel()
 *
 * DESCRIPTION:
 *     Requests data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferOutFastMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
    xvFrame *pFrame;
    uint64_t srcPtr64, dstPtr64;
    uint8_t *srcPtr;
	int32_t dmaIndex, pixWidth;
    int32_t srcHeight, srcWidth, srcPitchBytes;
    int32_t dstPitchBytes, numRows, rowSize;

    pFrame = pTile->pFrame;

	pixWidth      = pFrame->pixelRes * pFrame->numChannels;
    srcHeight     = pTile->height;
    srcWidth      = pTile->width;
	srcPitchBytes = pTile->pitch * pFrame->pixelRes;
	dstPitchBytes = pFrame->framePitch * pFrame->pixelRes;
    numRows       = XVTM_MIN(srcHeight, pFrame->frameHeight - pTile->y);
    rowSize       = XVTM_MIN(srcWidth, (pFrame->frameWidth - pTile->x));

    srcPtr = (uint8_t *) pTile->pData;
    dstPtr64 = pFrame->pFrameData + (uint64_t)((pTile->y * dstPitchBytes) + (pTile->x * pixWidth));

    if (pTile->x < 0)
    {
        rowSize += pTile->x; // x is negative;
        srcPtr  += (-pTile->x * pixWidth);
        dstPtr64  += (uint64_t)(-pTile->x * pixWidth);
    }

    if (pTile->y < 0)
    {
        numRows += pTile->y; // y is negative;
        srcPtr  += (-pTile->y * srcPitchBytes);
        dstPtr64  += (uint64_t)(-pTile->y * dstPitchBytes);
    }
	rowSize *= pixWidth;
    uint32_t intrCompletionFlag = (interruptOnCompletion != 0) ? DESC_NOTIFY_W_INT : 0;

    if ((rowSize != 0) && (numRows != 0))
    {
        pTile->status = pTile->status | XV_TILE_STATUS_DMA_ONGOING;
        srcPtr64 = (uint64_t)((uint32_t)srcPtr);
        
        TM_LOG_PRINT("line = %d, src: %llx, dst: %llx,  rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                __LINE__, srcPtr64, dstPtr64, rowSize, numRows, srcPitchBytes, dstPitchBytes, intrCompletionFlag);
        dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, &dstPtr64, &srcPtr64, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
        TM_LOG_PRINT("line = %d, dmaIndex: %d\n", __LINE__, dmaIndex);

        //dmaIndex = idma_copy_2d_desc64_wide(dmaChannel, dstPtr, srcPtr, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
        pTile->dmaIndex = dmaIndex;
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutFast16MultiChannel()
 *
 * DESCRIPTION:
 *     Requests 16b data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */
int32_t xvReqTileTransferOutFast16MultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{

    return(xvReqTileTransferOutFastMultiChannel(dmaChannel, pxvTM, pTile, interruptOnCompletion));
}

/**********************************************************************************
 * FUNCTION: xvCheckForIdmaIndexMultiChannel()
 *
 * DESCRIPTION:
 *     Checks if DMA transfer for given index is completed
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     int32_t       index                    Index of the dma transfer request, returned by xvAddIdmaRequest()
 *
 * OUTPUTS:
 *     Returns ONE if transfer is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */

int32_t xvCheckForIdmaIndexMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, int32_t index)
{
    int32_t retVal = 1;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    XV_CHECK_ERROR(index < 0,   , XVTM_ERROR, "XVTM_ERROR");

    retVal = idma_desc_done(dmaChannel, index);

    return(retVal);
}

/**********************************************************************************
 * FUNCTION: xvCheckTileReadyMultiChannel()
 *
 * DESCRIPTION:
 *     Checks if DMA transfer for given tile is completed.
 *     It checks all the tile in the transfer request buffer
 *     before the given tile and updates their respective
 *     status. It pads edges wherever required.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */

int32_t xvCheckTileReadyMultiChannel(int32_t dmaChannel, xvTileManager *pxvTM, xvTile const *pTile)
{
    int32_t loopInd, index, retVal, dmaIndex, statusFlag, doneCount;
    xvTile *pTile1;
    xvFrame *pFrame;
    int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tileHeight, tilePitchBytes, tileDMApendingCount;
    int32_t x1, y1, x2, y2, copyRowBytes, copyHeight;
    int8_t framePadLeft, framePadRight, framePadTop, framePadBottom;
    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
    int16_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
    int8_t pixWidth;
    uint8_t *srcPtr, *dstPtr;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    pxvTM->errFlag = XV_ERROR_SUCCESS;

    XV_CHECK_TILE(pTile, pxvTM);
    if (pTile->status == 0)
    {
        return(1);
    }

    doneCount = 0;
    index     = pxvTM->tileDMAstartIndex[dmaChannel];
    tileDMApendingCount = pxvTM->tileDMApendingCount[dmaChannel];
    for (loopInd = 0; loopInd < tileDMApendingCount; loopInd++)
    {
        index    = (pxvTM->tileDMAstartIndex[dmaChannel] + loopInd) % MAX_NUM_DMA_QUEUE_LENGTH;
        pTile1   = pxvTM->tileProcQueue[dmaChannel][index % MAX_NUM_DMA_QUEUE_LENGTH];
        dmaIndex = pTile1->dmaIndex;
        if (!(pTile1->status & XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED))
        {
            retVal = xvCheckForIdmaIndexMultiChannel(dmaChannel, pxvTM, dmaIndex);
            if (retVal == 1)
            {
                statusFlag = pTile1->status;
                statusFlag = statusFlag & ~XV_TILE_STATUS_DMA_ONGOING;

                pFrame          = pTile1->pFrame;
                pixWidth        = pFrame->pixelRes * pFrame->numChannels;
                frameWidth      = pFrame->frameWidth;
                frameHeight     = pFrame->frameHeight;
                framePitchBytes = pFrame->framePitch * pFrame->pixelRes;
                framePadLeft    = pFrame->leftEdgePadWidth;
                framePadRight   = pFrame->rightEdgePadWidth;
                framePadTop     = pFrame->topEdgePadHeight;
                framePadBottom  = pFrame->bottomEdgePadHeight;

                tileWidth      = pTile1->width;
                tileHeight     = pTile1->height;
                tilePitchBytes = pTile1->pitch * pFrame->pixelRes;
                tileEdgeLeft   = pTile1->tileEdgeLeft;
                tileEdgeRight  = pTile1->tileEdgeRight;
                tileEdgeTop    = pTile1->tileEdgeTop;
                tileEdgeBottom = pTile1->tileEdgeBottom;

                if ((statusFlag & XV_TILE_STATUS_EDGE_PADDING_NEEDED) != 0)
                {
                    if ((statusFlag & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) != 0)
                    {
                        y1           = pTile1->y - tileEdgeTop;
                        extraEdgeTop = -framePadTop - y1;
                        dstPtr       = ((uint8_t *) pTile1->pData - (((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth)));
                        srcPtr       = dstPtr + (extraEdgeTop * tilePitchBytes);
                        copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * XV_TYPE_ELEMENT_SIZE(pTile1->type);
                        copyBufferEdgeDataH(srcPtr, dstPtr, copyRowBytes, pixWidth, extraEdgeTop, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
                    }

                    if ((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) != 0)
                    {
                        y2              = pTile1->y + (tileHeight - 1) + tileEdgeBottom;
                        extraEdgeBottom = y2 - ((frameHeight - 1) + framePadBottom);
                        y2              = (frameHeight - 1) + framePadBottom;
                        dstPtr          = ((uint8_t *) pTile1->pData + (((y2 - pTile1->y) + 1) * tilePitchBytes)) - (tileEdgeLeft * pixWidth);
                        srcPtr          = dstPtr - tilePitchBytes;
                        copyRowBytes    = (tileEdgeLeft + tileWidth + tileEdgeRight) * XV_TYPE_ELEMENT_SIZE(pTile1->type);
                        copyBufferEdgeDataH(srcPtr, dstPtr, copyRowBytes, pixWidth, extraEdgeBottom, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
                    }

                    if ((statusFlag & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0)
                    {
                        x1            = pTile1->x - tileEdgeLeft;
                        extraEdgeLeft = -framePadLeft - x1;
                        dstPtr        = ((uint8_t *) pTile1->pData - (((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth)));
                        srcPtr        = dstPtr + (extraEdgeLeft * pixWidth);
                        copyHeight    = tileEdgeTop + tileHeight + tileEdgeBottom;
                        copyBufferEdgeDataV(srcPtr, dstPtr, extraEdgeLeft, pixWidth, copyHeight, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
                    }

                    if ((statusFlag & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0)
                    {
                        x2             = pTile1->x + (tileWidth - 1) + tileEdgeRight;
                        extraEdgeRight = x2 - ((frameWidth - 1) + framePadRight);
                        x2             = (frameWidth - 1) + framePadRight;
                        dstPtr         = ((uint8_t *) pTile1->pData - (tileEdgeTop * tilePitchBytes)) + (((x2 - pTile1->x) + 1) * pixWidth);
                        srcPtr         = dstPtr - (1 * pixWidth);
                        copyHeight     = tileEdgeTop + tileHeight + tileEdgeBottom;
                        copyBufferEdgeDataV(srcPtr, dstPtr, extraEdgeRight, pixWidth, copyHeight, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
                    }
                }

                statusFlag = statusFlag & ~XV_TILE_STATUS_EDGE_PADDING_NEEDED;

                if ((pTile1->pPrevTile) != NULL)
                {
                    pTile1->pPrevTile->reuseCount--;
                    pTile1->pPrevTile = NULL;
                }

                pTile1->status = statusFlag;
                doneCount++;
            }
            else // DMA not done for this tile
            {
                break;
            }
        }
        else
        {
            // Tile is not part of frame. Make everything constant
            pFrame         = pTile1->pFrame;
            tileWidth      = pTile1->width;
            tileHeight     = pTile1->height;
            pixWidth       = pFrame->pixelRes * pFrame->numChannels;
            tilePitchBytes = pTile1->pitch * pFrame->pixelRes;
            tileEdgeLeft   = pTile1->tileEdgeLeft;
            tileEdgeRight  = pTile1->tileEdgeRight;
            tileEdgeTop    = pTile1->tileEdgeTop;
            tileEdgeBottom = pTile1->tileEdgeBottom;

            dstPtr       = ((uint8_t *) pTile1->pData - (((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth)));
            srcPtr       = NULL;
            copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * XV_TYPE_ELEMENT_SIZE(pTile1->type);
            copyHeight   = tileHeight + tileEdgeTop + tileEdgeBottom;
            if (pFrame->paddingType != FRAME_EDGE_PADDING)
            {
                copyBufferEdgeDataH(srcPtr, dstPtr, copyRowBytes, pixWidth, copyHeight, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
            }
            pTile1->status = 0;
            doneCount++;
        }

        // Break if we reached the required tile
        if (pTile1 == pTile)
        {
            index = (index + 1) % MAX_NUM_DMA_QUEUE_LENGTH;
            break;
        }
    }

    pxvTM->tileDMAstartIndex[dmaChannel]   = index;
    pxvTM->tileDMApendingCount[dmaChannel] = pxvTM->tileDMApendingCount[dmaChannel] - doneCount;
    return(pTile->status == 0);
}

/**********************************************************************************
 * FUNCTION: xvPadEdges()
 *
 * DESCRIPTION:
 *     Pads edges of the given 8b tile. If FRAME_EDGE_PADDING mode is used,
 *     padding is done using edge values of the frame else if
 *     FRAME_CONSTANT_PADDING or FRAME_ZERO_PADDING mode is used,
 *     constant or zero value is padded
 *    xvPadEdges should be used with Fast functions
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvPadEdges(xvTileManager *pxvTM, xvTile *pTile)
{
    int32_t x1, x2, y1, y2, indy, copyHeight, copyRowBytes, wb, padVal;
    int32_t tileWidth, tileHeight, tilePitchBytes, frameWidth, frameHeight;

    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
    int16_t extraEdgeLeft, extraEdgeRight, extraEdgeTop, extraEdgeBottom;
	uint16_t * __restrict srcPtr_16b, * __restrict dstPtr_16b;
	uint32_t * __restrict srcPtr_32b, * __restrict dstPtr_32b;
    uint8_t * __restrict srcPtr, * __restrict dstPtr;
    xvFrame *pFrame;
	xb_vecNx16 vec1, * __restrict pvecDst;
    xb_vec2Nx8U dvec1, * __restrict pdvecDst;
	xb_vecN_2x32Uv hvec1, * __restrict phvecDst;
    valign vas1;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    XV_CHECK_TILE(pTile, pxvTM);
    if ((pTile->status & XV_TILE_STATUS_EDGE_PADDING_NEEDED) != 0)
    {
        pFrame = pTile->pFrame;
      	XV_CHECK_ERROR(pFrame == NULL,  pxvTM->errFlag = XV_ERROR_FRAME_NULL; , XVTM_ERROR, "XV_ERROR_FRAME_NULL");
		XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)),  pxvTM->errFlag = XV_ERROR_FRAME_NULL; , XVTM_ERROR, "XV_ERROR_FRAME_NULL");

        int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
        int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
        int32_t bytePerPel;
		bytePerPel = bytesPerPix / channel;

		XV_CHECK_ERROR(pFrame->pixelRes != bytePerPel,  pxvTM->errFlag = XV_ERROR_BAD_ARG; , XVTM_ERROR, "XV_ERROR_BAD_ARG");
		XV_CHECK_ERROR(pFrame->numChannels != channel,  pxvTM->errFlag = XV_ERROR_BAD_ARG; , XVTM_ERROR, "XV_ERROR_BAD_ARG");

        tileEdgeTop    = pTile->tileEdgeTop;
        tileEdgeBottom = pTile->tileEdgeBottom;
        tileEdgeLeft   = pTile->tileEdgeLeft;
        tileEdgeRight  = pTile->tileEdgeRight;
        tilePitchBytes = pTile->pitch;
        tileHeight     = pTile->height;
        tileWidth      = pTile->width;
        frameWidth     = pFrame->frameWidth;
        frameHeight    = pFrame->frameHeight;

        if (pFrame->paddingType == FRAME_EDGE_PADDING)
        {
			if (bytesPerPix == 1)
			{
	            if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0)
	            {
	                extraEdgeLeft = tileEdgeLeft - pTile->x;
	                dstPtr        = (uint8_t *) pTile->pData - ((tileEdgeTop * tilePitchBytes) + tileEdgeLeft);
	                srcPtr        = dstPtr + extraEdgeLeft;
	                copyHeight    = tileEdgeTop + tileHeight + tileEdgeBottom;

	                vas1 = IVP_ZALIGN();
	                for (indy = 0; indy < copyHeight; indy++)
	                {
	                    dvec1    = *srcPtr;
	                    pdvecDst = (xb_vec2Nx8U *) dstPtr;
	                    IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, extraEdgeLeft);
	                    IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
	                    dstPtr += tilePitchBytes;
	                    srcPtr += tilePitchBytes;
	                }
	            }

	            if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0)
	            {
	                x2             = pTile->x + (tileWidth - 1) + tileEdgeRight;
	                extraEdgeRight = x2 - (frameWidth - 1);
	                dstPtr         = (((uint8_t *) pTile->pData) - (tileEdgeTop * tilePitchBytes)) + ((tileWidth + tileEdgeRight) - extraEdgeRight );
	                srcPtr         = dstPtr - 1;
	                copyHeight     = tileEdgeTop + tileHeight + tileEdgeBottom;

	                vas1 = IVP_ZALIGN();
	                for (indy = 0; indy < copyHeight; indy++)
	                {
	                    dvec1    = *srcPtr;
	                    pdvecDst = (xb_vec2Nx8U *) dstPtr;
	                    IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, extraEdgeRight);
	                    IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
	                    dstPtr += tilePitchBytes;
	                    srcPtr += tilePitchBytes;
	                }
	            }
			}
			else if (bytesPerPix == 2)
			{
				if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0) {
					extraEdgeLeft = tileEdgeLeft - pTile->x;
					dstPtr_16b        = (uint16_t *) pTile->pData - ((tileEdgeTop * pTile->pitch) + tileEdgeLeft); // No need of multiplying by 2
					srcPtr_16b        = dstPtr_16b + extraEdgeLeft;                                                    // No need of multiplying by 2 as pointers are uint16_t *
					copyHeight    = tileEdgeTop + tileHeight + tileEdgeBottom;

					vas1 = IVP_ZALIGN();
					for (indy = 0; indy < copyHeight; indy++) {
						vec1    = *srcPtr_16b;
						pvecDst = (xb_vecNx16 *) dstPtr_16b;
						IVP_SAVNX16_XP(vec1, vas1, pvecDst, extraEdgeLeft * 2);
						IVP_SAPOSNX16_FP(vas1, pvecDst);
						dstPtr_16b += pTile->pitch;
						srcPtr_16b += pTile->pitch;
					}
				}

				if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0) {
					x2             = pTile->x + tileWidth + tileEdgeRight;
					extraEdgeRight = x2 - frameWidth;
					dstPtr_16b         = (((uint16_t *) pTile->pData) - (tileEdgeTop * pTile->pitch)) + ((tileWidth + tileEdgeRight) - extraEdgeRight );
					srcPtr_16b         = dstPtr_16b - 1;
					copyHeight     = tileEdgeTop + tileHeight + tileEdgeBottom;

					vas1 = IVP_ZALIGN();
					for (indy = 0; indy < copyHeight; indy++) {
						vec1    = *srcPtr_16b;
						pvecDst = (xb_vecNx16 *) dstPtr_16b;
						IVP_SAVNX16_XP(vec1, vas1, pvecDst, extraEdgeRight * 2);
						IVP_SAPOSNX16_FP(vas1, pvecDst);
						dstPtr_16b += pTile->pitch;
						srcPtr_16b += pTile->pitch;
					}
				}
			}
			else if (bytesPerPix == 4)
			{
				if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0) {
					extraEdgeLeft = tileEdgeLeft - pTile->x;
					dstPtr_32b        = (uint32_t *) pTile->pData - ((tileEdgeTop * pTile->pitch) + tileEdgeLeft); // No need of multiplying by 2
					srcPtr_32b        = dstPtr_32b + extraEdgeLeft;                                                    // No need of multiplying by 2 as pointers are uint32_t *
					copyHeight    = tileEdgeTop + tileHeight + tileEdgeBottom;

					vas1 = IVP_ZALIGN();
					for (indy = 0; indy < copyHeight; indy++) {
						hvec1    = *srcPtr_32b;
						phvecDst = (xb_vecN_2x32Uv *) dstPtr_32b;
						IVP_SAVN_2X32_XP(hvec1, vas1, phvecDst, extraEdgeLeft * 4);
						IVP_SAPOSN_2X32_FP(vas1, (xb_vecN_2x32v *)phvecDst);
						dstPtr_32b += pTile->pitch;
						srcPtr_32b += pTile->pitch;
					}
				}

				if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0) {
					x2             = pTile->x + tileWidth + tileEdgeRight;
					extraEdgeRight = x2 - frameWidth;
					dstPtr_32b         = (((uint32_t *) pTile->pData) - (tileEdgeTop * pTile->pitch)) + ((tileWidth + tileEdgeRight) - extraEdgeRight );
					srcPtr_32b         = dstPtr_32b - 1;
					copyHeight     = tileEdgeTop + tileHeight + tileEdgeBottom;

					vas1 = IVP_ZALIGN();
					for (indy = 0; indy < copyHeight; indy++) {
						vec1    = *srcPtr_32b;
						phvecDst = (xb_vecN_2x32Uv *) dstPtr_32b;
						IVP_SAVN_2X32_XP(hvec1, vas1, phvecDst, extraEdgeRight * 4);
						IVP_SAPOSN_2X32_FP(vas1, (xb_vecN_2x32v *)phvecDst);
						dstPtr_32b += pTile->pitch;
						srcPtr_32b += pTile->pitch;
					}
				}
	        }
        }
        else
        {
            padVal = 0;
            if (pFrame->paddingType == FRAME_CONSTANT_PADDING)
            {
                padVal = pFrame->paddingVal;
            }
            dvec1 = padVal;
			if(bytesPerPix == 1)
			{
				dvec1 = padVal;
			}
			else if(bytesPerPix == 2)
			{
				xb_vecNx16U vec =  padVal;
				dvec1 = IVP_MOV2NX8U_FROMNX16(vec);
			}
			else if(bytesPerPix == 4)
			{
				xb_vecN_2x32Uv hvec =  padVal;
				dvec1 = IVP_MOV2NX8U_FROMNX16(IVP_MOVNX16_FROMN_2X32U(hvec));
			}

            if (!(pTile->status & XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED))
            {
                if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0)
                {
                    x1            = pTile->x - tileEdgeLeft;
                    extraEdgeLeft = -x1;
                    dstPtr        = (uint8_t *) pTile->pData - (((tileEdgeTop * tilePitchBytes) + tileEdgeLeft)* bytesPerPix);
                    copyHeight    = tileEdgeTop + tileHeight + tileEdgeBottom;

                    pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    vas1     = IVP_ZALIGN();
                    for (indy = 0; indy < copyHeight; indy++)
                    {
                        for (wb = extraEdgeLeft * bytesPerPix; wb > 0; wb -= (2 * IVP_SIMD_WIDTH))
                        {
                            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
                        }
                        IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
                        dstPtr  += tilePitchBytes * bytesPerPix;
                        pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    }
                }

                if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0)
                {
                    x2             = pTile->x + (tileWidth - 1) + tileEdgeRight;
                    extraEdgeRight = x2 - (frameWidth - 1);
                    x2             = frameWidth - 1;
                    dstPtr         = (((uint8_t *) pTile->pData) - (tileEdgeTop * tilePitchBytes * bytesPerPix)) + (((tileWidth + tileEdgeRight) - extraEdgeRight )* bytesPerPix);
                    copyHeight     = tileEdgeTop + tileHeight + tileEdgeBottom;

                    pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    vas1     = IVP_ZALIGN();
                    for (indy = 0; indy < copyHeight; indy++)
                    {
                        for (wb = extraEdgeRight * bytesPerPix; wb > 0; wb -= (2 * IVP_SIMD_WIDTH))
                        {
                            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
                        }
                        IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
                        dstPtr  += tilePitchBytes * bytesPerPix;
                        pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    }
                }

                if ((pTile->status & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) != 0)
                {
                    y1           = pTile->y - tileEdgeTop;
                    extraEdgeTop = -y1;
                    dstPtr       = (uint8_t *) pTile->pData - (((tileEdgeTop * tilePitchBytes) + tileEdgeLeft) * bytesPerPix);
                    copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * bytesPerPix;

                    pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    vas1     = IVP_ZALIGN();
                    for (indy = 0; indy < extraEdgeTop; indy++)
                    {
                        for (wb = copyRowBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH))
                        {
                            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
                        }
                        IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
                        dstPtr  += tilePitchBytes * bytesPerPix;
                        pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    }
                }

                if ((pTile->status & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) != 0)
                {
                    y2              = pTile->y + tileHeight + tileEdgeBottom;
                    extraEdgeBottom = y2 - frameHeight;
                    dstPtr          = (uint8_t *) pTile->pData + ((((frameHeight - pTile->y) * tilePitchBytes) - tileEdgeLeft) * bytesPerPix);
                    copyRowBytes    = (tileEdgeLeft + tileWidth + tileEdgeRight) * bytesPerPix;

                    pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    vas1     = IVP_ZALIGN();
                    for (indy = 0; indy < extraEdgeBottom; indy++)
                    {
                        for (wb = copyRowBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH))
                        {
                            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
                        }
                        IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
                        dstPtr  += tilePitchBytes * bytesPerPix;
                        pdvecDst = (xb_vec2Nx8U *) dstPtr;
                    }
                }
            }
            else
            {
                // Tile is not part of frame. Make it constant
                dstPtr       = (uint8_t *) pTile->pData - (((tileEdgeTop * tilePitchBytes) + tileEdgeLeft) * bytesPerPix);
                copyHeight   = tileHeight + tileEdgeTop + tileEdgeBottom;
                copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * bytesPerPix;
                copyBufferEdgeDataH(NULL, dstPtr, copyRowBytes, bytesPerPix, copyHeight, (tilePitchBytes * bytesPerPix), pFrame->paddingType, pFrame->paddingVal);
            }
        }

        pTile->status = pTile->status & ~(XV_TILE_STATUS_EDGE_PADDING_NEEDED | XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED);
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvPadEdges16()
 *
 * DESCRIPTION:
 *     Pads edges of the given 16b tile. If FRAME_EDGE_PADDING mode is used,
 *     padding is done using edge values of the frame else if
 *     FRAME_CONSTANT_PADDING or FRAME_ZERO_PADDING mode is used,
 *     constant or zero value is padded
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
// xvPadEdges should be used with Fast functions
int32_t xvPadEdges16(xvTileManager *pxvTM, xvTile *pTile)
{

    return(xvPadEdges(pxvTM, pTile));
}

/**********************************************************************************
 * FUNCTION: xvCheckInputTileFree()
 *
 * DESCRIPTION:
 *     A tile is said to be free if all data transfers pertaining to data resue
 *     from this tile is completed
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if input tile is free and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */

int32_t xvCheckInputTileFree(xvTileManager *pxvTM, xvTile const *pTile)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_TILE(pTile, pxvTM);
    if (pTile->reuseCount == 0)
    {
        return(1);
    }
    else
    {
        return(0);
    }
}

/**********************************************************************************
 * FUNCTION: xvWaitForTileMultiChannel()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     int32_t       ch                       iDMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForTileMultiChannel(int32_t ch, xvTileManager *pxvTM, xvTile const *pTile)
{
	XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    XV_CHECK_TILE(pTile, pxvTM);
    int32_t status = 0;
    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        status = xvCheckTileReadyMultiChannel(ch, pxvTM, pTile);
    }
    return(pxvTM->idmaErrorFlag[ch]);
}

/**********************************************************************************
 * FUNCTION: xvSleepForTileMultiChannel()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     int32_t       ch                       iDMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForTileMultiChannel(int32_t ch, xvTileManager *pxvTM, xvTile const *pTile)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    XV_CHECK_TILE(pTile, pxvTM);
#if defined (XTENSA_SWVERSION_RH_2018_6) && (XTENSA_SWVERSION > XTENSA_SWVERSION_RH_2018_6)
    DECLARE_PS();
#endif
    IDMA_DISABLE_INTS();
    int32_t status = xvCheckTileReadyMultiChannel(ch, (pxvTM), (pTile));
    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        idma_sleep(ch);
        status = xvCheckTileReadyMultiChannel(ch,(pxvTM), (pTile));
    }
    IDMA_ENABLE_INTS();
    return(pxvTM->idmaErrorFlag[ch]);
}



/**********************************************************************************
 * FUNCTION: xvWaitForiDMAMultiChannel()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given DMA index is completed.
 *
 * INPUTS:
 *     int32_t       ch                       iDMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     uint32_t      dmaIndex                 DMA index
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForiDMAMultiChannel(int32_t ch, xvTileManager const *pxvTM, uint32_t dmaIndex)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    int32_t status = 0;

    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        status = idma_desc_done(ch, (dmaIndex));
    }
    return(pxvTM->idmaErrorFlag[ch]);
}

/**********************************************************************************
 * FUNCTION: xvSleepForiDMAMultiChannel()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given DMA index is completed.
 *
 * INPUTS:
 *     int32_t       ch                       iDMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     uint32_t      dmaIndex                 DMA index
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForiDMAMultiChannel(int32_t ch, xvTileManager const *pxvTM, uint32_t dmaIndex)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
#if defined (XTENSA_SWVERSION_RH_2018_6) && (XTENSA_SWVERSION > XTENSA_SWVERSION_RH_2018_6)
    DECLARE_PS();
#endif
    IDMA_DISABLE_INTS();
    int32_t status = idma_desc_done(ch, (dmaIndex));
    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        idma_sleep(ch);
        status = idma_desc_done(ch, (dmaIndex));
    }
    IDMA_ENABLE_INTS();
    return(pxvTM->idmaErrorFlag[ch]);
}

/**********************************************************************************
 * FUNCTION: xvWaitForTileFastMultiChannel()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     int32_t       ch                       DMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForTileFastMultiChannel(int32_t ch, xvTileManager const *pxvTM, xvTile  *pTile)
{
    int32_t status = 0;

    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        status = idma_desc_done(ch, (pTile)->dmaIndex);
    }
    (pTile)->status = (pTile)->status & ~XV_TILE_STATUS_DMA_ONGOING;
    return(pxvTM->idmaErrorFlag[ch]);
}

/**********************************************************************************
 * FUNCTION: xvSleepForTileFastMultiChannel()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     int32_t       ch                       DMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForTileFastMultiChannel(int32_t ch, xvTileManager const *pxvTM, xvTile *pTile)
{
#if defined (XTENSA_SWVERSION_RH_2018_6) && (XTENSA_SWVERSION > XTENSA_SWVERSION_RH_2018_6)
    DECLARE_PS();
#endif
    IDMA_DISABLE_INTS();
    int32_t status = idma_desc_done(ch, (pTile)->dmaIndex);
    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        idma_sleep(ch);
        status = idma_desc_done(ch, (pTile)->dmaIndex);
    }
    IDMA_ENABLE_INTS();
    (pTile)->status = (pTile)->status & ~XV_TILE_STATUS_DMA_ONGOING;
    return(pxvTM->idmaErrorFlag[ch]);
}
/**********************************************************************************
 * FUNCTION: xvGetErrorInfo()
 *
 * DESCRIPTION:
 *
 *     Prints the most recent error information.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *
 * OUTPUTS:
 *     It returns the most recent error code.
 *
 ********************************************************************************** */

xvError_t xvGetErrorInfo(xvTileManager const *pxvTM)
{
	XV_CHECK_ERROR(pxvTM == NULL,   , XV_ERROR_TILE_MANAGER_NULL, "NULL TM Pointer");
    return(pxvTM->errFlag);
}

#if (SUPPORT_3D_TILES == 1)
/**********************************************************************************
 * FUNCTION: xvAllocateFrame3D()
 *
 * DESCRIPTION:
 *     Allocates single 3D frame. It does not allocate buffer required for 3Dframe data.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns the pointer to allocated frame.
 *     Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate frame data buffer.
 *
 ********************************************************************************** */

xvFrame3D *xvAllocateFrame3D(xvTileManager *pxvTM)
{
    int32_t indx, indxArr, indxShift, allocFlags;
    xvFrame3D *pFrame3D = NULL;
    XV_CHECK_ERROR(pxvTM == NULL,   , ((xvFrame3D *) ((void *) (XVTM_ERROR))), "NULL TM Pointer");

    pxvTM->errFlag = XV_ERROR_SUCCESS;

    for (indx = 0; indx < MAX_NUM_FRAMES3D; indx++)
    {
        indxArr    = indx >> 5;
        indxShift  = indx & 0x1F;
        allocFlags = pxvTM->frame3DAllocFlags[indxArr];
        if (((allocFlags >> indxShift) & 0x1) == 0)
        {
            break;
        }
    }

    if (indx < MAX_NUM_FRAMES3D)
    {
        pFrame3D                          = &(pxvTM->frame3DArray[indx]);
        pxvTM->frame3DAllocFlags[indxArr] = allocFlags | (0x1 << indxShift);
        pxvTM->frame3DCount++;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_FRAME_BUFFER_FULL;
        return((xvFrame3D *) ((void *) XVTM_ERROR));
    }

    return(pFrame3D);
}

/**********************************************************************************
 * FUNCTION: xvFreeFrame3D()
 *
 * DESCRIPTION:
 *     Releases the given 3D frame. Does not release associated frame data buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     xvFrame3D       *pFrame3D    3D Frame that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeFrame3D(xvTileManager *pxvTM, xvFrame3D const *pFrame3D)
{
    int32_t indx, indxArr, indxShift, allocFlags;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR(pFrame3D == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL;  , XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    for (indx = 0; indx < MAX_NUM_FRAMES3D; indx++)
    {
        if (&(pxvTM->frame3DArray[indx]) == pFrame3D)
        {
            break;
        }
    }

    if (indx < MAX_NUM_FRAMES3D)
    {
        indxArr                         = indx >> 5;
        indxShift                       = indx & 0x1F;
        allocFlags                      = pxvTM->frame3DAllocFlags[indxArr];
        pxvTM->frame3DAllocFlags[indxArr] = allocFlags & ~(0x1 << indxShift);
        pxvTM->frame3DCount--;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_BAD_ARG;
        return(XVTM_ERROR);
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllFrames3D()
 *
 * DESCRIPTION:
 *     Releases all allocated 3D frames.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllFrames3D(xvTileManager *pxvTM)
{
    int32_t index;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    pxvTM->frameCount = 0;
    for (index = 0; index < ((MAX_NUM_FRAMES3D + 31) / 32); index++)
    {
        pxvTM->frame3DAllocFlags[index] = 0x00000000;
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvAllocateTile3D()
 *
 * DESCRIPTION:
 *     Allocates single 3D tile. It does not allocate buffer required for tile data.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns the pointer to allocated tile.
 *     Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate tile data buffer
 *
 ********************************************************************************** */

xvTile3D *xvAllocateTile3D(xvTileManager *pxvTM)
{
    int32_t indx, indxArr, indxShift, allocFlags;
    xvTile3D *pTile3D = NULL;
    XV_CHECK_ERROR(pxvTM == NULL,   , ((xvTile3D *) ((void *) XVTM_ERROR)), "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    for (indx = 0; indx < MAX_NUM_TILES3D; indx++)
    {
        indxArr    = indx >> 5;
        indxShift  = indx & 0x1F;
        allocFlags = pxvTM->tile3DAllocFlags[indxArr];
        if (((allocFlags >> indxShift) & 0x1) == 0)
        {
            break;
        }
    }

    if (indx < MAX_NUM_TILES3D)
    {
        pTile3D                          = &(pxvTM->tile3DArray[indx]);
        pxvTM->tile3DAllocFlags[indxArr] = allocFlags | (0x1 << indxShift);
        pxvTM->tile3DCount++;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_TILE_BUFFER_FULL;
        return((xvTile3D *) ((void *) XVTM_ERROR));
    }

    return(pTile3D);
}

/**********************************************************************************
 * FUNCTION: xvFreeTile3D()
 *
 * DESCRIPTION:
 *     Releases the given 3D tile. Does not release associated tile data buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     xvTile3D        *pTile3D      3D Tile that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */
int32_t xvFreeTile3D(xvTileManager *pxvTM, xvTile3D const *pTile3D)
{
    int32_t indx, indxArr, indxShift, allocFlags;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR(pTile3D == NULL,  pxvTM->errFlag = XV_ERROR_TILE_NULL; , XVTM_ERROR, "NULL TM Pointer");

    for (indx = 0; indx < MAX_NUM_TILES3D; indx++)
    {
        if (&(pxvTM->tile3DArray[indx]) == pTile3D)
        {
            break;
        }
    }

    if (indx < MAX_NUM_TILES3D)
    {
        indxArr                        = indx >> 5;
        indxShift                      = indx & 0x1F;
        allocFlags                     = pxvTM->tile3DAllocFlags[indxArr];
        pxvTM->tile3DAllocFlags[indxArr] = allocFlags & ~(0x1 << indxShift);
        pxvTM->tileCount--;
    }
    else
    {
        pxvTM->errFlag = XV_ERROR_BAD_ARG;
        return(XVTM_ERROR);
    }
    return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllTiles3D()
 *
 * DESCRIPTION:
 *     Releases all allocated 3D tiles.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllTiles3D(xvTileManager *pxvTM)
{
    int32_t index;
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    pxvTM->tile3DCount = 0;
    for (index = 0; index < ((MAX_NUM_TILES3D + 31) / 32); index++)
    {
        pxvTM->tile3DAllocFlags[index] = 0x00000000;
    }
    return(XVTM_SUCCESS);
}


/**********************************************************************************
 * FUNCTION: xvReqTileTransferInMultiChannel3D()
 *
 * DESCRIPTION:
 *     Requests data transfer from 3D frame present in system memory to local 3D tile memory.
 *
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInMultiChannel3D(int32_t dmaChannel, xvTileManager *pxvTM, xvTile3D *pTile3D, int32_t interruptOnCompletion)
{
    xvFrame3D *pFrame3D;
    int32_t frameWidth, frameHeight, frameDepth, framePitchBytes,  tileWidth, tileHeight, tileDepth, tilePitchBytes;
    int32_t statusFlag, x1, y1, x2, y2, z1, z2, dmaHeight, dmaWidthBytes, dmaIndex;
    int32_t tileIndex;
    int16_t framePadLeft, framePadRight, framePadTop, framePadBottom, framePadFront, framePadBack;
    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom, tileEdgeFront, tileEdgeBack, num2DTiles;
    int16_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight, extraEdgeFront, extraEdgeBack;
    int8_t pixWidth, pixRes, framePadType;
    int32_t framePadVal;
    uint64_t srcPtr64, dstPtr64;
    uint8_t *dstPtr, *edgePtr;
    int32_t framePitchTileBytes, tilePitchTileBytes;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((interruptOnCompletion < 0),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , XVTM_ERROR, "XV_ERROR_BAD_ARG");

    XV_CHECK_TILE_3D(pTile3D, pxvTM);
    pFrame3D = pTile3D->pFrame;
    XV_CHECK_ERROR(pFrame3D == NULL,  pxvTM->errFlag = XV_ERROR_FRAME_NULL;  , XVTM_ERROR, "XV_ERROR_FRAME_NULL");
    XV_CHECK_ERROR(((pFrame3D->pFrameBuff == NULL) || (pFrame3D->pFrameData == NULL)),  pxvTM->errFlag = XV_ERROR_FRAME_NULL;  , XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile3D));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile3D));
    int32_t bytePerPel  = bytesPerPix / channel;

    XV_CHECK_ERROR((pFrame3D->pixelRes != bytePerPel),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((pFrame3D->numChannels != channel),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , XVTM_ERROR, "XV_ERROR_BAD_ARG");

    frameWidth        = pFrame3D->frameWidth;
    frameHeight       = pFrame3D->frameHeight;
    framePadLeft      = pFrame3D->leftEdgePadWidth;
    framePadRight     = pFrame3D->rightEdgePadWidth;
    framePadTop       = pFrame3D->topEdgePadHeight;
    framePadBottom    = pFrame3D->bottomEdgePadHeight;
    frameDepth        = pFrame3D->frameDepth;
    framePadFront     = pFrame3D->frontEdgePadDepth;
    framePadBack      = pFrame3D->backEdgePadDepth;
    pixRes            = pFrame3D->pixelRes;
    pixWidth          = pFrame3D->pixelRes * pFrame3D->numChannels;
    framePitchBytes   = pFrame3D->framePitch * pixRes;
    framePitchTileBytes = pFrame3D->frame2DFramePitch * pFrame3D->pixelRes;

    tileWidth      = pTile3D->width;
    tileHeight     = pTile3D->height;
    tilePitchBytes = pTile3D->pitch * pixRes;
    tileEdgeLeft   = pTile3D->tileEdgeLeft;
    tileEdgeRight  = pTile3D->tileEdgeRight;
    tileEdgeTop    = pTile3D->tileEdgeTop;
    tileEdgeBottom = pTile3D->tileEdgeBottom;
    tileEdgeFront  = pTile3D->tileEdgeFront;
    tileEdgeBack   = pTile3D->tileEdgeBack;
    tileDepth      = pTile3D->depth;
    tilePitchTileBytes = pTile3D->Tile2Dpitch * pFrame3D->pixelRes;

    statusFlag = pTile3D->status;


    // Check front and back borders
    z1 = pTile3D->z - tileEdgeFront;
    if (z1 > (frameDepth + framePadBack))
    {
        z1 = frameDepth + framePadBack;
    }
    if (z1 < -framePadFront)
    {
        extraEdgeFront = -framePadFront - z1;
        z1           = -framePadFront;
        statusFlag  |= XV_TILE_STATUS_FRONT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeFront = 0;
    }

    z2 = pTile3D->z + (tileDepth - 1) + tileEdgeBack;
    if (z2 < -framePadFront)
    {
        z2 = (int32_t) (-framePadFront - 1);
    }
    if (z2 > ((frameDepth - 1) + framePadBack))
    {
        extraEdgeBack = z2 - ((frameDepth - 1) + framePadBack);
        z2              = (frameDepth - 1) + framePadBack;
        statusFlag     |= XV_TILE_STATUS_BACK_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeBack = 0;
    }


    // 1. CHECK IF EXTRA PADDING NEEDED
    // Check top and bottom borders
    y1 = pTile3D->y - tileEdgeTop;
    if (y1 > (frameHeight + framePadBottom))
    {
        y1 = frameHeight + framePadBottom;
    }
    if (y1 < -framePadTop)
    {
        extraEdgeTop = -framePadTop - y1;
        y1           = -framePadTop;
        statusFlag  |= XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeTop = 0;
    }

    y2 = pTile3D->y + (tileHeight - 1) + tileEdgeBottom;
    if (y2 < -framePadTop)
    {
        y2 = (int32_t) (-framePadTop - 1);
    }
    if (y2 > ((frameHeight - 1) + framePadBottom))
    {
        extraEdgeBottom = y2 - ((frameHeight - 1) + framePadBottom);
        y2              = (frameHeight - 1) + framePadBottom;
        statusFlag     |= XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeBottom = 0;
    }

    // Check left and right borders
    x1 = pTile3D->x - tileEdgeLeft;
    if (x1 > (frameWidth + framePadRight))
    {
        x1 = frameWidth + framePadRight;
    }
    if (x1 < -framePadLeft)
    {
        extraEdgeLeft = -framePadLeft - x1;
        x1            = -framePadLeft;
        statusFlag   |= XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeLeft = 0;
    }

    x2 = pTile3D->x + (tileWidth - 1) + tileEdgeRight;
    if (x2 < -framePadLeft)
    {
        x2 = (int32_t) (-framePadLeft - 1);
    }
    if (x2 > ((frameWidth - 1) + framePadRight))
    {
        extraEdgeRight = x2 - ((frameWidth - 1) + framePadRight);
        x2             = (frameWidth - 1) + framePadRight;
        statusFlag    |= XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED;
    }
    else
    {
        extraEdgeRight = 0;
    }

    // 2. FILL ALL TILE and DMA RELATED DATA
    // No Need to align srcPtr and dstPtr as DMA does not need aligned start
    srcPtr64        = pFrame3D->pFrameData + (x1 * pixWidth) + (y1 * framePitchBytes)   + (z1*framePitchTileBytes);
    dmaHeight     = (y2 - y1) + 1;
    dmaWidthBytes = ((x2 - x1) + 1) * pixWidth;
    num2DTiles = (z2 - z1) + 1;

    edgePtr       = ((uint8_t *) pTile3D->pData) - (((int32_t) tileEdgeFront * tilePitchTileBytes) + ((int32_t) tileEdgeTop * tilePitchBytes) + \
                                                    ((int32_t) tileEdgeLeft * (int32_t) pixWidth));
    dstPtr        = edgePtr + (extraEdgeTop * tilePitchBytes) + (extraEdgeLeft * pixWidth) + (extraEdgeFront * tilePitchTileBytes );// For DMA

    // 
    if ((dmaHeight > 0) && (dmaWidthBytes > 0) && (num2DTiles > 0))
    {
        {
            dstPtr64 = (uint64_t)((uint32_t)dstPtr);
            dmaIndex = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, dmaWidthBytes,
                                                               dmaHeight, framePitchBytes, tilePitchBytes, interruptOnCompletion, framePitchTileBytes, tilePitchTileBytes, num2DTiles);

        }
    }
    else
    {

        //Tile is completely outside frame boundaries.
        //NOTE: EDGE PADDING NOT SUPPORTED for this scenario.
        //initialize first row of first 2D tile with zero/constant padding.

        statusFlag  = XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
        framePadVal     = pFrame3D->paddingVal;
        framePadType    = pFrame3D->paddingType;
        if (framePadType != FRAME_CONSTANT_PADDING)
        {
            framePadVal = 0;
        }

        int NumBytes = pixWidth*(tileWidth + tileEdgeLeft + tileEdgeRight);
        COPY_PADDING_DATA(edgePtr, framePadVal, pixWidth, NumBytes);
        //now set up a 2D DMA to copy first row to remaining rows of the tile
        srcPtr64 = (uint64_t)((uint32_t)edgePtr);
        dstPtr64 = srcPtr64 + tilePitchBytes;

        dmaIndex = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, NumBytes,
                                                           (tileEdgeTop + tileHeight + tileEdgeBottom - 1), 0, tilePitchBytes, interruptOnCompletion, 0, 0, 1);

    }
    statusFlag |= ( (interruptOnCompletion & 1) << XV_TILE_STATUS_INTERRUPT_ON_COMPLETION_SHIFT);
    pTile3D->status = statusFlag | XV_TILE_STATUS_DMA_ONGOING;
    tileIndex = (pxvTM->tile3DDMAstartIndex[dmaChannel] + pxvTM->tile3DDMApendingCount[dmaChannel]) % MAX_NUM_DMA_QUEUE_LENGTH;
    pxvTM->tile3DDMApendingCount[dmaChannel]++;
    pxvTM->tile3DProcQueue[dmaChannel][tileIndex] = pTile3D;
    pTile3D->dmaIndex                 = dmaIndex;
    return(XVTM_SUCCESS);
}



/**********************************************************************************
 * FUNCTION: xvCheckTileReadyMultiChannel3D()
 *
 * DESCRIPTION:
 *     Checks if DMA transfer for given 3D tile is completed.
 *     It checks all the tile in the transfer request buffer
 *     before the given tile and updates their respective
 *     status. It pads edges wherever required.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile3D        *pTile3D               Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ***********************************************************************************/

int32_t xvCheckTileReadyMultiChannel3D(int32_t dmaChannel, xvTileManager *pxvTM, xvTile3D const *pTile3D)
{
    int32_t loopInd, index, retVal, dmaIndex, dmaIndexTemp, statusFlag, doneCount;
    xvTile3D *pTile13D;
    xvFrame3D *pFrame3D;
    int32_t frameWidth, frameHeight, frameDepth, framePitchBytes, tileWidth, tileHeight, tileDepth, tilePitchBytes, tilePitch2DtileBytes, tile3DDMApendingCount;
    int32_t x1, y1, x2, y2, z1, z2;
    int16_t framePadLeft, framePadRight, framePadTop, framePadBottom, framePadFront, framePadBack;
    int16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom, tileEdgeFront, tileEdgeBack;
    int16_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight, extraEdgeFront, extraEdgeBack;
    int8_t pixWidth, framePadType;
    int32_t framePadVal;
    uint64_t srcPtr64, dstPtr64;
    uint32_t src2DtilePitch;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;

    XV_CHECK_TILE_3D(pTile3D, pxvTM);
    if (pTile3D->status == 0)
    {
        return(1);
    }

    doneCount = 0;
    index     = pxvTM->tile3DDMAstartIndex[dmaChannel];
    tile3DDMApendingCount = pxvTM->tile3DDMApendingCount[dmaChannel];
    for (loopInd = 0; loopInd < tile3DDMApendingCount; loopInd++)
    {
        index    = (pxvTM->tile3DDMAstartIndex[dmaChannel] + loopInd) % MAX_NUM_DMA_QUEUE_LENGTH;
        pTile13D   = pxvTM->tile3DProcQueue[dmaChannel][index % MAX_NUM_DMA_QUEUE_LENGTH];
        dmaIndex = pTile13D->dmaIndex;


        {
            retVal = xvCheckForIdmaIndexMultiChannel(dmaChannel, pxvTM, dmaIndex);
            if (retVal == 1)
            {
                statusFlag = pTile13D->status;
                int32_t InterruptFlag = ( statusFlag >> XV_TILE_STATUS_INTERRUPT_ON_COMPLETION_SHIFT) & 1;


                statusFlag = statusFlag & ~(XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_INTERRUPT_ON_COMPLETION);

                pFrame3D        = pTile13D->pFrame;
                pixWidth        = pFrame3D->pixelRes * pFrame3D->numChannels;
                frameWidth      = pFrame3D->frameWidth;
                frameHeight     = pFrame3D->frameHeight;
                frameDepth      = pFrame3D->frameDepth;
                framePitchBytes = pFrame3D->framePitch * pFrame3D->pixelRes;
                framePadLeft    = pFrame3D->leftEdgePadWidth;
                framePadRight   = pFrame3D->rightEdgePadWidth;
                framePadTop     = pFrame3D->topEdgePadHeight;
                framePadBottom  = pFrame3D->bottomEdgePadHeight;
                framePadType    = pFrame3D->paddingType;
                framePadVal     = pFrame3D->paddingVal;
                framePadFront   = pFrame3D->frontEdgePadDepth;
                framePadBack    = pFrame3D->backEdgePadDepth;


                tileWidth      = pTile13D->width;
                tileHeight     = pTile13D->height;
                tileDepth      =  pTile13D->depth;
                tilePitchBytes = pTile13D->pitch * pFrame3D->pixelRes;
                tileEdgeLeft   = pTile13D->tileEdgeLeft;
                tileEdgeRight  = pTile13D->tileEdgeRight;
                tileEdgeTop    = pTile13D->tileEdgeTop;
                tileEdgeBottom = pTile13D->tileEdgeBottom;
                tilePitch2DtileBytes = pTile13D->Tile2Dpitch*pFrame3D->pixelRes;
                tileEdgeFront  = pTile13D->tileEdgeFront;
                tileEdgeBack   = pTile13D->tileEdgeBack;

                while (((statusFlag & XV_TILE_STATUS_EDGE_PADDING_NEEDED) != 0) || ((statusFlag & XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED) != 0) )
                {


                    // Check front and back borders
                    z1 = pTile3D->z - tileEdgeFront;
                    if (z1 > (frameDepth + framePadBack))
                    {
                        z1 = frameDepth + framePadBack;
                    }
                    if (z1 < -framePadFront)
                    {
                        extraEdgeFront = -framePadFront - z1;
                        z1           = -framePadFront;
                    }
                    else
                    {
                        extraEdgeFront = 0;
                    }

                    z2 = pTile3D->z + (tileDepth - 1) + tileEdgeBack;
                    if (z2 < -framePadFront)
                    {
                        z2 = (int32_t) (-framePadFront - 1);
                    }
                    if (z2 > ((frameDepth - 1) + framePadBack))
                    {
                        extraEdgeBack = z2 - ((frameDepth - 1) + framePadBack);
                        z2              = (frameDepth - 1) + framePadBack;
                    }
                    else
                    {
                        extraEdgeBack = 0;
                    }


                    // 1. CHECK IF EXTRA PADDING NEEDED
                    // Check top and bottom borders
                    y1 = pTile3D->y - tileEdgeTop;
                    if (y1 > (frameHeight + framePadBottom))
                    {
                        y1 = frameHeight + framePadBottom;
                    }
                    if (y1 < -framePadTop)
                    {
                        extraEdgeTop = -framePadTop - y1;
                        y1           = -framePadTop;
                    }
                    else
                    {
                        extraEdgeTop = 0;
                    }

                    y2 = pTile3D->y + (tileHeight - 1) + tileEdgeBottom;
                    if (y2 < -framePadTop)
                    {
                        y2 = (int32_t) (-framePadTop - 1);
                    }
                    if (y2 > ((frameHeight - 1) + framePadBottom))
                    {
                        extraEdgeBottom = y2 - ((frameHeight - 1) + framePadBottom);
                        y2              = (frameHeight - 1) + framePadBottom;
                    }
                    else
                    {
                        extraEdgeBottom = 0;
                    }

                    // Check left and right borders
                    x1 = pTile3D->x - tileEdgeLeft;
                    if (x1 > (frameWidth + framePadRight))
                    {
                        x1 = frameWidth + framePadRight;
                    }
                    if (x1 < -framePadLeft)
                    {
                        extraEdgeLeft = -framePadLeft - x1;
                        x1            = -framePadLeft;
                    }
                    else
                    {
                        extraEdgeLeft = 0;
                    }

                    x2 = pTile3D->x + (tileWidth - 1) + tileEdgeRight;
                    if (x2 < -framePadLeft)
                    {
                        x2 = (int32_t) (-framePadLeft - 1);
                    }
                    if (x2 > ((frameWidth - 1) + framePadRight))
                    {
                        extraEdgeRight = x2 - ((frameWidth - 1) + framePadRight);
                        x2             = (frameWidth - 1) + framePadRight;
                    }
                    else
                    {
                        extraEdgeRight = 0;
                    }


                    if ((statusFlag & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0)
                    {
                        if (statusFlag &  XV_TILE_STATUS_LEFT_EDGE_PADDING_ONGOING)
                        {
                            //We just finished this iDMA operation.
                            statusFlag &= ~(XV_TILE_STATUS_LEFT_EDGE_PADDING_ONGOING | XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED);

                            //free temp buffer
                            if (pTile13D->pTemp != NULL)
                            {
                                xvFreeBuffer(pxvTM, pTile13D->pTemp);
                                pTile13D->pTemp = NULL;
                            }
                        }
                        else
                        {

                            if (framePadType != FRAME_EDGE_PADDING)
                            {
                                //zero or constant padding.
                                //allocate buffer
                                pTile13D->pTemp = xvAllocateBuffer(pxvTM, extraEdgeLeft*pixWidth, XV_MEM_BANK_COLOR_ANY, 64);
                                if (pTile13D->pTemp == NULL)
                                {
                                    return(XVTM_ERROR);
                                }

                                if (framePadType == FRAME_ZERO_PADDING)
                                {
                                    framePadVal = 0;
                                }

                                COPY_PADDING_DATA(pTile13D->pTemp, framePadVal, pixWidth, extraEdgeLeft*pixWidth)

                                //use same source buffer for all 2D tiles and all rows in wach 2D tile.
                                //simply set source row pitch and 2Dtile pitch to zero.


                                dstPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData) - tileEdgeLeft*pixWidth - (tileEdgeTop - extraEdgeTop)*tilePitchBytes - \
                                           (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes;
                                srcPtr64 =  (uint64_t) (uint32_t) pTile13D->pTemp;


                                dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, extraEdgeLeft*pixWidth,
                                                                                       y2 - y1 + 1, 0, tilePitchBytes, InterruptFlag, 0, tilePitch2DtileBytes, z2 - z1 + 1);

                            }
                            else
                            {
                                //edge padding.
                                int i;
                                srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData) - (tileEdgeLeft - extraEdgeLeft)*pixWidth - (tileEdgeTop - extraEdgeTop)*tilePitchBytes - \
                                           (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes;
                                dstPtr64 =  srcPtr64 - pixWidth;


                                for (i = 0; i < extraEdgeLeft; i++)
                                {
                                    dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, pixWidth,
                                                                                           y2 - y1 + 1, tilePitchBytes, tilePitchBytes,  InterruptFlag, tilePitch2DtileBytes, tilePitch2DtileBytes, z2 - z1 + 1);
                                    dstPtr64 -= pixWidth;
                                }

                            }
                            statusFlag |= XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_LEFT_EDGE_PADDING_ONGOING;
                            pTile13D->dmaIndex  = dmaIndexTemp;
                            break;
                        }
                    }


                    if ((statusFlag & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0)
                    {
                        if (statusFlag &  XV_TILE_STATUS_RIGHT_EDGE_PADDING_ONGOING)
                        {
                            //We just finished this iDMA operation.
                            statusFlag &= ~(XV_TILE_STATUS_RIGHT_EDGE_PADDING_ONGOING | XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED);

                            //free temp buffer
                            if (pTile13D->pTemp != NULL)
                            {
                                xvFreeBuffer(pxvTM, pTile13D->pTemp);
                                pTile13D->pTemp = NULL;
                            }
                        }
                        else
                        {

                            if (framePadType != FRAME_EDGE_PADDING)
                            {
                                //zero or constant padding.
                                //allocate buffer
                                pTile13D->pTemp = xvAllocateBuffer(pxvTM, extraEdgeRight*pixWidth, XV_MEM_BANK_COLOR_ANY, 64);
                                if (pTile13D->pTemp == NULL)
                                {
                                    return(XVTM_ERROR);
                                }

                                if (framePadType == FRAME_ZERO_PADDING)
                                {
                                    framePadVal = 0;
                                }

                                COPY_PADDING_DATA(pTile13D->pTemp, framePadVal, pixWidth, extraEdgeRight*pixWidth)

                                dstPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)  - (tileEdgeTop - extraEdgeTop)*tilePitchBytes - \
                                           (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes + (((x2 - pTile13D->x) + 1) * pixWidth);

                                srcPtr64 =  (uint64_t) (uint32_t) pTile13D->pTemp;

                                dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, extraEdgeRight*pixWidth,
                                                                                       y2 - y1 + 1, 0, tilePitchBytes, InterruptFlag, 0, tilePitch2DtileBytes, z2 - z1 + 1);

                            }
                            else
                            {

                                //edge padding.
                                int i;

                                srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)  - (tileEdgeTop - extraEdgeTop)*tilePitchBytes - \
                                           (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes + (tileWidth + tileEdgeRight - extraEdgeRight - 1) * pixWidth;

                                dstPtr64 = srcPtr64 + pixWidth;

                                for (i = 0; i < extraEdgeRight; i++)
                                {
                                    dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, pixWidth,
                                                                                           y2 - y1 + 1, tilePitchBytes, tilePitchBytes,  InterruptFlag, tilePitch2DtileBytes, tilePitch2DtileBytes, z2 - z1 + 1);
                                    dstPtr64 += pixWidth;
                                }

                            }

                            statusFlag |= XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_RIGHT_EDGE_PADDING_ONGOING;
                            pTile13D->dmaIndex  = dmaIndexTemp;
                            break;
                        }
                    }

                    if ((statusFlag & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) != 0)
                    {
                        if (statusFlag &  XV_TILE_STATUS_TOP_EDGE_PADDING_ONGOING)
                        {
                            //We just finished this iDMA operation.
                            statusFlag &= ~(XV_TILE_STATUS_TOP_EDGE_PADDING_ONGOING | XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED);

                            //free temp buffer
                            if (pTile13D->pTemp != NULL)
                            {
                                //This is an error. should not happen.
                                xvFreeBuffer(pxvTM, pTile13D->pTemp);
                                pTile13D->pTemp = NULL;
                            }
                        }
                        else
                        {

                            int RowSize = (tileWidth + tileEdgeRight + tileEdgeLeft)*pixWidth;

                            if (framePadType != FRAME_EDGE_PADDING)
                            {
                                //zero or constant padding.
                                //allocate buffer
                                pTile13D->pTemp = xvAllocateBuffer(pxvTM, RowSize, XV_MEM_BANK_COLOR_ANY, 64);
                                if (pTile13D == NULL)
                                {
                                    return(XVTM_ERROR);
                                }

                                if (framePadType == FRAME_ZERO_PADDING)
                                {
                                    framePadVal = 0;
                                }

                                COPY_PADDING_DATA(pTile13D->pTemp, framePadVal, pixWidth, RowSize)
                                srcPtr64 =  (uint64_t) (uint32_t) pTile13D->pTemp;
                                src2DtilePitch = 0;
                            }
                            else
                            {
                                srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)  - (tileEdgeTop - extraEdgeTop)*tilePitchBytes - (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes - \
                                           tileEdgeLeft* pixWidth;
                                src2DtilePitch = tilePitch2DtileBytes;
                            }

                            dstPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)  - tileEdgeTop*tilePitchBytes - (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes - \
                                       tileEdgeLeft* pixWidth;


                            dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, RowSize,
                                                                                   extraEdgeTop, 0, tilePitchBytes, InterruptFlag, src2DtilePitch, tilePitch2DtileBytes, z2 - z1 + 1);

                            statusFlag |= XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_TOP_EDGE_PADDING_ONGOING;
                            pTile13D->dmaIndex  = dmaIndexTemp;
                            break;

                        }

                    }

                    if ((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) != 0)
                    {
                        if (statusFlag &  XV_TILE_STATUS_BOTTOM_EDGE_PADDING_ONGOING)
                        {
                            //We just finished this iDMA operation.
                            statusFlag &= ~(XV_TILE_STATUS_BOTTOM_EDGE_PADDING_ONGOING | XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED);

                            //free temp buffer
                            if (pTile13D->pTemp != NULL)
                            {
                                xvFreeBuffer(pxvTM, pTile13D->pTemp);
                                pTile13D->pTemp = NULL;
                            }
                        }
                        else
                        {

                            int RowSize = (tileWidth + tileEdgeRight + tileEdgeLeft)*pixWidth;

                            if (framePadType != FRAME_EDGE_PADDING)
                            {
                                //zero or constant padding.
                                //allocate buffer
                                pTile13D->pTemp = xvAllocateBuffer(pxvTM, RowSize, XV_MEM_BANK_COLOR_ANY, 64);
                                if (pTile13D->pTemp == NULL)
                                {
                                    return(XVTM_ERROR);
                                }

                                if (framePadType == FRAME_ZERO_PADDING)
                                {
                                    framePadVal = 0;
                                }

                                COPY_PADDING_DATA(pTile13D->pTemp, framePadVal, pixWidth, RowSize)
                                srcPtr64 =  (uint64_t) (uint32_t) pTile13D->pTemp;
                                src2DtilePitch = 0;
                            }
                            else
                            {

                                srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)  + (tileHeight  + tileEdgeBottom - extraEdgeBottom - 1)*tilePitchBytes - \
                                           tileEdgeLeft* pixWidth - (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes;
                                src2DtilePitch = tilePitch2DtileBytes;
                            }


                            dstPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)  + (tileHeight  + tileEdgeBottom - extraEdgeBottom)*tilePitchBytes - \
                                       tileEdgeLeft* pixWidth - (tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes;


                            dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, RowSize,
                                                                                   extraEdgeBottom, 0, tilePitchBytes, InterruptFlag, src2DtilePitch, tilePitch2DtileBytes, z2 - z1 + 1);

                            statusFlag |= XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_BOTTOM_EDGE_PADDING_ONGOING;
                            pTile13D->dmaIndex  = dmaIndexTemp;
                            break;

                        }
                    }

                    if ((statusFlag & XV_TILE_STATUS_FRONT_EDGE_PADDING_NEEDED) != 0)
                    {
                        if (statusFlag &  XV_TILE_STATUS_FRONT_EDGE_PADDING_ONGOING)
                        {
                            //We just finished this iDMA operation.
                            statusFlag &= ~(XV_TILE_STATUS_FRONT_EDGE_PADDING_ONGOING | XV_TILE_STATUS_FRONT_EDGE_PADDING_NEEDED);

                            //free temp buffer
                            if (pTile13D->pTemp != NULL)
                            {
                                xvFreeBuffer(pxvTM, pTile13D->pTemp);
                                pTile13D->pTemp = NULL;
                            }
                        }
                        else
                        {

                            int RowSize = (tileWidth + tileEdgeRight + tileEdgeLeft)*pixWidth;
                            int srcPitch;

                            if (framePadType != FRAME_EDGE_PADDING)
                            {
                                //zero or constant padding.
                                //allocate buffer
                                pTile13D->pTemp = xvAllocateBuffer(pxvTM, RowSize, XV_MEM_BANK_COLOR_ANY, 64);
                                if (pTile13D->pTemp == NULL)
                                {
                                    return(XVTM_ERROR);
                                }

                                if (framePadType == FRAME_ZERO_PADDING)
                                {
                                    framePadVal = 0;
                                }

                                COPY_PADDING_DATA(pTile13D->pTemp, framePadVal, pixWidth, RowSize)
                                srcPtr64 =  (uint64_t) (uint32_t) pTile13D->pTemp;
                                srcPitch = 0;                                
                            }
                            else
                            {
                                srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)   - (tileEdgeTop*tilePitchBytes) - \
                                           (tileEdgeLeft* pixWidth) - ((tileEdgeFront - extraEdgeFront)*tilePitch2DtileBytes);
                                srcPitch = tilePitchBytes;                                
                            }

                            dstPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)   - (tileEdgeTop*tilePitchBytes) - \
                                       (tileEdgeLeft* pixWidth) - (tileEdgeFront*tilePitch2DtileBytes);


                            dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, RowSize,
                                                                                   tileEdgeTop + tileHeight + tileEdgeBottom, srcPitch, tilePitchBytes, InterruptFlag, 0, tilePitch2DtileBytes, extraEdgeFront);

                            statusFlag |= XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_FRONT_EDGE_PADDING_ONGOING;
                            pTile13D->dmaIndex  = dmaIndexTemp;
                            break;
                        }
                    }

                    if ((statusFlag & XV_TILE_STATUS_BACK_EDGE_PADDING_NEEDED) != 0)
                    {
                        if (statusFlag &  XV_TILE_STATUS_BACK_EDGE_PADDING_ONGOING)
                        {
                            //We just finished this iDMA operation.
                            statusFlag &= ~(XV_TILE_STATUS_BACK_EDGE_PADDING_ONGOING | XV_TILE_STATUS_BACK_EDGE_PADDING_NEEDED);

                            //free temp buffer
                            if (pTile13D->pTemp != NULL)
                            {
                                xvFreeBuffer(pxvTM, pTile13D->pTemp);
                                pTile13D->pTemp = NULL;
                            }
                        }
                        else
                        {

                            int RowSize = (tileWidth + tileEdgeRight + tileEdgeLeft)*pixWidth;
                            int srcPitch;

                            if (framePadType != FRAME_EDGE_PADDING)
                            {
                                //zero or constant padding.
                                //allocate buffer
                                pTile13D->pTemp = xvAllocateBuffer(pxvTM, RowSize, XV_MEM_BANK_COLOR_ANY, 64);
                                if (pTile13D->pTemp == NULL)
                                {
                                    return(XVTM_ERROR);
                                }

                                if (framePadType == FRAME_ZERO_PADDING)
                                {
                                    framePadVal = 0;
                                }

                                COPY_PADDING_DATA(pTile13D->pTemp, framePadVal, pixWidth, RowSize)
                                srcPtr64 =  (uint64_t) (uint32_t) pTile13D->pTemp;
                                srcPitch = 0;                                
                            }
                            else
                            {
                                srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)   - (tileEdgeTop*tilePitchBytes) - \
                                           (tileEdgeLeft* pixWidth) + ((tileDepth  + tileEdgeBack - extraEdgeBack - 1)*tilePitch2DtileBytes);
                                srcPitch = tilePitchBytes;                                
                            }


                            dstPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)   - (tileEdgeTop*tilePitchBytes) - \
                                       (tileEdgeLeft* pixWidth) + ((tileDepth  + tileEdgeBack - extraEdgeBack)*tilePitch2DtileBytes);


                            dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, RowSize,
                                                                                   tileEdgeTop + tileHeight + tileEdgeBottom, srcPitch, tilePitchBytes, InterruptFlag, 0, tilePitch2DtileBytes, extraEdgeBack);

                            statusFlag |= XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_BACK_EDGE_PADDING_ONGOING;
                            pTile13D->dmaIndex  = dmaIndexTemp;
                            break;

                        }
                    }

                    if ((pTile13D->status & XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED) != 0)
                    {

                        if ((statusFlag & XV_TILE_STATUS_DUMMY_DMA_ONGOING) != 0)
                        {
                            //3D DMA is complete.
                            statusFlag = 0;
                        }
                        else
                        {
                            // Tile is not part of frame. Make everything constant
                            //first 2D tile in stack is ready with required padding data.
                            //Set up DMA to copy it to rest of the stack
                            srcPtr64 = ( (uint64_t) (uint32_t) pTile13D->pData)   - (tileEdgeTop*tilePitchBytes) - \
                                       (tileEdgeLeft* pixWidth) - (tileEdgeFront*tilePitch2DtileBytes);

                            dstPtr64 = srcPtr64 + tilePitch2DtileBytes;

                            int num2DTiles = (tileEdgeFront + tileEdgeBack + tileDepth - 1);

                            int RowSize = (tileWidth + tileEdgeRight + tileEdgeLeft)*pixWidth;

                            if (num2DTiles > 0)
                            {
                                dmaIndexTemp = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, RowSize,
                                                                                       tileEdgeTop + tileHeight + tileEdgeBottom, tilePitchBytes, tilePitchBytes, InterruptFlag, tilePitch2DtileBytes, tilePitch2DtileBytes, num2DTiles);

                                statusFlag = XV_TILE_STATUS_DMA_ONGOING | XV_TILE_STATUS_DUMMY_DMA_ONGOING | XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
                                pTile13D->dmaIndex  = dmaIndexTemp;
                                break;
                            }
                            else
                            {
                                //here we are done.
                                statusFlag = 0;
                            }
                        }

                    }
                }


                pTile13D->status = statusFlag;
                if (statusFlag == 0)
                {
                    doneCount++;
                }
                else
                {
                    //restore interrupt flag
                    pTile13D->status |= (InterruptFlag << XV_TILE_STATUS_INTERRUPT_ON_COMPLETION_SHIFT);
                    break;
                }
            }
            else // DMA not done for this tile
            {
                break;
            }
        }

        // Break if we reached the required tile
        if ((pTile13D == pTile3D) && ( pTile13D->status == 0))
        {
            index = (index + 1) % MAX_NUM_DMA_QUEUE_LENGTH;
            break;
        }
    }

    pxvTM->tile3DDMAstartIndex[dmaChannel]   = index;
    pxvTM->tile3DDMApendingCount[dmaChannel] = pxvTM->tile3DDMApendingCount[dmaChannel] - doneCount;
    return(pTile3D->status == 0);
}

/**********************************************************************************
 * FUNCTION: xvCreateFrame3D()
 *
 * DESCRIPTION:
 *     Allocates single 3D frame. It does not allocate buffer required for frame data.
 *     Initializes the frame elements
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     void          *imgBuff        Pointer to image buffer
 *     uint32_t       frameBuffSize   Size of allocated image buffer
 *     int32_t       width           Width of image
 *     int32_t       height          Height of image
 *     int32_t       pitch           Pitch of image
 *     int32_t       depth           depth of image
 *     int32_t       Frame2Dpitch    2DFrame pitch
 *     uint8_t       pixRes          Pixel resolution of image in bytes
 *     uint8_t       numChannels     Number of channels in the image
 *     uint8_t       paddingtype     Supported padding type
 *     uint32_t       paddingVal      Padding value if padding type is edge extension
 *
 * OUTPUTS:
 *     Returns the pointer to allocated 3D frame.
 *     Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate frame data buffer.
 *
 ********************************************************************************** */

xvFrame3D *xvCreateFrame3D(xvTileManager *pxvTM, uint64_t imgBuff, uint32_t frameBuffSize, int32_t width, int32_t height, int32_t pitch, int32_t depth, int32_t Frame2Dpitch, uint8_t pixRes, uint8_t numChannels, uint8_t paddingType, uint32_t paddingVal)
{

    XV_CHECK_ERROR(((pxvTM == NULL) || (((void *) (uint32_t) imgBuff) == NULL)),   , ((xvFrame3D *) ((void *) XVTM_ERROR)), "NULL TM Pointer");
    XV_CHECK_ERROR(((width < 0) || (height < 0) || (pitch < 0) || (depth < 0) || (Frame2Dpitch < 0)),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR(((width * numChannels) > pitch),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((Frame2Dpitch < (pitch * height)),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((frameBuffSize < (Frame2Dpitch * depth * pixRes)),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR(((numChannels > MAX_NUM_CHANNEL) || (numChannels <= 0)),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((paddingType > FRAME_PADDING_MAX),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");

    xvFrame3D *pFrame3D = xvAllocateFrame3D(pxvTM);
    XV_CHECK_ERROR(((void *) pFrame3D == (void *) XVTM_ERROR),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvFrame3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");

    SETUP_FRAME_3D(pFrame3D, imgBuff, frameBuffSize, width, height, depth, pitch, Frame2Dpitch, 0, 0, 0, pixRes, numChannels, paddingType, paddingVal);
    return(pFrame3D);
}

/**********************************************************************************
 * FUNCTION: xvCreateTile3D()
 *
 * DESCRIPTION:
 *     Allocates single 3D tile and associated buffer data.
 *     Initializes the elements in tile
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     int32_t       tileBuffSize    Size of allocated tile buffer
 *     int32_t       width           Width of tile
 *     uint16_t      height          Height of tile
 *     uint16_t      depth           depth of tile
 *     int32_t       pitch           row pitch of tile
 *     int32_t       pitch2D         Pitch of 2D-tile
 *     uint16_t      edgeWidth       Edge width of tile
 *     uint16_t      edgeHeight      Edge height of tile
 *     uint16_t      edgeHDepth      Edge depth of tile
 *     int32_t       color           Memory pool from which the buffer should be allocated
 *     xvFrame3D     *pFrame3D       3D Frame associated with the 3D tile
 *     uint16_t       tileType        Type of tile
 *     int32_t       alignType       Alignment tpye of tile. could be edge aligned of data aligned
 *
 * OUTPUTS:
 *     Returns the pointer to allocated 3D tile.
 *     Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
 *
 ********************************************************************************** */

xvTile3D  *xvCreateTile3D(xvTileManager *pxvTM, int32_t tileBuffSize, int32_t width, uint16_t height, uint16_t depth, int32_t pitch, int32_t pitch2D, uint16_t edgeWidth, uint16_t edgeHeight,
                          uint16_t edgeDepth, int32_t color, xvFrame3D *pFrame3D, uint16_t xvTileType, int32_t alignType)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , ((xvTile3D *) ((void *) XVTM_ERROR)), "NULL TM Pointer");
    XV_CHECK_ERROR(((width < 0) || (pitch < 0)),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR(((alignType != EDGE_ALIGNED_32) && (alignType != DATA_ALIGNED_32) && (alignType != EDGE_ALIGNED_64) && (alignType != DATA_ALIGNED_64)),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR(((color < 0) || (color >= pxvTM->numMemBanks)) && (color != XV_MEM_BANK_COLOR_ANY),pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");

    int32_t channel = XV_TYPE_CHANNELS(xvTileType);
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(xvTileType);
    int32_t bytePerPel  = bytesPerPix / channel;
    XV_CHECK_ERROR((((width + (2 * edgeWidth)) * channel) > pitch),pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((((width + (2 * edgeWidth)) * (height + 2 * (edgeHeight))) > pitch2D),pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((tileBuffSize < (pitch2D * (depth + (2 * edgeDepth)) * bytePerPel)),pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");


    if (pFrame3D != NULL)
    {
    	XV_CHECK_ERROR((pFrame3D->pixelRes != bytePerPel),pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    	XV_CHECK_ERROR((pFrame3D->numChannels != channel),pxvTM->errFlag = XV_ERROR_BAD_ARG;  , ((xvTile3D *) ((void *) XVTM_ERROR)), "XV_ERROR_BAD_ARG");
    }
    void *tileBuff = NULL;
    tileBuff = xvAllocateBuffer(pxvTM, tileBuffSize, color, 64);

    if (tileBuff == (void *) XVTM_ERROR)
    {
        return((xvTile3D *) ((void *) XVTM_ERROR));
    }

    xvTile3D *pTile3D = xvAllocateTile3D(pxvTM);
    if ((void *) pTile3D == (void *) XVTM_ERROR)
    {
        //Type cast to void to avoid MISRA 17.7 violation
        (void) xvFreeBuffer(pxvTM, tileBuff);
        return((xvTile3D *) ((void *) XVTM_ERROR));
    }


    SETUP_TILE_3D(pTile3D, tileBuff, tileBuffSize, pFrame3D, width, height, depth, pitch, pitch2D, xvTileType, edgeWidth, edgeHeight, edgeDepth, 0, 0, 0, alignType);
    return(pTile3D);
}

/**********************************************************************************
 * FUNCTION: xvWaitForTileMultiChannel3D()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given 3D tile is completed.
 *
 * INPUTS:
 *     int32_t       ch                       iDMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile3D                 Input 3D tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForTileMultiChannel3D(int32_t ch, xvTileManager *pxvTM, xvTile3D const *pTile3D)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , (XVTM_ERROR), "NULL TM Pointer");

    XV_CHECK_TILE_3D(pTile3D, pxvTM);
    int32_t status = 0;
    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        status = xvCheckTileReadyMultiChannel3D(ch, pxvTM, pTile3D);
    }
    return(pxvTM->idmaErrorFlag[ch]);
}


/**********************************************************************************
 * FUNCTION: xvSleepForTileMultiChannel3D()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given 3D-tile is completed.
 *
 * INPUTS:
 *     int32_t       ch                       iDMA channel
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile3D        *pTile3D               3D-Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input 3D-tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForTileMultiChannel3D(int32_t ch, xvTileManager *pxvTM, xvTile3D const *pTile3D)
{
    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");

    XV_CHECK_TILE_3D(pTile3D, pxvTM);
#if defined (XTENSA_SWVERSION_RH_2018_6) && (XTENSA_SWVERSION > XTENSA_SWVERSION_RH_2018_6)
    DECLARE_PS();
#endif
    IDMA_DISABLE_INTS();
    int32_t status = xvCheckTileReadyMultiChannel3D(ch, (pxvTM), (pTile3D));
    while ((status == 0) && (pxvTM->idmaErrorFlag[ch] == XV_ERROR_SUCCESS))
    {
        idma_sleep(ch);
        status = xvCheckTileReadyMultiChannel3D(ch,(pxvTM), (pTile3D));
    }
    IDMA_ENABLE_INTS();
    return(pxvTM->idmaErrorFlag[ch]);
}


/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutMultiChannel3D()
 *
 * DESCRIPTION:
 *     Requests data transfer from 3D tile present in local memory to 3D frame in system memory.
 *
 * INPUTS:
 *     int32_t        dmaChannel              DMA channel number
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile3D        *pTile3D               Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */
int32_t xvReqTileTransferOutMultiChannel3D(int32_t dmaChannel, xvTileManager *pxvTM, xvTile3D *pTile3D, int32_t interruptOnCompletion)
{
    xvFrame3D *pFrame3D;
    uint64_t srcPtr64, dstPtr64;
    int32_t pixWidth, tileIndex, dmaIndex;
    int32_t srcHeight, srcWidth, srcDepth, srcPitchBytes, srcTilePitchBytes;
    int32_t dstPitchBytes, dstTilePitchBytes, numRows, rowSize, numTiles;

    XV_CHECK_ERROR(pxvTM == NULL,   , XVTM_ERROR, "NULL TM Pointer");
    pxvTM->errFlag = XV_ERROR_SUCCESS;
    XV_CHECK_ERROR((interruptOnCompletion < 0),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , XVTM_ERROR, "XV_ERROR_BAD_ARG");

    XV_CHECK_TILE_3D(pTile3D, pxvTM);
    pFrame3D = pTile3D->pFrame;
    XV_CHECK_ERROR(pFrame3D == NULL,  pxvTM->errFlag = XV_ERROR_FRAME_NULL;  , XVTM_ERROR, "XV_ERROR_FRAME_NULL");
    XV_CHECK_ERROR(((pFrame3D->pFrameBuff == NULL) || (pFrame3D->pFrameData == NULL)),  pxvTM->errFlag = XV_ERROR_FRAME_NULL;  , XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile3D));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile3D));
    int32_t bytePerPel  = bytesPerPix / channel;
    XV_CHECK_ERROR((pFrame3D->pixelRes != bytePerPel),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((pFrame3D->numChannels != channel),  pxvTM->errFlag = XV_ERROR_BAD_ARG;  , XVTM_ERROR, "XV_ERROR_BAD_ARG");

    pixWidth      = pFrame3D->pixelRes * pFrame3D->numChannels;
    srcHeight     = pTile3D->height;
    srcWidth      = pTile3D->width;
    srcPitchBytes = pTile3D->pitch * pFrame3D->pixelRes;
    srcTilePitchBytes = pTile3D->Tile2Dpitch * pFrame3D->pixelRes;
    srcDepth      = pTile3D->depth;
    dstPitchBytes = pFrame3D->framePitch * pFrame3D->pixelRes;
    dstTilePitchBytes = pFrame3D->frame2DFramePitch * pFrame3D->pixelRes;

    numRows       = XVTM_MIN(srcHeight, pFrame3D->frameHeight - pTile3D->y);
    rowSize       = XVTM_MIN(srcWidth, (pFrame3D->frameWidth - pTile3D->x));
    numTiles       = XVTM_MIN(srcDepth, pFrame3D->frameDepth - pTile3D->z);

    srcPtr64 =(uint64_t) (uint32_t) pTile3D->pData;
    dstPtr64 = pFrame3D->pFrameData + (uint64_t)((pTile3D->y * dstPitchBytes) + (pTile3D->x * pixWidth)  + (pTile3D->z*dstTilePitchBytes));

    if (pTile3D->x < 0)
    {
        rowSize += pTile3D->x; // x is negative;
        srcPtr64  += (-pTile3D->x * pixWidth);
        dstPtr64  += (uint64_t)(-pTile3D->x * pixWidth);
    }

    if (pTile3D->y < 0)
    {
        numRows += pTile3D->y; // y is negative;
        srcPtr64  += (-pTile3D->y * srcPitchBytes);
        dstPtr64  += (uint64_t)(-pTile3D->y * dstPitchBytes);
    }

    if (pTile3D->z < 0)
    {
        numTiles += pTile3D->z; // z is negative;
        srcPtr64  += (-pTile3D->z * srcTilePitchBytes);
        dstPtr64  += (uint64_t)(-pTile3D->z * dstTilePitchBytes);
    }


    rowSize *= pixWidth;

    if ((rowSize > 0) && (numRows > 0) && (numTiles > 0))
    {
        pTile3D->status = pTile3D->status | XV_TILE_STATUS_DMA_ONGOING;
        tileIndex     = (pxvTM->tile3DDMAstartIndex[dmaChannel] + pxvTM->tile3DDMApendingCount[dmaChannel]) % MAX_NUM_DMA_QUEUE_LENGTH;
        pxvTM->tile3DDMApendingCount[dmaChannel]++;
        pxvTM->tile3DProcQueue[dmaChannel][tileIndex] = pTile3D;

        dmaIndex = addIdmaRequestMultiChannel_wide3D(dmaChannel, pxvTM, dstPtr64, srcPtr64, rowSize,
                                                           numRows, srcPitchBytes, dstPitchBytes, interruptOnCompletion, srcTilePitchBytes,
                                                           dstTilePitchBytes, numTiles);

        pTile3D->dmaIndex                 = dmaIndex;
    }
    return(XVTM_SUCCESS);
}

#endif
