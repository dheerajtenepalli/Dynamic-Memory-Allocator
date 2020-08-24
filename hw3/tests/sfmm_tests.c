#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"

#define MIN_BLOCK_SIZE (32)

void assert_free_block_count(size_t size, int count);
void assert_free_list_block_count(size_t size, int count);

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & BLOCK_SIZE_MASK))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		 size, count, cnt);
}

Test(sf_memsuite_student, malloc_an_Integer_check_freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sf_memsuite_student, malloc_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(3 * PAGE_SZ - 2 * sizeof(sf_block));

	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_over_four_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(PAGE_SZ << 2);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(16336, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sf_memsuite_student, free_quick, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(32);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(3936, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_no_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(224, 1);
	assert_free_block_count(3760, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *w = */ sf_malloc(8);
	void *x = sf_malloc(200);
	void *y = sf_malloc(300);
	/* void *z = */ sf_malloc(4);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 2);
	assert_free_block_count(544, 1);
	assert_free_block_count(3440, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *u = sf_malloc(200);
	/* void *v = */ sf_malloc(300);
	void *w = sf_malloc(200);
	/* void *x = */ sf_malloc(500);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(700);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 4);
	assert_free_block_count(224, 3);
	assert_free_block_count(1808, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 2*sizeof(sf_header),
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 2*sizeof(sf_header));
}

Test(sf_memsuite_student, realloc_larger_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(10);
	x = sf_realloc(x, sizeof(int) * 10);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 64, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 2);
	assert_free_block_count(3920, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int) * 8);
	void *y = sf_realloc(x, sizeof(char));

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 48, "Block size not what was expected!");

	// There should be only one free block of size 4000.
	assert_free_block_count(0, 1);
	assert_free_block_count(4000, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_free_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(double) * 8);
	void *y = sf_realloc(x, sizeof(int));

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 32, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size 48 to the freelist.
	// This block will go into the main freelist and be coalesced.
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

Test(yatna_suite, 1_malloc_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void * x = sf_malloc(0);

	cr_assert_null(x, "x is not NULL!");

	x = sf_malloc(sizeof(double)*100000);

	cr_assert_null(x, "x is not NULL!");
	cr_assert_eq(sf_errno,ENOMEM,"Enomem not set");
}
Test(yatna_suite, 2_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	char * x = NULL;
	sf_free(x);
}
Test(yatna_suite, 3_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	sf_malloc(8);
	sf_free((char *)sf_mem_start() + 8);
}
Test(yatna_suite, 4_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	sf_malloc(8);
	sf_free((char *)sf_mem_start() + 16);
}
Test(yatna_suite, 5_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	sf_malloc(8);
	sf_free((char *)sf_mem_end());
}
Test(yatna_suite, 6_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	sf_malloc(8);
	sf_free((char *)sf_mem_end() -8);
}
Test(yatna_suite, 7_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	char * x = sf_malloc(8);
	sf_free(x - 8);
}
Test(yatna_suite, 8_free_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	char * x = sf_malloc(8);
	sf_free(x + 8);
}
Test(yatna_suite, 9_realloc_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini, .signal = SIGABRT) {
	sf_realloc(sf_mem_start(),32);
}
Test(yatna_suite, 10_realloc_edge_cases, .init = sf_mem_init, .fini = sf_mem_fini) {
	size_t * t = sf_malloc(sizeof(size_t));
	*t = 1234567;
	void * nt = sf_realloc(t,0);
	cr_assert_null(nt,"Valit ptr 0 size not returning null");
	t = sf_malloc(sizeof(size_t));
	nt = sf_realloc(t,1000000);
	cr_assert_null(nt, "x is not NULL!");
	cr_assert_eq(sf_errno,ENOMEM,"Enomem not set");

}

Test(yatna_suite, 11_malloc_free, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));
	cr_assert_not_null(x, "x is NULL!");
	*x = 4;
	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);

	long *y = sf_malloc(sizeof(long)*2);
	*y = 12345678912345678;
	*(y+1) = 12345678912345678;
	cr_assert(*y == 12345678912345678, "sf_malloc failed to give proper space for an long!");
	cr_assert(*(y+1) == 12345678912345678, "sf_malloc failed to give proper space for an long!");
	assert_free_block_count(0, 1);
	assert_free_block_count(3984, 1);

	char *z = sf_malloc(sizeof(char)*17);
	for(int i=0;i<17;i++)
		*(z+i) = 'a'+i;
	cr_assert(*(z) == 'a', "sf_malloc failed to give proper space for an char!");
	cr_assert(*(z+16) == 'q', "sf_malloc failed to give proper space for an char!");
	assert_free_block_count(0, 1);
	assert_free_block_count(3936, 1);

	sf_free(y);
	assert_free_block_count(0, 2);
	assert_free_block_count(32, 1);
	assert_free_block_count(3936, 1);

	sf_free(x);
	assert_free_block_count(0, 2);
	assert_free_block_count(64, 1);
	assert_free_block_count(3936, 1);

	sf_free(z);
	assert_free_block_count(0, 1);
	assert_free_block_count(4048, 1);


	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");

}
Test(yatna_suite, 12_malloc_free, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	char *x = sf_malloc(4032);
	cr_assert_not_null(x, "x is NULL!");
	*x = 'a';
	cr_assert(*x == 'a', "sf_malloc failed to give proper space for an int!");
	assert_free_block_count(0, 0);

	sf_free(x);
	assert_free_block_count(0, 1);
	assert_free_block_count(4048, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(yatna_suite, 13_malloc_free, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(8144); //4048 + 4096
	cr_assert_not_null(x, "x is NULL!");
	*x = 4;
	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");
	assert_free_block_count(0, 1);
	assert_free_block_count(4080, 1);

	char * z = sf_malloc(2064);
	assert_free_block_count(0, 1);
	assert_free_block_count(2000, 1);

	char * y = sf_malloc(1);
	assert_free_block_count(0, 1);
	assert_free_block_count(1968, 1);

	sf_free(x);
	assert_free_block_count(0, 2);
	assert_free_block_count(1968, 1);
	assert_free_block_count(8160, 1);

	sf_malloc(1);
	assert_free_block_count(0, 2);
	assert_free_block_count(1936, 1);
	assert_free_block_count(8160, 1);

	sf_free(y);
	assert_free_block_count(0, 3);
	assert_free_block_count(32, 1);
	assert_free_block_count(1936, 1);
	assert_free_block_count(8160, 1);

	sf_free(z);
	assert_free_block_count(0, 2);
	assert_free_block_count(1936, 1);
	assert_free_block_count(10272, 1);

	char * a = sf_malloc(64);
	sf_malloc(64);
	char * b = sf_malloc(65);
	sf_malloc(65);
	char * c = sf_malloc(96);
	sf_malloc(96);

	assert_free_block_count(0, 2);
	assert_free_block_count(1360, 1);
	assert_free_block_count(10272, 1);
	sf_free(c);
	sf_free(b);
	sf_free(a);

	assert_free_block_count(0, 5);
	assert_free_block_count(80, 1);
	assert_free_block_count(96, 1);
	assert_free_block_count(112, 1);
	assert_free_block_count(1360, 1);
	assert_free_block_count(10272, 1);

	sf_malloc(80);
	assert_free_block_count(96, 0);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + 3*PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(yatna_suite, 14_realloc_same, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	//same size
	int *x = sf_malloc(sizeof(int));
	*x = -2147483648;
	x = sf_realloc(x,sizeof(long));
	cr_assert_not_null(x, "x is NULL!");
	cr_assert(*x == -2147483648, "sf_reallox failed to give proper space for an int!");
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);

	x = sf_realloc(x,sizeof(int));
	cr_assert_not_null(x, "x is NULL!");
	cr_assert(*x == -2147483648, "sf_reallox failed to give proper space for an int!");
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);


	sf_free(x);
	assert_free_block_count(0, 1);
	assert_free_block_count(4048, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}
Test(yatna_suite, 14_realloc_smaller, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	//no splinter
	int *x = sf_malloc(48); //64
	*x = -2147483648;
	x = sf_realloc(x,32);
	cr_assert_not_null(x, "x is NULL!");
	cr_assert(*x == -2147483648, "sf_reallox failed to give proper space for an int!");
	assert_free_block_count(0, 1);
	assert_free_block_count(3984, 1);

	sf_malloc(1);
	assert_free_block_count(0, 1);
	assert_free_block_count(3952, 1);

	//splinter
	x = sf_realloc((void *)x,sizeof(int)); //32
	cr_assert_not_null(x, "x is NULL!");
	cr_assert(*x == -2147483648, "sf_reallox failed to give proper space for an int!");
	assert_free_block_count(0, 2);
	assert_free_block_count(32, 1);
	assert_free_block_count(3952, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(yatna_suite, 15_realloc_larger, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	//no splinter
	int *x = sf_malloc(16); //32
	*x = -2147483648;
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);
	x = sf_realloc(x,48);
	cr_assert_not_null(x, "x is NULL!");
	cr_assert(*x == -2147483648, "sf_reallox failed to give proper space for an int!");
	assert_free_block_count(0, 2);
	assert_free_block_count(32, 1);
	assert_free_block_count(3952, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}