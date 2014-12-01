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
#include "cycle.h"
#ifdef PAPI
#include <papi.h>
#if 0
const int num_hwcntrs = 2;
int Events[4] = {PAPI_TOT_CYC,PAPI_TLB_TL,PAPI_L2_DCM,PAPI_L2_DCA};
char * EventStrings[4] = {"PAPI_TOT_CYC", "PAPI_TLB_TL", "PAPI_L2_DCM", "PAPI_L2_DCA" };
#endif
#if 1
const int num_hwcntrs = 3;
int Events[3] = {PAPI_TOT_CYC,PAPI_L2_DCM,PAPI_L2_DCA};
char * EventStrings[3] = {"PAPI_TOT_CYC", "PAPI_L2_DCM", "PAPI_L2_DCA" };
#endif

#endif

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
  char pad[NPAD];
} list_elem;

/***********************************************************************
 * function definitions
 ***********************************************************************/

#ifdef PAPI
int papi_error_handler (int retval) {
	switch (retval) {
		case PAPI_EINVAL: 
		 	fprintf(stderr, "One or more of the arguments is invalid.\n");
			break;
		case PAPI_EISRUN : 
		 	fprintf(stderr, "Counters have already been started, you must call PAPI_stop_counters() before you call this function again.\n");
			break;
		case PAPI_ESYS : 
			fprintf(stderr, "A system or C library call failed inside PAPI, see the errno variable.\n");
			break;
		case PAPI_ENOMEM : 
			fprintf(stderr, "Insufficient memory to complete the operation.\n");
			break;
		case PAPI_ECNFLCT : 
			fprintf(stderr, "The underlying counter hardware cannot count this event and other events in the EventSet simultaneously.\n");
			break;
		case PAPI_ENOEVNT : 
			fprintf(stderr, "The PAPI preset is not available on the underlying hardware.\n");
			break;
		default:
			fprintf(stderr, "Unknown PAPI error (%d).\n", retval);
		}
	exit(1);
}
#endif

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
 * The list elements are connected in an inverse sequential roud robing way.
 */
list_elem * init_inverse_sequential( long int size ){
  long int i;
  list_elem *wsetptr;

  wsetptr = (list_elem *)  malloc( size + sizeof(list_elem ) );
  if( wsetptr == NULL )
    return NULL;

  for( i = 1; i < size / sizeof(struct l); i++ )
    wsetptr[i].next = &wsetptr[i-1];
  wsetptr[0].next = &wsetptr[i-1]; // first element points to the last one
  
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
  ticks ticks1, ticks2;

  if( wsetptr == NULL )
    return 0;
  lptr = wsetptr;

  clear_cache();

  start = timer();
  ticks1 = getticks();

#ifdef PAPI
	long long values1[num_hwcntrs];
	long long values2[num_hwcntrs];
	PAPI_accum_counters	( values1, num_hwcntrs );
#endif
  /* Main loop acessing the data set */
  for( access_num = 0; access_num < num_accesses; access_num++ )
    lptr = lptr->next;
#ifdef PAPI
	PAPI_accum_counters	( values1, num_hwcntrs );
#endif

  ticks2 = getticks();
  stop = timer();

  long test = (long) lptr;
  fprintf(stderr, "%ld\r", test );
  exponent = log((double) size) / log(2.);
  /* log2(size)  size  accesses/s  cyc/access */
#ifdef PAPI
  fprintf( stdout, "%4.lf %12.ld %16.2lf %8.1lf",exponent , size, num_accesses / (stop - start), (double)(ticks2 - ticks1) / num_accesses );
	int ii;
	for( ii = 0; ii < num_hwcntrs; ii++) {
		fprintf(stdout, "\t%lld", values1[ii] );
	}
  fprintf( stdout, "\n" );
#else
  fprintf( stdout, "%4.lf %12.ld %16.2lf %8.1lf\n",exponent , size, num_accesses / (stop - start), (double)(ticks2 - ticks1) / num_accesses );
#endif
      
  return ticks2 - ticks1;
}

int main( int argc, char* argv[] ){

  long int size = 1;
  list_elem *wsetptr;

#ifdef PAPI
  int retval;

  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
	fprintf(stderr,"PAPI library init error!\n");
	papi_error_handler(retval);
	exit(1);
  }	
  retval = PAPI_start_counters(Events, num_hwcntrs);
  if (retval < PAPI_OK) {
	fprintf(stderr,"PAPI event initialization failed\n");
	papi_error_handler(retval);
	exit(1);
  }

#endif
  fprintf(stdout,"# 10^n\tsize\taccess/sec\tticks/access" );
#ifdef PAPI
  int j;
  for( j = 0; j < num_hwcntrs; j++ ) {
	fprintf(stdout,"\t%s", EventStrings[j]);
  }
#endif
  fprintf(stdout,"\n" );

  fprintf( stdout, "# Access padding: %ld Bytes\n#\n", NPAD * sizeof(char) );

  fprintf( stdout, "# sequentiall list\n" );
  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_sequential( size );
    test_read( size, wsetptr );
    free( wsetptr );
  }
  fprintf( stdout, "\n\n" );

  fprintf( stdout, "# inverse sequentiall list\n" );
  for( size = wset_start_size; size <= wset_final_size; size *= 2 ) {
    wsetptr = init_inverse_sequential( size );
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
#ifdef PAPI
  long long dummy[num_hwcntrs];
  retval = PAPI_stop_counters(dummy, num_hwcntrs);
  if (retval < PAPI_OK) {
    fprintf(stderr,"PAPI event initialization failed\n");
    papi_error_handler(retval);
    exit(1);
  }
#endif

  return 0;
}
