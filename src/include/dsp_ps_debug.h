
#ifndef DSP_PS_DEBUG_H
#define DSP_PS_DEBUG_H
#include "xrp_types.h"
#include "xrp_kernel_dsp_interface.h"

#ifdef DEBUG
#define FILENAME(x) strrchr(x,'/')?strrchr(x,'/')+1:x
#include <stdio.h>
//#define ps_log(fmt, ...)   printf("%s[%d]:"fmt,FILENAME(__FILE__),__LINE__,##__VA_ARGS__)
#define ps_log(level,fmt, ...)  printf("CSI_DSP_FW %s,(%s,%d):"fmt,level,__FUNCTION__,__LINE__,##__VA_ARGS__)

#else
#define ps_log
#endif

#define  DSP_PS_ERR   "ERROR"
#define  DSP_PS_WRN   "WARNING"
#define  DSP_PS_INFO  "INFO"
#define  DSP_PS_DEBUG  "DEBUG"

extern int dsp_log_level;

#define ps_err(fmt, ...) \
	{\
		if(dsp_log_level>=FW_DEBUG_LOG_MODE_ERR) \
	    	ps_log(DSP_PS_ERR,fmt, ##__VA_ARGS__);\
	}

#define ps_wrn(fmt, ...) \
		{\
			if(dsp_log_level>=FW_DEBUG_LOG_MODE_WRN) \
				ps_log(DSP_PS_WRN,fmt,##__VA_ARGS__);\
		}

#define ps_info(fmt, ...) \
		{\
			if(dsp_log_level>=FW_DEBUG_LOG_MODE_INF) \
				ps_log(DSP_PS_INFO,fmt,##__VA_ARGS__);\
		}

#define ps_debug(fmt, ...) \
		{\
			if(dsp_log_level>=FW_DEBUG_LOG_MODE_DBG) \
				ps_log(DSP_PS_DEBUG,fmt,##__VA_ARGS__);\
		}


/*****************************************************************************************
                        Other Definition.
*****************************************************************************************/
#define _FUNC_ENTRY_    ps_log(":E")
#define _FUNC_EXIT_     ps_log(":X")
#endif
