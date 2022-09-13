/********************************************************************************************

 ************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __XTENSA__
#include <conio.h>
#endif

#include <csi_dsp_types.h>
//#include "commondef.h"
//#include "dma.h"
#include "dmaif.h"
//#include "dl_hw_cfg.h"

#include <xtensa\config\core-isa.h>

#define DMA_DESCR_CNT            8 // number of DMA descriptors
#define MAX_PIF                  XCHAL_IDMA_MAX_OUTSTANDING_REQ

#define _USE_IDMA_
#define _USE_MULTI_IDMA
//#define _USE_TCM_ALLOCATE
//#if XCHAL_HAVE_NX
IDMA_BUFFER_DEFINE(idmaObjBuff, DMA_DESCR_CNT, IDMA_64B_DESC);
//#else
//IDMA_BUFFER_DEFINE(idmaObjBuff, DMA_DESCR_CNT, IDMA_2D_DESC);
//#endif
//#if XCHAL_HAVE_NX
#ifdef _USE_MULTI_IDMA
IDMA_BUFFER_DEFINE(idmaObjBuff1, DMA_DESCR_CNT, IDMA_64B_DESC);
#else
static idma_buffer_t *idmaObjBuff1=NULL;
#endif
//#endif

#define _LOCAL_RAM0_  __attribute__((section(".dram0.data")))
#define _LOCAL_RAM1_  __attribute__((section(".dram1.data")))
#define ALIGN64  __attribute__((aligned(64)))

#ifdef _USE_TCM_ALLOCATE
#define NON_POOL_MEM 			(20*1024)
#define POOL_SIZE1                ((XCHAL_DATARAM1_SIZE) - NON_POOL_MEM)
#define POOL_SIZE0                ((XCHAL_DATARAM0_SIZE) - NON_POOL_MEM)
uint8_t ALIGN64 pBankBuffPool0[POOL_SIZE0]  _LOCAL_RAM0_;
uint8_t ALIGN64 pBankBuffPool1[POOL_SIZE1] 	_LOCAL_RAM1_;
#endif
/****************************************************************************************
log
****************************************************************************************/
#include "dsp_ps_debug.h"
#define DL_VRB(fmt, ...)    //ps_debug(fmt,##__VA_ARGS__)
#define DL_DBG(fmt, ...)	ps_debug(fmt,##__VA_ARGS__)
#define DL_INF(fmt, ...)	ps_info(fmt,##__VA_ARGS__)
#define DL_WRN(fmt, ...)	ps_wrn(fmt,##__VA_ARGS__)
#define DL_ERR(fmt, ...)	ps_err(fmt,##__VA_ARGS__)
//#include <vpu_log.h>
/****************************************************************************************
    IVPEP Highlevel spec.

    .One and only one of the source or destination must be local data memory (DRam) and remain so for the duration of the transfer
    .One and only one of the source or destination must be external memory (AXI) and remain so for the duration of the transfer
    .DMA of external memory to external memory is not supported
    .DMA of local memory to local memory is not supported
    .DMA that switches direction during the transfer is not supported
    .moves two-dimensional (2-D) data tiles between the IVP-EP core¡¦s local data RAMs (DRam0 and DRam1) and memories external to the core accessed via AXI

    local ram
       /\
       |
      512b
       |
    micro DMA controller
       |
      128b
       |
       \/
    external ram

    .register
        IVP_RER
        IVP_WER
            ƒÞ Enabling DMA
            ƒÞ Enabling NMI interrupt for error (descriptor and bus errors) reporting
            ƒÞ Pointers to a circular buffer of DMA descriptors
            ƒÞ Number of DMA descriptors to process
            ƒÞ Status registers
    .DMA completion may be signaled in the following ways:
        ƒÞ Reading a status or progress register that can be polled with an IVP_RER operation.
        ƒÞ Setting up a short DMA transaction to write a memory location with a completion flag.
        ƒÞ Generating an edge-triggered interrupt when enabled by a descriptor field.
    .DMA can only access the IVP-EP core¡¦s DRams, not its IRam.
    .A DMA transaction is generally defined by the following parameters:
        ƒÞ Source and destination addresses
        ƒÞ Source and destination pitch (distance from the start of one row to the start of the next row in a 2-D transfer)
        ƒÞ Number of rows to transfer and number of bytes to transfer per row
        ƒÞ Block size limit for PIF requests
        ƒÞ Number of outstanding requests limit
        ƒÞ Control for completion interrupt notification
    .Descriptors have the following properties:
        ƒÞ Size 32B
        ƒÞ Aligned to 32B
    .mem footpring
        0x101C Reserved
        0x1018 Reserved
        0x1014 Destination Pitch
        0x1010 Source Pitch
        0x100C Num of Rows
        0x1008 Control
        0x1004 Destination Address
        0x1000 Source Address
    .Block size limit (i.e. maximum size) for DMA
        0: Block 2 (32 bytes)
        1: Block 4 (64 bytes)
        2: Block 8 (128 bytes)
        3: Block 16 (256 bytes)
    .Transfers with row lengths of 16 B or less have the following additional requirements:
        ƒÞ The number of bytes per row must be 1, 2, 4, 8, or 16
        ƒÞ Both the source and destination addresses must have the same 16B alignment
        ƒÞ The source and destination addresses must be aligned to the number of bytes per row
    .Transfers with row lengths greater than 16B transfer each row using one or more block (or burst) transactions. Block transaction sizes are 2, 4, 8, or 16 transfers, corresponding to 32B, 64B, 128B, or 256B respectively. The same block transaction size is used for the entire transfer ¡V i.e., for all rows and all block transactions within each row. The following additional requirements apply:
        ƒÞ There must be a block transaction size that meets the following requirements ¡V each row transfer will use the largest block size meeting these requirements that is no larger than the maximum block size specified in the descriptor:
            ƒÞ The number of bytes per row is an integer multiple of the block size
            ƒÞ The external memory address (source or destination) is aligned to the size of the block transaction
            ƒÞ The local memory address (source or destination) is aligned to the smaller of the local memory width (64 B) or the block transaction length
        ƒÞ The row transfer must not cross a local memory DRam boundary
****************************************************************************************/
/****************************************************************************************
    declaration
****************************************************************************************/
#define _ALIGN_(a,b) ((((TUINT32)a+(b-1))/b)*b)
/****************************************************************************************
    declaration
****************************************************************************************/
TBOOL init();
TBOOL uninit();
TBOOL config(xvTile *tile_obj, uint32_t tile_x,uint32_t tile_y );
xvTile *setup(uint64_t imgBuff,uint32_t frame_BuffSize,int32_t frame_width, int32_t frame_height, int32_t frame_pitch, uint8_t frame_pixRes, uint8_t frame_numChannels, uint8_t frame_paddingtype, uint32_t frame_paddingVal,
			  int32_t tile_BuffSize, int32_t tile_width, uint16_t tile_height, int32_t tile_pitch, uint16_t tile_edgeWidth, uint16_t tile_edgeHeight, int32_t tile_color, uint16_t tile_type, int32_t alignType);
TBOOL release(xvTile *tile_obj);
TUINT32 store(xvTile *tile_obj, uint64_t pdst64, uint64_t psrc64, uint32_t srcPitchBytes, uint32_t dstPitchBytes, uint32_t numRows, uint32_t numBytesPerRow, uint32_t interruptOnCompletion);
TUINT32 copy(xvTile *tile_obj,uint64_t pdst64, uint64_t psrc64,  uint32_t srcPitchBytes, uint32_t dstPitchBytes, uint32_t numRows, uint32_t numBytesPerRow, uint32_t interruptOnCompletion);
TBOOL stop();
TBOOL stall();
TBOOL isDone();
TBOOL align_check(xvTile *tile_obj,uint64_t pdst64,uint64_t psrc64,TUINT32 srcPitchBytes,TUINT32 dstPitchBytes,TUINT32 numRows,TUINT32 numBytesPerRow,tm_dma_direction direction);
TBOOL get_alignment(_IN_ xvTile *tile_obj,_IN_ TUINT32 numBytesPerRow,_OUT_ TUINT32 *rowByteAlign,_OUT_ TUINT32 *sysMemAlign,_OUT_ TUINT32 *SMemAlign);

TUINT32 load(xvTile * tile_obj, uint64_t pdst64, uint64_t psrc64,  uint32_t srcPitchBytes, uint32_t dstPitchBytes, uint32_t numRows, uint32_t numBytesPerRow, uint32_t interruptOnCompletion);
TBOOL start(TUINT32, TUINT32);
TBOOL waitDone(xvTile * tileIn_obj,xvTile * tileOut_obj);
TBOOL waitLoadDone(xvTile * tile_obj);
TBOOL waitStoreDone(xvTile * tile_obj);
T_PTR allocateTcm(_IN_ TUINT32 num_bytes,_IN_ TUINT32 buf_id,_IN_ TUINT32 align);
TBOOL freeTcm(void* ptr);
/****************************************************************************************

****************************************************************************************/
static volatile uint32_t idxDmaCnt[DMA_DIR_MAX];
static int32_t  idma_requests[DMA_DIR_MAX][DMA_DESCR_CNT];
static uint32_t dmaAlignment = 0;

//static xv_dmaObject ALIGN64 dmaObj _LOCAL_DRAM0_;
static xvTileManager ALIGN64 g_TMObj _LOCAL_RAM0_;


static idma_max_block_t idma_block_index;
//static xvFrame* frame;
//static xvTile * tile;

typedef struct intrCallbackDataStruct
{
	int32_t intrCount;
} intrCbDataStruct;
intrCbDataStruct cbData0 _LOCAL_RAM0_;
intrCbDataStruct cbData1 _LOCAL_RAM0_;
/****************************************************************************
 * 					internal func
 * ****************************************************************/
// IDMA error callback function
static void errCallbackFunc0(idma_error_details_t* data)
{
	DL_DBG("ERROR CALLBACK: iDMA in Error\n");
	g_TMObj.errFlag = (xvError_t)XVTM_ERROR;
	g_TMObj.idmaErrorFlag[0] = (xvError_t)XVTM_ERROR;
	printf("COPY FAILED, Error %d at desc:%p, PIF src/dst=%x/%x\n", data->err_type, (void *) data->currDesc, data->srcAddr, data->dstAddr);
	return;
}

// IDMA error callback function
static void errCallbackFunc1(idma_error_details_t* data)
{
	DL_DBG("ERROR CALLBACK: iDMA in Error\n");
	g_TMObj.errFlag = (xvError_t)XVTM_ERROR;
	g_TMObj.idmaErrorFlag[1] = (xvError_t)XVTM_ERROR;
	//printf("COPY FAILED, Error %d at desc:%p, PIF src/dst=%x/%x\n", data->err_type, (void *) data->currDesc, data->srcAddr, data->dstAddr);
	return;
}

// IDMA Interrupt callback function
static void intrCallbackFunc0(void *pCallBackStr)
{
//	DL_DBG("INTERRUPT CALLBACK : processing iDMA interrupt\n");
	((intrCbDataStruct *) pCallBackStr)->intrCount++;

	return;
}

static void intrCallbackFunc1(void *pCallBackStr)
{
//	DL_DBG("INTERRUPT CALLBACK : processing iDMA interrupt\n");
	((intrCbDataStruct *) pCallBackStr)->intrCount++;

	return;
}

static TMDMA g_TMDMA = {
			.init = init,
            .uninit = uninit,
            .setup = setup,
            .release = release,
            .config = config,
            .load = load,
            .store = store,
            .copy = copy,
            .start = start,
            .stop = stop,
            .stall = stall,
            .waitDone = waitDone,
            .waitLoadDone = waitLoadDone,
            .waitStoreDone = waitStoreDone,
            .isDone = isDone,
            .align_check = align_check,
            .get_alignment = get_alignment,
            .allocateTcm =allocateTcm,
			.freeTcm = freeTcm};
/****************************************************************************************

****************************************************************************************/
TMDMA*
dmaif_getDMACtrl(void)
{
    return &g_TMDMA;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
init()
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

    DL_DBG("%s\n",__func__);
#if defined  (_USE_IDMA_)
	idma_block_index= MAX_BLOCK_16;
	dmaAlignment = 32;
	#if 1
		void *buffPool[2]={NULL};
		int32_t buffSize[2]={0};
		uint32_t pool_num=0;

		#ifdef _USE_TCM_ALLOCATE
		pool_num=2;
		buffPool[0] = pBankBuffPool0;
		buffPool[1] = pBankBuffPool1;
		buffSize[0] = POOL_SIZE0;
		buffSize[1] = POOL_SIZE1;
		xvCreateTileManagerMultiChannel(&g_TMObj,idmaObjBuff,idmaObjBuff1,pool_num,buffPool,buffSize,
							(idma_err_callback_fn)errCallbackFunc0, (idma_err_callback_fn)errCallbackFunc1,
									intrCallbackFunc0, (void *) &cbData0,
									intrCallbackFunc1, (void *) &cbData1,
									DMA_DESCR_CNT, idma_block_index, MAX_PIF);
		#else
		xvInitIdmaMultiChannel(&g_TMObj,idmaObjBuff,idmaObjBuff1,DMA_DESCR_CNT, idma_block_index, MAX_PIF,
							   (idma_err_callback_fn)errCallbackFunc0, (idma_err_callback_fn)errCallbackFunc1,
													intrCallbackFunc0, (void *) &cbData0,
													intrCallbackFunc1, (void *) &cbData1);
		#endif
	#else
//		int32_t timeoutTicks         = 0;
//		int32_t initFlags            = 0;
//		int32_t retVal;
//		retVal = idma_init(TM_IDMA_CH0, initFlags, idma_block_index, MAX_PIF, TICK_CYCLES_2, timeoutTicks, errCallbackFunc0);
//		retVal |= idma_init_loop(TM_IDMA_CH0, idmaObjBuff, IDMA_64B_DESC, DMA_DESCR_CNT, cbData0, intrCallbackFunc0);
//
//		#ifdef _USE_MULTI_IDMA
//		retVal = idma_init(TM_IDMA_CH1, initFlags, idma_block_index, MAX_PIF, TICK_CYCLES_2, timeoutTicks, errCallbackFunc1);
//		retVal |= idma_init_loop(TM_IDMA_CH1, idmaObjBuff1, IDMA_64B_DESC, DMA_DESCR_CNT, cbData1, intrCallbackFunc1);
//
//		#endif
//		idxDmaCnt[DMA_DIR_IN] =0;
//		idxDmaCnt[DMA_DIR_OUT] =0;
	#endif
#endif

    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
uninit()
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

    xvResetTileManager(&g_TMObj);

    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
xvTile *
setup(uint64_t imgBuff,uint32_t frame_BuffSize,int32_t frame_width, int32_t frame_height, int32_t frame_pitch, uint8_t frame_pixRes, uint8_t frame_numChannels, uint8_t frame_paddingtype, uint32_t frame_paddingVal,
	  int32_t tile_BuffSize, int32_t tile_width, uint16_t tile_height, int32_t tile_pitch, uint16_t tile_edgeWidth, uint16_t tile_edgeHeight, int32_t tile_color, uint16_t tile_type, int32_t alignType)
{
	xvTile * tile;
	xvFrame * frame = xvCreateFrame(&g_TMObj,imgBuff, frame_BuffSize, frame_width, frame_height, frame_pitch, frame_pixRes, frame_numChannels, frame_paddingtype, frame_paddingVal);
	tile = xvCreateTile(&g_TMObj, tile_BuffSize, tile_width, tile_height,  tile_pitch, tile_edgeWidth, tile_edgeHeight,tile_color, frame, tile_type, alignType);

	if((void *)XVTM_ERROR == (void *)tile)
		tile =NULL;

	return tile;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
release(xvTile *tile_obj)
{
	if(tile_obj==NULL)
	{
		return TFALSE;
	}
	xvFrame * frame = tile_obj->pFrame;
	if(XVTM_ERROR ==xvFreeBuffer(&g_TMObj,XV_TILE_GET_BUFF_PTR(tile_obj)))
	{
		return TFALSE;
	}
	if(XVTM_ERROR==xvFreeTile(&g_TMObj,tile_obj))
	{
		return TFALSE;
	}
	if(XVTM_ERROR==xvFreeFrame(&g_TMObj,frame))
	{
		return TFALSE;
	}
	return TTRUE;
}

/****************************************************************************************

****************************************************************************************/
TBOOL
config(xvTile * tile_obj,
    uint32_t tile_x,
    uint32_t tile_y)
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");
    if(tile_obj == NULL)
    {
    	return TFALSE;
    }
    XV_TILE_SET_X_COORD(tile_obj, tile_x);
    XV_TILE_SET_Y_COORD(tile_obj, tile_y);
    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TUINT32
load(xvTile * tile_obj, uint64_t pdst64, uint64_t psrc64, uint32_t srcPitchBytes, uint32_t dstPitchBytes, uint32_t numRows, uint32_t numBytesPerRow, uint32_t interruptOnCompletion)
{
    //
    numBytesPerRow = (_ALIGN_(numBytesPerRow,dmaAlignment) > dstPitchBytes) ? dstPitchBytes : _ALIGN_(numBytesPerRow,dmaAlignment);

#ifndef _USE_IDMA_

    copy(g_TMObj, psrc, pdst, srcPitchBytes, dstPitchBytes, numRows, numBytesPerRow, interruptOnCompletion);

#else
	if(tile_obj)
	{
		 int32_t ret = xvReqTileTransferInFastMultiChannel(TM_IDMA_CH0,&g_TMObj,tile_obj,interruptOnCompletion);
		 return ret==XVTM_SUCCESS?TTRUE:TFALSE;
	}
	else
	{

	    if (DMA_DESCR_CNT <= idxDmaCnt[DMA_DIR_IN]) {
	        DL_ERR("desc full(%d)",idxDmaCnt[DMA_DIR_IN]);
	        return 0;
	    }
	    //from sysram to local
	    int32_t id = xvAddIdmaRequestMultiChannel_wide(TM_IDMA_CH0,&g_TMObj,pdst64,psrc64,numBytesPerRow,numRows,srcPitchBytes,dstPitchBytes,interruptOnCompletion);

	    if ( 0 != numBytesPerRow && 0 != numRows ) {
	    	idma_requests[DMA_DIR_IN][idxDmaCnt[DMA_DIR_IN]]=id;
	    	idxDmaCnt[DMA_DIR_IN]++;
	    }
	}

#endif

    return (numBytesPerRow*numRows);
}
/****************************************************************************************

****************************************************************************************/
TUINT32
store(xvTile * tile_obj, uint64_t pdst64, uint64_t psrc64, uint32_t srcPitchBytes, uint32_t dstPitchBytes, uint32_t numRows, uint32_t numBytesPerRow, uint32_t interruptOnCompletion)
{
    //
    numBytesPerRow = (_ALIGN_(numBytesPerRow,dmaAlignment) > dstPitchBytes) ? dstPitchBytes : _ALIGN_(numBytesPerRow,dmaAlignment);

#ifndef _USE_IDMA_
    
    copy(g_TMObj, pdst64, psrc64, srcPitchBytes, dstPitchBytes, numRows, numBytesPerRow, interruptOnCompletion);

#else

	if(tile_obj)
	{
		int32_t ret;
		#ifdef _USE_MULTI_IDMA
		ret = xvReqTileTransferOutFastMultiChannel(TM_IDMA_CH1,&g_TMObj,tile_obj,interruptOnCompletion);
		#else
		ret = xvReqTileTransferOutFastMultiChannel(TM_IDMA_CH0,&g_TMObj,tile_obj,interruptOnCompletion);
		#endif
		 return ret==XVTM_SUCCESS?TTRUE:TFALSE;
	}
	else
	{
		if ( DMA_DESCR_CNT <= idxDmaCnt[DMA_DIR_OUT] ) {
			DL_ERR("desc full(%d)",idxDmaCnt[DMA_DIR_OUT]);
			return 0;
		}
		//from sysram to local
		#ifdef _USE_MULTI_IDMA
		int32_t id = xvAddIdmaRequestMultiChannel_wide(TM_IDMA_CH1,&g_TMObj,pdst64,psrc64,numBytesPerRow,numRows,srcPitchBytes,dstPitchBytes,interruptOnCompletion);
		#else
		int32_t id = xvAddIdmaRequestMultiChannel_wide(TM_IDMA_CH0,&g_TMObj,pdst64,psrc64,numBytesPerRow,numRows,srcPitchBytes,dstPitchBytes,interruptOnCompletion);

		#endif
		if ( 0 != numBytesPerRow && 0 != numRows ) {
			idma_requests[DMA_DIR_OUT][idxDmaCnt[DMA_DIR_OUT]]=id;
			idxDmaCnt[DMA_DIR_OUT]++;
		}
	}
#endif
   return (numBytesPerRow*numRows);
}
/****************************************************************************************

****************************************************************************************/
TUINT32
copy(xvTile * tile_obj, uint64_t pdst64, uint64_t psrc64, uint32_t srcPitchBytes, uint32_t dstPitchBytes, uint32_t numRows, uint32_t numBytesPerRow, uint32_t interruptOnCompletion)
{

    uint32_t i;
    uint8_t *pdst = (uint8_t *)(uint32_t)pdst64;
    uint8_t *psrc = (uint8_t *)(uint32_t)psrc64;

    if((pdst64&0xffffffff00000000) | (psrc64&0xffffffff00000000))
    {
    	return 0;
    }
    for ( i=0; i<numRows;i++) {
        memcpy(pdst+dstPitchBytes*i, psrc+srcPitchBytes*i, numBytesPerRow);
    }

    return (numBytesPerRow*numRows);
}
/****************************************************************************************

****************************************************************************************/
TBOOL
start(TUINT32 sDesc, TUINT32 eDesc)
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
stop()
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
stall()
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");


    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
waitDone(xvTile * tileIn_obj,xvTile * tileOut_obj)
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");


    ret = waitLoadDone(tileIn_obj);

    ret &= waitStoreDone(tileOut_obj);

    DL_VRB(":X");
    return ret;
}

TBOOL
waitLoadDone(xvTile * tile_obj)
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

    if(tile_obj)
    {
    	xvWaitForTileFastMultiChannel(TM_IDMA_CH0,&g_TMObj,tile_obj);
    }
    else
    {
    	int32_t loop;
    	int32_t done_flag=0;
    	for(loop= 0; loop < idxDmaCnt[DMA_DIR_IN];loop++)
    	{
    		do{
    			done_flag = xvCheckForIdmaIndexMultiChannel(TM_IDMA_CH0,&g_TMObj,idma_requests[DMA_DIR_IN][loop]);
    		}while(!done_flag && g_TMObj.idmaErrorFlag[TM_IDMA_CH0]==XV_ERROR_SUCCESS);

    	}
    }
    idxDmaCnt[DMA_DIR_IN] = 0;
    if( g_TMObj.idmaErrorFlag[TM_IDMA_CH0]!=XV_ERROR_SUCCESS)
    {
    	DL_DBG("Fail\n");
    	ret =TFALSE;
    }
    DL_VRB(":X");
    return ret;
}

TBOOL
waitStoreDone(xvTile * tile_obj)
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

	#ifdef _USE_MULTI_IDMA
	int32_t channle = TM_IDMA_CH1;
	#else
	int32_t channle = TM_IDMA_CH0;
	#endif
    if(tile_obj)
    {
    	xvWaitForTileFastMultiChannel(channle,&g_TMObj,tile_obj);
    }
    else
    {
    	int32_t loop;
    	int32_t done_flag=0;

    	for(loop= 0; loop < idxDmaCnt[DMA_DIR_OUT];loop++)
    	{
    		do{

    			done_flag =xvCheckForIdmaIndexMultiChannel(channle,&g_TMObj,idma_requests[DMA_DIR_OUT][loop]);

    		}while(!done_flag&&g_TMObj.idmaErrorFlag[channle]==XV_ERROR_SUCCESS);
    	}
    }
    idxDmaCnt[DMA_DIR_OUT] = 0;
    if(g_TMObj.idmaErrorFlag[channle]!=XV_ERROR_SUCCESS)
    {
    	DL_DBG("Fail");
    	ret =TFALSE;
    }
    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
isDone()
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");


    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
align_check(xvTile * tile_obj,
			uint64_t pdst64,
			uint64_t psrc64,
            TUINT32 srcPitchBytes,
            TUINT32 dstPitchBytes,
            TUINT32 numRows,
            TUINT32 numBytesPerRow,
            tm_dma_direction direction)
{
    TBOOL ret = TTRUE;
    int i = 0;

    DL_VRB(":E");


/*
    .Transfers with row lengths of 16 B or less have the following additional requirements:
        ƒÞ The number of bytes per row must be 1, 2, 4, 8, or 16
        ƒÞ Both the source and destination addresses must have the same 16B alignment
        ƒÞ The source and destination addresses must be aligned to the number of bytes per row
    .Transfers with row lengths greater than 16B transfer each row using one or more block (or burst) transactions. 
     Block transaction sizes are 2, 4, 8, or 16 transfers, corresponding to 32B, 64B, 128B, or 256B respectively. 
     The same block transaction size is used for the entire transfer ¡V i.e., for all rows and all block transactions 
     within each row. The following additional requirements apply:
        ƒÞ There must be a block transaction size that meets the following requirements ¡V each row transfer will use 
           the largest block size meeting these requirements that is no larger than the maximum block size specified
           in the descriptor:
            ƒÞ The number of bytes per row is an integer multiple of the block size
            ƒÞ The external memory address (source or destination) is aligned to the size of the block transaction
            ƒÞ The local memory address (source or destination) is aligned to the smaller of the local memory width (64 B) 
               or the block transaction length
        ƒÞ The row transfer must not cross a local memory DRam boundary

*/
    if ( 16 >= numBytesPerRow ) {
        for ( i=1; i<=5; i++ ) {
            if ( 0 == (numBytesPerRow >> i) ) {
                break;
            }
        }
        if ( 1<<i != numBytesPerRow ) {
            DL_ERR("invalid numBytesPerRow(%d)",numBytesPerRow);
            ret = TFALSE;
        }
        if ( 0 != ((TUINT32)psrc64&0x0F) || 0 != ((TUINT32)pdst64&0x0F) ) {
            DL_ERR("invalid psrc64/pdst alignment(0x%x/0x%x)",(TUINT32)psrc64,(TUINT32)pdst64);
            ret = TFALSE;
        }
    }else {
        int ialign = 1 << (idma_block_index + 5);
        if ( 0 != (numBytesPerRow&(~ialign)) ) {
            DL_ERR("invalid numBytesPerRow alignment(0x%x/0x%x)",numBytesPerRow,ialign);
            ret = TFALSE;
        }
        if ( DMA_DIR_IN == direction ) {
            /* external mem*/
            if ( 0 != ((TUINT32)psrc64&(~ialign)) ) {
                DL_ERR("invalid psrc64 alignment(0x%x/0x%x),dir(%d)",(TUINT32)psrc64,ialign,direction);
                ret = TFALSE;
            }
            /* internal mem.*/
            if ( 0 != ((TUINT32)psrc64&(~((64>ialign)?ialign:64))) ) {
                DL_ERR("invalid psrc64 alignment(0x%x/0x%x),dir(%d)",(TUINT32)psrc64,ialign,direction);
                ret = TFALSE;
            }
        }else {
            /* external mem*/
            if ( 0 != ((TUINT32)psrc64&(~ialign)) ) {
                DL_ERR("invalid psrc64 alignment(0x%x/0x%x),dir(%d)",(TUINT32)psrc64,ialign,direction);
                ret = TFALSE;
            }
            /* internal mem.*/
            if ( 0 != ((TUINT32)psrc64&(~((64>ialign)?ialign:64))) ) {
                DL_ERR("invalid psrc64 alignment(0x%x/0x%x),dir(%d)",(TUINT32)psrc64,ialign,direction);
                ret = TFALSE;
            }
        }
    }

    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
get_alignment(_IN_ xvTile * tile_obj,
              _IN_ TUINT32 numBytesPerRow,
              _OUT_ TUINT32 *rowByteAlign,
              _OUT_ TUINT32 *sysMemAlign,
              _OUT_ TUINT32 *SMemAlign)
{
    TBOOL ret = TTRUE;
    DL_VRB(":E");

    if ( 16 >= numBytesPerRow ) {
        *rowByteAlign = 1;
        *sysMemAlign = 0x10; /* 16B align*/
        *SMemAlign   = 0x10; /* 16B align*/
    }
    else {
        int ialign = 1 << (idma_block_index + 5);
        *rowByteAlign = ialign;
        *sysMemAlign  = ialign; /* 16B align*/
        *SMemAlign    = (64>ialign)?ialign:64;   /* 16B align*/
    }

    DL_VRB(":X");
    return ret;
}
/****************************************************************************************

****************************************************************************************/
T_PTR
allocateTcm(_IN_ TUINT32 num_bytes,
			_IN_ TUINT32 buf_id,
			_IN_ TUINT32 align)
{
	T_PTR ptr=xvAllocateBuffer(&g_TMObj,num_bytes,buf_id,align);
	if(ptr ==(void*)XVTM_ERROR)
	{
		return 0;
	}
	return ptr;
}
/****************************************************************************************

****************************************************************************************/
TBOOL
freeTcm(void* ptr)
{
	return xvFreeBuffer(&g_TMObj,ptr)==XVTM_ERROR?TFALSE:TTRUE;
}
