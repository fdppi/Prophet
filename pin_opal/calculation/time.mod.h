#include <time.h>
#include <sys/time.h>

#define RUN_TIME(variable, ...) do{		\
	struct timeval start, end;			\
	gettimeofday(&start, NULL);			\
	__VA_ARGS__;						\
	gettimeofday(&end, NULL);			\
	variable += (double) (end.tv_usec - start.tv_usec) / 1000000 + 	\
				(double)(end.tv_sec-start.tv_sec);\
}while(0)
