#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#define ALIGN	4096
#define MAX_N	(10*1024*1024)		// bytes

#ifndef ITER
#define ITER  30
#endif

#define BKNI_MEMCPY 1


// Two arrays of MAX_N bytes size
// Defined as unsigned long to ensure alignment
static unsigned long static_a_[(MAX_N+ALIGN)/sizeof(unsigned long)];
static unsigned long static_b_[(MAX_N+ALIGN)/sizeof(unsigned long)];


int test_memcpy(unsigned long *a_, unsigned long *b_)
{
	struct timeval begin,end;
	register int i;
	double diff;
	unsigned long long bytes;
	char *c_ptr_from, *c_ptr_to;
	static unsigned long *a;
	static unsigned long *b;
	int iterations, copy_size;

	// a and b point into the arrays at word-aligned addresses
	a = (void *)((((unsigned long)(a_)+ALIGN-1)/ALIGN)*ALIGN);
	b = (void *)((((unsigned long)(b_)+ALIGN-1)/ALIGN)*ALIGN);

// 1Mbyte copies repeated
	iterations = ITER;
	copy_size = MAX_N;

	fflush(stdout);
	printf("Static Allocation:\na @ 0x%08lX, b @ 0x%08lX\n", (long)a, (long)b);
	printf("%u iterations, each iteration copies %u bytes\n\n", (unsigned)iterations, (unsigned)copy_size);

// word-aligned
	c_ptr_from = (char *)a;
	c_ptr_to   = (char *)b;
	c_ptr_from += 0;
	c_ptr_to += 0;

	gettimeofday(&begin, NULL);
	for (i = 0; i < iterations; i++)
	{
#if BKNI_MEMCPY
		BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size);
#else
		memcpy(c_ptr_to, c_ptr_from, copy_size);
#endif
	}

	gettimeofday(&end, NULL);
	bytes = (long long)iterations*(long long)copy_size;
	diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

	printf("Bandwidth (word aligned) = \t");
	printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

// word+1 aligned
	c_ptr_from = (char *)a;
	c_ptr_to   = (char *)b;
	c_ptr_from += 1;
	c_ptr_to += 1;

	gettimeofday(&begin, NULL);
	for (i = 0; i < iterations; i++)
	{
#if BKNI_MEMCPY
		BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size);
#else
		memcpy(c_ptr_to, c_ptr_from, copy_size);
#endif
	}

	gettimeofday(&end, NULL);
	bytes = (long long)iterations*(long long)copy_size;
	diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

	printf("Bandwidth (word+1 aligned) = \t");
	printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

// word+2 aligned
	c_ptr_from = (char *)a;
	c_ptr_to   = (char *)b;
	c_ptr_from += 2;
	c_ptr_to += 2;

	gettimeofday(&begin, NULL);
	for (i = 0; i < iterations; i++)
	{
#if BKNI_MEMCPY
		BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size);
#else
		memcpy(c_ptr_to, c_ptr_from, copy_size);
#endif
	}

	gettimeofday(&end, NULL);
	bytes = (long long)iterations*(long long)copy_size;
	diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

	printf("Bandwidth (word+2 aligned) = \t");
	printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

// word+3 aligned
	c_ptr_from = (char *)a;
	c_ptr_to   = (char *)b;
	c_ptr_from += 3;
	c_ptr_to += 3;

	gettimeofday(&begin, NULL);
	for (i = 0; i < iterations; i++)
	{
#if BKNI_MEMCPY
		BKNI_Memcpy(c_ptr_to, c_ptr_from, copy_size);
#else
		memcpy(c_ptr_to, c_ptr_from, copy_size);
#endif
	}

	gettimeofday(&end, NULL);
	bytes = (long long)iterations*(long long)copy_size;
	diff = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec)/1000000.0;

	printf("Bandwidth (word+3 aligned) = \t");
	printf("\t%.2f MBytes/sec\n", (bytes/diff)/((double)1024*1024.0));

//	printf("Each iteration takes %.3f msec\n", (diff/ITER)*1000);

	fflush(stdout);
	return 0;
}

int main(void)
{
    unsigned long *a_ = static_a_;
    unsigned long *b_ = static_b_;

	memset(a_, 0, sizeof(static_a_));
	memset(b_, 0, sizeof(static_b_));

	test_memcpy(a_, b_);

	test_memcpy(b_, a_);

	return 0;
}

