#include <stdio.h>
#include <stdint.h>

#include "CppUTest/CommandLineTestRunner.h"

#ifdef __cplusplus
extern "C" {
#ifdef TEST_RESULT_TO_BUFFER
extern void set_stdout_buf(char* ptr,int size);
extern void restore_stdout();
#endif
}
#endif

extern "C" {
int run_case( char*ptr,int size,int argc, char **argv )
{
	int ret =0;
#ifdef TEST_RESULT_TO_BUFFER
	set_stdout_buf(ptr,size);
#endif
#ifdef DSP_IP_TEST
	ret= RUN_ALL_TESTS(argc, argv);
#endif
#ifdef TEST_RESULT_TO_BUFFER
	restore_stdout();
#endif
	return ret;

}
}
