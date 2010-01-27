/*
 * Cache analyses program
 * Implemented on the bases of Ullrich Dreppers "What every Programmer should know about Memory"
 *
 * date: 17.10.2010
 * author: Christoph Niethammer
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "timer.h"

/* definitions and default values */

#ifndef CLEAR_CACHE_BLOCK_SIZE
#define CLEAR_CACHE_BLOCK_SIZE 16*1024*1024	// 16 MB
#endif

#ifndef NPAD
#define NPAD 0
#endif

#ifndef NUM_ACCESS_FACTOR 
#define NUM_ACCESS_FACTOR 2
#endif

long int wset_start_size = 1 << 10;	// 1 kB
long int wset_final_size = 1 << 29;	// 512 MB


/***********************************************************************
 * data type definitions 
 ***********************************************************************/

typedef struct l {
  struct l *next;
  long int pad[NPAD];
} list_elem;

/***********************************************************************
 * function definitions
 ***********************************************************************/

/*
 * this function clears the CPU cache by accessing all elements of a large array.
 */
int clear_cache(){
  const long int num_ints = (CLEAR_CACHE_BLOCK_SIZE + sizeof(int)) / sizeof(int);
  long int i;
  long int value = 0;
  int * large_mem;

  large_mem = (int *)malloc (num_ints * sizeof(int));
  if (large_mem == NULL)
    return 0;

  for (i=0; i < num_ints; i++)
    value += large_mem[i];
  free( large_mem );

  return value;
}

/*
 * allocates an array of 'list_elem'ents with at least size size Byte.
 * The list elements are connected in a sequential roud robing way.
 */
list_elem * init_sequential( long int size ){
  long int i;
  list_elem *wsetptr;

  wsetptr = (list_elem *)  malloc( size + sizeof(list_elem ) );
  if( wsetptr == NULL )
    return NULL;

  for( i = 0; i < size / sizeof(struct l) - 1; i++ )
    wsetptr[i].next = &wsetptr[i+1];
  wsetptr[i].next = &wsetptr[0]; // last element points to the first one
  
  return wsetptr;
}

/*
 * allocates an array of 'list_elem'ents with at least size size Byte.
 * The list elements are connected in a random roud robing way.
 */
list_elem * init_random( long int size ){
  long int i;
  list_elem *wsetptr;

  wsetptr = (list_elem *) malloc( size + sizeof(list_elem ) );
  if( wsetptr == NULL )
    return NULL;

  /* set all pointers to 0 so we know later on which elements were already 
   * used as their pointers will be unequal 0 */
  for( i = 0; i < (size/sizeof(list_elem)); i++ )
    wsetptr[i].next = 0;

# define FIRST_ID 0
  long int id = FIRST_ID;

  for( i = 0; i < (size/sizeof(list_elem) - 1); i++ ){
    long int nextid;
    do {
      nextid = ( random() % (1 + size/sizeof(list_elem)) );
    } while( wsetptr[nextid].next != 0 );
    wsetptr[id].next = &wsetptr[nextid];
    id = nextid;
  }
  wsetptr[id].next = &wsetptr[FIRST_ID]; // last element points to the first one
  
  return wsetptr;
}

long int test_read( long int size, list_elem *wsetptr ) {

  long int i;
  long int access_num;
  long int num_accesses = NUM_ACCESS_FACTOR * wset_final_size / sizeof( list_elem * );
  double start, stop;
  list_elem *lptr;
  double exponent;

  if( wsetptr == NULL )
    return 0;
  lptr = wsetptr;

  clear_cache();

  start = timer();
  /* Main loop acessing the data set */
  for( access_num = 0; access_num < num_accesses; access_num++ )
    lptr = lptr->next;
  stop = timer();

  exponent = log((double) size) / log(2.);
  fprintf( stdout, "%4.lf %10.ld %16.2lf\n",exponent , size, num_accesses / (stop - start) );
      
  return 0;
}

int main( int argc, char* argv[] ){

  long int size = 1;
  list_elem *wsetptr;
  fprintf( stdout, "# Access padding: %ld\n#\n", NPAD * sizeof( long int ) );

  fprintf( stdout, "# sequentiall list\n" );
  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_sequential( size );
    test_read( size, wsetptr );
    free( wsetptr );
  }
  fprintf( stdout, "\n\n" );

  fprintf( stdout, "# random list\n" );
  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_random( size );
    test_read( size, wsetptr );
    free( wsetptr );
  }

  return 0;
}
