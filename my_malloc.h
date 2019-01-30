#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <assert.h>
// 
void * ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);


// Best fit malloc/free
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

unsigned long get_data_segment_size();
unsigned long get_data_segment_free_space_size();


