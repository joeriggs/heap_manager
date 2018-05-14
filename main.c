/*******************************************************************************
 * This test program is based very closely on the Heap-Manager project that is
 * located at https://github.com/sandmman/Heap-Manager .  It's a good test
 * program.  Thanks to the author for writing it and making it available.
 ******************************************************************************/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "shmHeap.h"

#define DEBUG printf

#define RAND() ((double)random()/RAND_MAX)
#define SEED(x) (srandom((x)))

#define BUFLEN (1000)

#define LOOPCNT (50000)

/* You can play around with these number to get different tests. */
#define MAX_HEAP_SIZE	(1024*1024*64)
#define MAX_ALLOC_SIZE (MAX_HEAP_SIZE/1000)

/* Set to 1 for non-deterministic seeding after each execution */
#define PSEUDO_RANDOM_SEED	1

#define ALLOC_CONST	0.5

/* Test program. */
int main(int argc, char **argv)
{
	printf("Heap manager\n");

	static unsigned char test6Heap[MAX_HEAP_SIZE];
	printf("%s(): test6Heap %p\n", __func__, test6Heap);
	shmHeapInit(test6Heap, sizeof(test6Heap));
	shmHeapDisp();

	int size;
	int itr;
	void *ptr[BUFLEN];
	int i,j;
	double randvar;
	int fail = 0;
        long int global[LOOPCNT][2];

	clock_t begin, end;
	double time_spent;

	for(j = 0; j < LOOPCNT; j++) {
		global[j][0] = -1;
		global[j][1] = -1;
	}

	/* Set the PSEUDO_RANDOM_SEED for pseduo random seed initialization based on time, i.e.,
 	 * the random values changes after each execution
 	 */
	if(PSEUDO_RANDOM_SEED)
		SEED(time(NULL));

	assert(MAX_HEAP_SIZE >= 1024*1024 && "MAX_HEAP_SIZE is too low; Recommended setting is at least 1MB for test_stress2");

	for(i=0; i < BUFLEN; i++) {
		ptr[i] = NULL;
	}

	begin = clock();

	i = 0;
	while(i < LOOPCNT) {
		DEBUG("\nCNT %li\n",i);
		itr = (int)(RAND() * BUFLEN);

		randvar = RAND();

		if(randvar < ALLOC_CONST && ptr[itr] == NULL) {
			DEBUG("Attempting an allocate\n");
			size = (int)(RAND() * MAX_ALLOC_SIZE);
			if(size > 0) {
				ptr[itr] = shmHeapMalloc(size);
				//memset(ptr[itr], 0, size);
			}
			else
				continue;
			if(ptr[itr] == NULL) {
				DEBUG("malloc at iteration %d failed for size %d\n", i,size);
				fflush(stderr);
				++fail;
				continue;
			}
			/* Range check */
			for(j = 0; j < i; j++) {
				if(global[j][0] == -1) {
					continue;
				}
				if(((long int) ptr[itr] >= global[j][0]) && ((long int) ptr[itr]+size <= global[j][1])) {
					printf("[s] = %ld, [e] = %ld, [p] = %ld, [itr] = %d, [size] = %ld\n", global[j][0], global[j][1], ptr[itr], i, size);
					printf("Correctness check failed\n");
					exit(EXIT_FAILURE);
				}
			}
			global[i][0] = (long int) ptr[itr];
			global[i][1] = (long int) ptr[itr] + size;
			printf("Assigned: [s] = %ld, [e] = %ld, [p] = %ld, [itr] = %d, [size] = %ld\n", global[i][0], global[i][1], ptr[itr],i, size);

		} else if(randvar >= ALLOC_CONST && ptr[itr] != NULL) {
			DEBUG("Freeing ptr[%d]\n", itr);
			for(j = 0; j < i; j++) {
				if(global[j][0] == (long int) ptr[itr]) {
					global[j][0] = -1;
					global[j][1] = -1;
				}
			}
			shmHeapFree(ptr[itr]);
			ptr[itr] = NULL;
		}
		else {
			DEBUG("Fell through.  Try again\n");
			continue;
		}

		i++;
	}

	/*
	 * now -- free them
 	 */
	for(i=0; i < BUFLEN; i++) {
		if(ptr[i] != NULL) {
			shmHeapFree(ptr[i]);
			ptr[i] = NULL;
		}
	}
	end = clock();

	//print_freelists();
	DEBUG("\n");
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

	printf("Test case summary\n");
	printf("Loop count: %d, malloc successful: %d, malloc failed: %d, execution time: %g seconds\n\n", LOOPCNT, LOOPCNT-fail, fail, time_spent);

	printf("Stress testcases3 passed!\n");

	shmHeapDisp();
	return 0;
}

