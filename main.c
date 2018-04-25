#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "shmHeap.h"

#undef TEST1
#undef TEST2
#undef TEST3
#define TEST4

/* Test program. */
int main(int argc, char **argv)
{
	printf("Heap manager\n");

	/* This is our heap. */
	static unsigned char h1[1000]; printf("%s(): h1 %p\n", __func__, h1); shmHeapInit(h1, sizeof(h1));
	static unsigned char h5[ 500]; printf("%s(): h5 %p\n", __func__, h5); shmHeapInit(h5, sizeof(h5));
	static unsigned char h2[4000]; printf("%s(): h2 %p\n", __func__, h2); shmHeapInit(h2, sizeof(h2));
//	static unsigned char h3[4000]; printf("%s(): h3 %p\n", __func__, h3); shmHeapInit(h3, sizeof(h3));
//	static unsigned char h4[9000]; printf("%s(): h4 %p\n", __func__, h4); shmHeapInit(h4, sizeof(h4));

	sizeTreeTraverse(sizeTreeRoot);
	addrTreeTraverse(addrTreeRoot);

#ifdef TEST1
	void *p1 = shmHeapMalloc(50); printf("%s(): p1 = %p\n", __func__, p1);
	void *p2 = shmHeapMalloc(50); printf("%s(): p2 = %p\n", __func__, p2);
	void *p3 = shmHeapMalloc(50); printf("%s(): p3 = %p\n", __func__, p3);
	void *p4 = shmHeapMalloc(50); printf("%s(): p4 = %p\n", __func__, p4);

	shmHeapFree(p1);
	shmHeapFree(p2);
	shmHeapFree(p3);
	shmHeapFree(p4);
#elif defined(TEST2)
	void *p1 = shmHeapMalloc(50); printf("%s(): p1 = %p\n", __func__, p1);
	void *p2 = shmHeapMalloc(50); printf("%s(): p2 = %p\n", __func__, p2);
	void *p3 = shmHeapMalloc(50); printf("%s(): p3 = %p\n", __func__, p3);
	void *p4 = shmHeapMalloc(50); printf("%s(): p4 = %p\n", __func__, p4);

	shmHeapFree(p3);
	shmHeapFree(p2);
	shmHeapFree(p4);
	shmHeapFree(p1);
#elif defined(TEST3)
	void *p1 = shmHeapMalloc(50); printf("%s(): p1 = %p\n", __func__, p1); shmHeapFree(p1);
	void *p2 = shmHeapMalloc(50); printf("%s(): p2 = %p\n", __func__, p2); shmHeapFree(p2);
	void *p3 = shmHeapMalloc(50); printf("%s(): p3 = %p\n", __func__, p3); shmHeapFree(p3);
	void *p4 = shmHeapMalloc(50); printf("%s(): p4 = %p\n", __func__, p4); shmHeapFree(p4);
#elif defined(TEST4)
	void *p1 = shmHeapMalloc(700); printf("%s(): p1 = %p\n", __func__, p1); shmHeapFree(p1);
#endif

	sizeTreeTraverse(sizeTreeRoot);
	addrTreeTraverse(addrTreeRoot);
	return 0;
}

