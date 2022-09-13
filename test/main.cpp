#include <stdio.h>
#include <stdint.h>

#include "CppUTest/CommandLineTestRunner.h"
#ifdef DSP_TEST
int main( int argc, char **argv )
{
	int32_t test;
	printf("Hello World\n");
	printf("Hello DSP\n");
	return RUN_ALL_TESTS(argc, argv);
	return 0;

}
#endif