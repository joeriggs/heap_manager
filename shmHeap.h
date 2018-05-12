#ifndef __SHM_HEAP_H__
#define __SHM_HEAP_H__

extern void shmHeapInit(unsigned char *heap, size_t size);
extern void *shmHeapMalloc(size_t size);
extern void shmHeapFree(void *ptr);
extern void shmHeapDisp(void);

#endif // __SHM_HEAH_H__
