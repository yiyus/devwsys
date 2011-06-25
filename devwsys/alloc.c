#include <lib9.h>

typedef struct{} Pool;

void* poolalloc(Pool *a, ulong b) { return malloc(b); }
void poolfree(Pool* a, void* b) { free(b); }

Pool imagmem;

void*
mallocz(ulong n, int clr)
{
	void *v;

	v = malloc(n);
	if(clr && v)
		memset(v, 0, n);
	return v;
}
