
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "shmHeap.h"

/*******************************************************************************
 * We maintain a Size Tree and an Address Tree.
 *
 * - The Size Tree is used to locate the smallest suitable chunk of free
 *   memory available when doing a malloc.
 *
 * - The Address Tree is used to help recombine chunks when they are freed.
 *   The recombining is done to reduce fragmentation.
 ******************************************************************************/

struct allocStruct;

static void *_shmHeapMalloc(size_t size);
static void _shmHeapFree(void *ptr);

#define SHM_HEAP_MAGIC 0xDEBB1E83

/******************************************************************************
 ******************************************************************************
 **** This is the implementation of the Size Tree.
 ******************************************************************************
 ******************************************************************************/
/* This is the definition of the Size Tree. */
typedef struct sizeTree {
	struct sizeTree *left;
	struct sizeTree *right;

	/* This is the key.  It represents the size of each chunk of memory
	 * in this node.  All of the chunks of memory in a node are the same
	 * size. */
	size_t size;

	/* This is a pointer to our internal data structure. */
	struct allocStruct *ptr;

	/* This is a linked list of all of the chunks.  There can be more
	 * than one chunk of free memory that is "size" bytes long.  We store
	 * them all together. */
	struct sizeTree *list;
} SizeTree;

static SizeTree *sizeTreeRoot = 0;

/* Traverse the tree (tree-verse (HAHA)). */
static void sizeTreeTraverse(SizeTree *tree)
{
	if(tree == NULL) {
		return;
	}

	sizeTreeTraverse(tree->left);
	fprintf(stderr, "size %ld: ptr ( ", tree->size);
	SizeTree *node = tree;
	while(node) {
		fprintf(stderr, "%p ", node->ptr);
		node = node->list;
	}
	fprintf(stderr, ")\n");
	sizeTreeTraverse(tree->right);
}

/* Search "tree" for the predecessor to "node".  Return it in "predecessor".
 * The entry remains in the tree.*/
static void sizeTreeFindPredecessor(SizeTree *tree, SizeTree *node, SizeTree **predecessor)
{
	if(tree == NULL) {
		return;
	}

	/* If the current value is larger or equal to "node", look for something
	 * smaller. */
	if(tree->size >= node->size) {
		sizeTreeFindPredecessor(tree->left, node, predecessor);
	}

	/* The current value is smaller.  Save it and look down the right side
	 * for a better match. */
	else {
		*predecessor = tree;
		sizeTreeFindPredecessor(tree->right, node, predecessor);
	}
}

/* Locate an entry of "size" bytes (or slightly larger) from "tree".  Return it
 * in "node".  We're essentially looking for either an exact match or the
 * successor to an exact match.
 */
static void sizeTreeFindNode(SizeTree *tree, size_t size, SizeTree **node)
{
	if(tree == NULL) {
		return;
	}

	/* If the current value is too small, look for something larger. */
	if(tree->size < size) {
		sizeTreeFindNode(tree->right, size, node);
	}

	/* This size will work.  Save it and look down the left side for a
	 * better match. */
	else {
		*node = tree;
		sizeTreeFindNode(tree->left, size, node);
	}
}

/* Insert "node" into "tree". */
static SizeTree *sizeTreeInsertNode(SizeTree *tree, SizeTree *node)
{
	if(tree == NULL) {
		tree = node;
	}
	else if(tree->size == node->size) {
		node->list = tree->list;
		tree->list = node;
	}
	else if(tree->size > node->size) {
		tree->left = sizeTreeInsertNode(tree->left, node);
	}
	else {
		tree->right = sizeTreeInsertNode(tree->right, node);
	}

	return tree;
}

/* Locate and remove "node" from "tree". */
static SizeTree *sizeTreeRemoveNode(SizeTree *tree, SizeTree *node, int *success)
{
	if(tree != NULL) {

		/* If "node" is at the head of the "tree"... */
		if(tree == node) {
			if(node->list) {
				/* There are other nodes of the same size.  Just
				 * promote one of them up so it's the new head
				 * of the tree. */
				node->list->left = node->left;
				node->list->right = node->right;
				tree = node->list;
			}
			else if(node->left == NULL) {
				/* Left side is empty.  Promote the right side up
				 * one level.  Note that the right side might also
				 * be empty. */
				tree = node->right;
			}
			else if(node->right == NULL) {
				/* Right side is empty.  Promote the left side up
				 * one level.  Note that the left side might also
				 * be empty. */
				tree = node->left;
			}
			else {
				/* There are left and right entries.  Use the
				 * left side as the new tree, and hook the right
				 * side to the far-right edge of the left side. */
				SizeTree *pred = NULL;
				sizeTreeFindPredecessor(tree->left, node, &pred);
				tree = node->left;
				pred->right = node->right;
			}

			*success = 1;
		}

		 /* If our "node" is on the "list" for the size we're staring at,
		  * just remove it from the list.  The tree doesn't change. */
		else if(tree->size == node->size) {
			SizeTree *n = tree;
			while(n) {
				if(n->list == node) {
					n->list = node->list;
					*success = 1;
					break;
				}
				n = n->list;
			}
		}

		/* If the current tree node is too big, look for something smaller. */
		else if(tree->size > node->size) {
			tree->left = sizeTreeRemoveNode(tree->left, node, success);
		}

		/* If the current tree node is too small, look for something larger. */
		else {
			tree->right = sizeTreeRemoveNode(tree->right, node, success);
		}
	}

	return tree;
}

/******************************************************************************
 ******************************************************************************
 **** This is the implementation of the Address Tree.
 ******************************************************************************
 ******************************************************************************/
/* This is the definition of the address tree. */
typedef struct addrTree {
	struct addrTree *left;
	struct addrTree *right;

	/* This is a pointer to our internal data structure. */
	struct allocStruct *ptr;
} AddrTree;

static AddrTree *addrTreeRoot = 0;

/* Traverse the tree. */
static void addrTreeTraverse(AddrTree *tree)
{
	if(tree == NULL) {
		return;
	}

	addrTreeTraverse(tree->left);
	fprintf(stderr, "addr %p\n", tree->ptr);
	addrTreeTraverse(tree->right);
}

/* Find the largest address in the Addr Tree that is smaller than ptr.  Return
 * it in "predecessor".  Note that the entry remains in the tree. */
static void addrTreeFindPredecessor(AddrTree *tree, AddrTree *node, AddrTree **predecessor)
{
	if(tree == NULL) {
		return;
	}

	/* If the current value is larger than or equal to "node", look for
	 * something smaller. */
	if(tree->ptr >= node->ptr) {
		addrTreeFindPredecessor(tree->left, node, predecessor);
	}

	/* The current value is smaller.  Save it and look down the right side
	 * for a better match. */
	else {
		*predecessor = tree;
		addrTreeFindPredecessor(tree->right, node, predecessor);
	}
}

static AddrTree *addrTreeInsertNode(AddrTree *tree, AddrTree *node)
{
	if(tree == NULL) {
		tree = node;
	}
	else if(tree->ptr > node->ptr) {
		tree->left = addrTreeInsertNode(tree->left, node);
	}
	else {
		tree->right = addrTreeInsertNode(tree->right, node);
	}

	return tree;
}

static AddrTree *addrTreeRemove(AddrTree *tree, AddrTree *node, int *status)
{
	if(tree == NULL) {
		return NULL;
	}
	else if(tree->ptr > node->ptr) {
		tree->left = addrTreeRemove(tree->left, node, status);
	}
	else if(tree->ptr < node->ptr) {
		tree->right = addrTreeRemove(tree->right, node, status);
	}
	else {
		/* We found our entry. */
		*status = 1;

		if(tree->right == NULL) {
			tree = tree->left;
		}
		else if(tree->left == NULL) {
			tree = tree->right;
		}
		else {
			/* There are left and right entries.  Use the left side
			 * as the new tree, and hook the right side to the
			 * far-right edge of the left side. */
			AddrTree *pred = NULL;
			addrTreeFindPredecessor(tree->left, node, &pred);
			tree = node->left;
			pred->right = node->right;
		}
	}

	return tree;
}

/******************************************************************************
 ******************************************************************************
 **** This is the public API.
 ******************************************************************************
 ******************************************************************************/

/* This is the data structure that is used for each chunk of allocated mem. */
typedef struct allocStruct {
	uint32_t magic;
	size_t size;
	int allocated;

	/* This is how we store it in the Size Tree. */
	SizeTree sizeTreeNode;

	/* This is how we store it in the Address Tree. */
	AddrTree addrTreeNode;

	/* The actual data. */
	unsigned char data[0];
} AllocStruct;
static size_t AllocStructDataOffset = (size_t) (&((AllocStruct *)0)->data);

static int counterMalloc = 0;
static int counterFree = 0;

/* This function can be called more than once if you want to manage more than
 * 1 chunk of memory.
 */
void shmHeapInit(unsigned char *heap, size_t size)
{
	unsigned char *heapEnd = heap + size;

	/* Create a dummy AllocStruct data structure at the end of this heap.
	 * We use that data structure as a flag to let us know it's the end of
	 * this heap (and we can't go past it). */
	AllocStruct *endStruct = (AllocStruct *) (heapEnd - sizeof(AllocStruct));
	endStruct->magic = SHM_HEAP_MAGIC;
	endStruct->size = 0;
	endStruct->allocated = 1;

	endStruct->addrTreeNode.ptr = endStruct;
	addrTreeRoot = addrTreeInsertNode(addrTreeRoot, &endStruct->addrTreeNode);

	endStruct->sizeTreeNode.size = 0;
	endStruct->sizeTreeNode.ptr = endStruct;
	sizeTreeRoot = sizeTreeInsertNode(sizeTreeRoot, &endStruct->sizeTreeNode);

	/* Adjust size to allow for endStruct. */
	size -= sizeof(*endStruct);

	/* Create an AllocStruct data structure that matches the format that is
	 * created by shmHeapMalloc().  Then pass it to _shmHeapFree().  This
	 * way it looks like a regular call to _shmHeapFree(). */
	AllocStruct *newStruct = (AllocStruct *) heap;
	newStruct->magic = SHM_HEAP_MAGIC;
	newStruct->size = size - AllocStructDataOffset;
	newStruct->allocated = 1;
	_shmHeapFree(newStruct->data);
}

static void *_shmHeapMalloc(size_t size)
{
	SizeTree *sizeTreeNode = 0;
	sizeTreeFindNode(sizeTreeRoot, size + sizeof(AllocStruct), &sizeTreeNode);
	if(sizeTreeNode == 0) 	{
		fprintf(stderr, "%s(): ERROR: Out of memory.\n", __func__);
		return (void *) NULL;
	}

	int success = 0;
	sizeTreeRoot = sizeTreeRemoveNode(sizeTreeRoot, sizeTreeNode, &success);
	if(success == 0) {
		fprintf(stderr, "%s(): ERROR: Didn't find matching size node\n", __func__);
	}

	AllocStruct *curr = sizeTreeNode->ptr;
	if(curr->magic != SHM_HEAP_MAGIC) {
		fprintf(stderr, "%s(): ERROR: Invalid header.\n", __func__);
	}

	success = 0;
	addrTreeRoot = addrTreeRemove(addrTreeRoot, &curr->addrTreeNode, &success);
	if(success == 0) {
		fprintf(stderr, "%s(): ERROR: Didn't find matching addr node\n", __func__);
	}

	/* Calculate the total number of bytes required to service this alloc
	 * request, and calculate the number of left over bytes in this chunk
	 * of memory. */
	size_t currFullSize = sizeof(AllocStruct) + size;
	long extraBytes = curr->size - size;

	memset(curr, 0, sizeof(*curr));
	curr->magic = SHM_HEAP_MAGIC;
	curr->size = size;
	curr->allocated = 1;

	curr->addrTreeNode.left = curr->addrTreeNode.right = NULL;
	curr->addrTreeNode.ptr = curr;

	curr->sizeTreeNode.left = curr->sizeTreeNode.right = NULL;
	curr->sizeTreeNode.size = size;
	curr->sizeTreeNode.ptr = curr;

	/* Can we split this chunk?  There has to be enough extra space to create
	 * a new AllocStruct header.  If there is enough space, we'll split it
	 * and create a new chunk. */
	if(extraBytes > 0) {
		AllocStruct *extra = (AllocStruct *) ((unsigned char *) curr + currFullSize);
		memset(extra, 0, sizeof(*extra));
		extra->magic = SHM_HEAP_MAGIC;
		extra->size = extraBytes - AllocStructDataOffset;
		extra->allocated = 1;
		extra->sizeTreeNode.size = extra->size;
		extra->sizeTreeNode.ptr = extra;
		extra->addrTreeNode.ptr = extra;
		_shmHeapFree(extra->data);
	}

	return curr->data;
}

static void _shmHeapFree(void *ptr)
{
	if(ptr == NULL) {
		return;
	}

	AllocStruct *curr;
	curr = (AllocStruct *) ((unsigned char *) ptr - AllocStructDataOffset);
	if(curr->magic != SHM_HEAP_MAGIC) {
		fprintf(stderr, "%s(): ERROR: Invalid header.\n", __func__);
		return;
	}

	/* Check to see if the next memory block (a.k.a. the successor) is
	 * currently free.  If it is, combine it with this memory block.  This
	 * reduces fragmentation. */
	AllocStruct *next = (AllocStruct *) ((unsigned char *) curr +
	                    AllocStructDataOffset + curr->size);
	if(next->magic != SHM_HEAP_MAGIC) {
		fprintf(stderr, "%s(): ERROR: Invalid header.\n", __func__);
		return;
	}
	if(next->allocated == 0) {
		int success = 0;
		sizeTreeRoot = sizeTreeRemoveNode(sizeTreeRoot, &next->sizeTreeNode, &success);
		//fprintf(stderr, "REMOVED sizeTreeNode %p? %d\n", &next->sizeTreeNode, success);
		if(success == 0) {
			fprintf(stderr, "ERROR: Unable to locate sizeTreeNode.\n");
		}

		success = 0;
		addrTreeRoot = addrTreeRemove(addrTreeRoot, &next->addrTreeNode, &success);
		//fprintf(stderr, "REMOVED addrTreeNode %p? %d\n", &next->addrTreeNode, success);
		if(success == 0) {
			fprintf(stderr, "ERROR: Unable to locate addrTreeNode.\n");
		}

		curr->size += next->size + sizeof(AllocStruct);
		memset(next, 0, sizeof(*next));
	}

	/* Check to see if the previous memory block (a.k.a. the predecessor) is
	 * currently free.  If it is, combine it with this memory block.  This
	 * reduces fragmentation. */
	AddrTree *pred = NULL;
	addrTreeFindPredecessor(addrTreeRoot, &curr->addrTreeNode, &pred);
	if(pred) {
		/* Set prev to the predecessor.  Note that this refers to the
		 * previous block that is unallocated.  We now need to look and
		 * see if that block is adjacent to the block we're freeing. */
		AllocStruct *prev = pred->ptr;
		//fprintf(stderr, "prev->magic = %08x: prev->allocated = %d\n", prev->magic, prev->allocated);

		AllocStruct *prev2 = (AllocStruct *) ((unsigned char *) prev + sizeof(AllocStruct) + prev->size);
		//fprintf(stderr, "prev2->magic = %08x: prev2->allocated = %d\n", prev2->magic, prev2->allocated);

		//fprintf(stderr, "prev2 %p : curr %p\n", prev2, curr);
		if(prev2 == curr) {
			int success = 0;
			sizeTreeRoot = sizeTreeRemoveNode(sizeTreeRoot, &prev->sizeTreeNode, &success);
			//fprintf(stderr, "REMOVED pred sizeTreeNode? %d\n", success);
			if(success == 0) {
				fprintf(stderr, "ERROR: Unable to locate sizeTreeNode.\n");
			}

			success = 0;
			addrTreeRoot = addrTreeRemove(addrTreeRoot, &prev->addrTreeNode, &success);
			//fprintf(stderr, "REMOVED pred addrTreeNode? %d\n", success);
			if(success == 0) {
				fprintf(stderr, "ERROR: Unable to locate addrTreeNode.\n");
			}

			size_t prevSize = prev->size + curr->size + sizeof(AllocStruct);
			memset(curr, 0, sizeof(*curr));

			memset(prev, 0, sizeof(*prev));
			prev->magic = SHM_HEAP_MAGIC;
			prev->size = prevSize;
			prev->sizeTreeNode.size = prev->size;
			prev->sizeTreeNode.ptr = prev;
			prev->addrTreeNode.ptr = prev;

			curr = prev;
		}
	}

	curr->allocated = 0;

	/* Place the chunk into the Size and Address Trees.  It is now
	 * available for re-allocation. */
	curr->addrTreeNode.ptr = curr;
	addrTreeRoot = addrTreeInsertNode(addrTreeRoot, &curr->addrTreeNode);

	curr->sizeTreeNode.size = curr->size;
	curr->sizeTreeNode.ptr = curr;
	sizeTreeRoot = sizeTreeInsertNode(sizeTreeRoot, &curr->sizeTreeNode);
}

void *shmHeapMalloc(size_t size)
{
	counterMalloc++;
	return _shmHeapMalloc(size);
}

void shmHeapFree(void *ptr)
{
	counterFree++;
	_shmHeapFree(ptr);
}

void shmHeapDisp(void)
{
	fprintf(stderr, "%s(): counterMalloc = %d: counterFree = %d.\n", __func__, counterMalloc, counterFree);

	fprintf(stderr, "This is the Size Tree:\n");
	sizeTreeTraverse(sizeTreeRoot);
	fprintf(stderr, "\n");

	fprintf(stderr, "This is the Address Tree:\n");
	addrTreeTraverse(addrTreeRoot);
	fprintf(stderr, "\n");
}

