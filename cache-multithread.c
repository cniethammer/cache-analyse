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
#include <pthread.h>
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
#define NUM_ACCESS_FACTOR 1
#endif

long int wset_start_size = 1 << 10;	// 1 kB
long int wset_final_size = 1 << 29;	// 512 MB


/***********************************************************************
 * data type definitions 
 ***********************************************************************/

typedef struct l {
  struct l *next;
  struct l *prev;
  long int pad[NPAD];
} list_elem;

typedef enum {
  FORWARD,
  BACKWARD
} direction_t;

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

  for( i = 1; i < size / sizeof(struct l) - 1; i++ ){
    wsetptr[i].next = &wsetptr[i+1];
    wsetptr[i].prev = &wsetptr[i-1];
  }
  wsetptr[0].next = &wsetptr[1];
  wsetptr[0].prev = &wsetptr[i]; // first element points to the last one
  wsetptr[i].next = &wsetptr[0]; // last element points to the first one
  wsetptr[i].prev = &wsetptr[i-1];
  
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

long int test_read( long int size, list_elem *wsetptr, direction_t direction, FILE *fp ) {

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
  switch( direction ){
    case FORWARD:
      for( access_num = 0; access_num < num_accesses; access_num++ )
	lptr = lptr->next;
      break;
    case BACKWARD:
      for( access_num = 0; access_num < num_accesses; access_num++ )
	lptr = lptr->prev;
      break;
  }
  stop = timer();

  exponent = log((double) size) / log(2.);
  fprintf( fp, "%4.lf %10.ld %16.2lf\n",exponent , size, num_accesses / (stop - start) );
      
  return 0;
}

typedef struct {
  list_elem * wsetptr;
  long int size;
  FILE * fp;
} read_head_data;

void * test_read_th( void *data) {
  list_elem *wsetptr = ((read_head_data*) data)->wsetptr;
  long int   size    = ((read_head_data*) data)->size;
  FILE      *fp      = ((read_head_data*) data)->fp; 
  test_read( size, wsetptr, FORWARD, fp );
  return NULL;
}

int main( int argc, char* argv[] ){

  int i;
  long int size = 1;
  list_elem *wsetptr;
  fprintf( stdout, "# Access padding: %ld\n#\n", NPAD * sizeof( long int ) );

#ifdef THREADED
  int num_threads = 2;
  pthread_t th[num_threads];
  long int *returnval[num_threads];
  read_head_data data[num_threads];
  char * filename;

  for( i = 0; i < num_threads; i++ ){
    sprintf( filename, "read-tests-th%d", i );
    data[i].fp = fopen( filename, "w+");
    fprintf( data[i].fp, "# sequentiall list forward\n" );
  }
#endif

  fprintf( stdout, "# sequentiall list forward\n" );
  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_sequential( size );
#ifdef THREADED
    {
      for( i = 0; i < num_threads; i++ ) {
	data[i].wsetptr = wsetptr;
	data[i].size = size;
	pthread_create( &th[i], NULL, &test_read_th, (void *) &data[i] );
      }
      for( i = 0; i < num_threads; i++ ) {
	pthread_join( th[i], (void **) &returnval[i]);
      }
    }
#else
    test_read( size, wsetptr, FORWARD );
#endif
    free( wsetptr );
  }
  fprintf( stdout, "\n\n" );

#ifdef SEQ_BACKWARD
  fprintf( stdout, "# sequentiall list backward\n" );
  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_sequential( size );
    test_read( size, wsetptr, BACKWARD );
    free( wsetptr );
  }
  fprintf( stdout, "\n\n" );
#endif

  fprintf( stdout, "# random list\n" );
#ifdef THREADED
  for( i = 0; i < num_threads; i++ )
    fprintf( data[i].fp, "\n\n# random list forward\n" );
#endif

  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_random( size );
#ifdef THREADED
    {
      for( i = 0; i < num_threads; i++ ) {
	data[i].wsetptr = wsetptr;
	data[i].size = size;
	pthread_create( &th[i], NULL, &test_read_th, (void *) &data[i] );
      }
      for( i = 0; i < num_threads; i++ ) {
	pthread_join( th[i], (void **) &returnval[i]);
      }
    }
#else
    test_read( size, wsetptr, FORWARD, stdout );
#endif
    free( wsetptr );
  }
  for( i = 0; i < num_threads; i++ )
    fclose( data[i].fp );
    

  return 0;
}
