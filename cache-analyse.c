/*
 * Cache analyses program
 * Implemented on the bases of Ullrich Dreppers "What every Programmer should know about Memory"
 *
 * date: 17.10.2010
 * author: Christoph Niethammer <christoph.niethammer@gmail.com>
 *
 */
#include "timer.h"
#include "cycle.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef PAPI
#include <papi.h>
#endif

#ifdef PAPI
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
#endif /* PAPI */

/***********************************************************************
 * data type definitions
 ***********************************************************************/
/** data element */
struct l {
  struct l *next; /**< pointer to next data element */
  char pad[NPAD]; /**< padding */
};
typedef struct l list_elem;

/***********************************************************************
 * definitions and default values
 ***********************************************************************/
/* size of the allocated memory region used to clear CPU caches */
#ifndef CLEAR_CACHE_BLOCK_SIZE
#define CLEAR_CACHE_BLOCK_SIZE 16*1024*1024	// 16 MB
#endif

/* size of the padding used in the data stucture, actual size of data structure
 * may be lareger due to alignment by the compiler */
#ifndef NPAD
#define NPAD 0
#endif

#ifndef NUM_ACCESS_FACTOR 
#define NUM_ACCESS_FACTOR 2
#endif

/* working set minimum and maximum size */
long int wset_start_size = sizeof(list_elem);	// minimum is size of list_elem
long int wset_final_size = 1 << 27;	// 128 MB
long int wset_stride = 1; // stride between elements in array to be considered
                          // For stride 2  elements 0, 2, 4, 6, ... will be used for access

#ifndef SMALL_ARRAY_LIMIT
#define SMALL_ARRAY_LIMIT (1024)
#endif
double factor = 1.05;

FILE *logfile;

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

/**
 * Clear the CPU cache by accessing all elements of an large array.
 * @return random int computed from the uninitialized memory
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

/**
 * Allocate an array of 'list_elem'ents with at least size size Byte.
 * The list elements are connected in a sequential round robing way.
 * @return pointer to the begin of array, NULL in case of an error
 */
list_elem * init_sequential(long int size){
	long int i;
	list_elem *wsetptr;

	wsetptr = (list_elem *) malloc(size + sizeof(list_elem));
	if( wsetptr == NULL )
		return NULL;
	/* initialize the linear pointer chain */
	long num_elem = size / sizeof(list_elem);

	for( i = 0; i < num_elem - 1; i++ )
		wsetptr[i].next = &wsetptr[i+1];
	wsetptr[i].next = &wsetptr[0]; // last element points to the first one
	
	return wsetptr;
}

/**
 * Allocate an array of 'list_elem'ents with at least size size Byte.
 * The list elements are connected in an inverse sequential round robing way.
 * @return pointer to the begin of array, NULL in case of an error
 */
list_elem * init_inverse_sequential(long int size){
	long int i;
	list_elem *wsetptr;

	wsetptr = (list_elem *) malloc(size + sizeof(list_elem));
	if( wsetptr == NULL )
		return NULL;
	/* initialize the linear pointer chain */
	for( i = 1; i < size / sizeof(struct l); i++ )
		wsetptr[i].next = &wsetptr[i-1];
	wsetptr[0].next = &wsetptr[i-1]; // first element points to the last one
	
	return wsetptr;
}

/**
 * Allocate an array of 'list_elem'ents with at least size size Byte.
 * The list elements are connected in a random round robing way where
 * only elements with index multiple of stride are connected.
 * @return pointer to the begin of array, NULL in case of an error
 */
list_elem * init_random(long int size){
	long int i;
	list_elem *wsetptr;

	wsetptr = (list_elem *) malloc( size + sizeof(list_elem ) );
	if( wsetptr == NULL )
		return NULL;

	long num_elements = size/sizeof(list_elem);
	/* Use pointer array to generate a mapper list */
	for( i = 0; i < num_elements; i++ )
		wsetptr[i].next = (void *) i;
	/* Use Fisher–Yates shuffle algorithm to randomize mapping but consider only every 'stride' element
	 * starting with element 0.
	 * e.g. stride = 4:
	 * 0 1 2 3 4 5 6 7 8 9 10 11 12 13
	 * 0       4       8         12
	 * 8       12      4         0
	 */
	for( i = ((num_elements - 1) / wset_stride) ; i > 0; i-- ) {
		long j =  (random() % (i + 1));
		long ii = wset_stride * i;
		long jj = wset_stride * j;
		long tmp = (long) wsetptr[ii].next;
		wsetptr[ii].next = (void *) wsetptr[jj].next;
		wsetptr[jj].next = (void *) tmp;
	}
	for( i = 0; i < num_elements; i+= wset_stride ) {
		long id = (long) wsetptr[i].next;
		wsetptr[i].next = &wsetptr[id];
	}

	return wsetptr;
}

long int test_read(long int size, list_elem *wsetptr) {
	long int access_num;
	long int num_accesses = NUM_ACCESS_FACTOR * wset_final_size / sizeof( list_elem * );
	double start, stop;
	list_elem *lptr;
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
	double etime = stop - start;

#ifdef PAPI
	fprintf( logfile, "%12.ld %10.6lf %16.2lf %8.1lf", size, etime, num_accesses / etime, (double)(ticks2 - ticks1) / num_accesses );
	int ii;
	for( ii = 0; ii < num_hwcntrs; ii++) {
		fprintf(logfile, "\t%lld", values1[ii] );
	}
	fprintf( logfile, "\n" );
#else
	fprintf( logfile, "%12.ld %10.6lf %16.2lf %8.1lf\n", size, etime, num_accesses / etime, (double)(ticks2 - ticks1) / num_accesses );
#endif
	fflush(logfile);
      
	return (long) lptr;
}

void result_head(){
    fprintf(logfile,"# %10s %10s %16s %8s\n", "size", "etime", "access/sec", "ticks/access");
}

int main( int argc, char* argv[] ){

	int i;
	long size;
	list_elem *wsetptr;
	long result = 0;
	char logfilename[256];
	snprintf(logfilename, 255, "%s-pad%d.log", argv[0], NPAD);
	logfile = fopen(logfilename, "w+");

	typedef list_elem* (*init_fct_ptr)(long);
	typedef struct {
		init_fct_ptr function;
		char *name;
		int execute;
	} init_fct_spec;

	init_fct_spec init_functions[] = {
		{init_sequential, "sequential", 1},
		{init_inverse_sequential, "inverse-sequential", 1},
		{init_random, "random", 1}
	};

	const char optstring[] = "hm:M:p:s:";

	char opt;
	char pattern[1024];
	char *ptr;
	char delimiter[] = ",";

	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch(opt) {
			case 'm':
				wset_start_size = atol(optarg);
				break;
			case 'M':
				wset_final_size = atol(optarg);
				break;
			case 'p':
				for(i = 0; i < sizeof(init_functions)/sizeof(init_functions[0]); i++) {
					init_functions[i].execute = 0;
				}
				strcpy(pattern, optarg);
				ptr = strtok(pattern, delimiter);
				while(ptr != NULL) {
					if(strcmp(ptr, "all") == 0) {
						for(i = 0; i < sizeof(init_functions)/sizeof(init_functions[0]); i++) {
							init_functions[i].execute = 1;
						}
					}
					else {
						for(i = 0; i < sizeof(init_functions)/sizeof(init_functions[0]); i++) {
							if(strcmp(ptr, init_functions[i].name) == 0) {
								init_functions[i].execute = 1;
							}
						}
					}
					ptr = strtok(NULL, delimiter);
				}
				break;
			case 's':
				wset_stride = atol(optarg);
				break;
			case 'h':
			default:
				fprintf(stderr, "Usage: %s [-m min] [-M max] [-p pattern] [-s stride]\n", argv[0]);
				fprintf(stderr, "Available memory traversal patterns:\n");
				for(i = 0; i < sizeof(init_functions)/sizeof(init_functions[0]); i++) {
					fprintf(stderr, "* %s\n", init_functions[i].name);
				}
				exit(1);
				break;
		}
	}

	if(wset_start_size < wset_stride) {
		fprintf(stderr, "ERROR: Stride has to be larger than the minumum size. (stride=%ld, min_size=%ld)\n", wset_stride, wset_start_size);
		exit(1);
	}

#ifdef PAPI
	int retval;
	retval = PAPI_library_init(PAPI_VER_CURRENT);
	if (retval != PAPI_VER_CURRENT) {
		fprintf(stderr,"PAPI library init error!\n");
		papi_error_handler(retval);
	}	
	retval = PAPI_start_counters(Events, num_hwcntrs);
	if (retval < PAPI_OK) {
		fprintf(stderr,"PAPI event initialization failed\n");
		papi_error_handler(retval);
	}
	int j;
	for( j = 0; j < num_hwcntrs; j++ ) {
		fprintf(logfile,"\t%s", EventStrings[j]);
	}
#endif /* PAPI */

	fprintf(logfile, "# ------------------------------\n" );
	fprintf(logfile, "# Cache-Analysis\n");
	fprintf(logfile, "# Logfilename:    %s\n", logfilename);
	fprintf(logfile, "# Access padding: %ld Bytes\n", NPAD * sizeof(char) );
	fprintf(logfile, "# Struct size:    %ld Bytes\n", sizeof(list_elem));
	fprintf(logfile, "# wset_start_size:    %ld Bytes\n", wset_start_size);
	fprintf(logfile, "# wset_final_size:    %ld Bytes\n", wset_final_size);
	fprintf(logfile, "# wset_stride:    %ld elements\n", wset_stride);
	fprintf(logfile, "# # accesses:     %ld\n", NUM_ACCESS_FACTOR * wset_final_size / sizeof( list_elem * ));
	fprintf(logfile, "# ------------------------------\n\n" );
	fflush (logfile);

	for(i = 0; i < sizeof(init_functions)/sizeof(init_functions[0]); i++) {
		if(init_functions[i].execute == 0) {
			continue;
		}
		time_t starttime = time(NULL); /* calendar time */
		fprintf( logfile, "# Starttime: %s", asctime( localtime(&starttime) ) );
		fprintf( logfile, "# %s\n", init_functions[i].name );
		result_head();
		for( size = wset_start_size; size <= wset_final_size; ) {
			wsetptr = init_functions[i].function( size );
			result += test_read( size, wsetptr );
			free( wsetptr );
			if(size < SMALL_ARRAY_LIMIT) {
				size += sizeof(list_elem);
			}
			else {
				size *= factor;
			}
			//size = (size + sizeof(list_elem) > size * factor) ? size + sizeof(list_elem) : size * factor;
		}
		fprintf( logfile, "# Result: %ld\n", result );
		time_t endtime = time(NULL); /* calendar time */
		fprintf( logfile, "# Endtime: %s", asctime( localtime(&endtime) ) );
		fprintf( logfile, "# Duration: %lf sec\n\n\n", difftime(endtime, starttime) );
	}

#ifdef PAPI
	long long dummy[num_hwcntrs];
	retval = PAPI_stop_counters(dummy, num_hwcntrs);
	if (retval < PAPI_OK) {
		fprintf(stderr,"PAPI event initialization failed\n");
		papi_error_handler(retval);
	}
#endif

	return 0;
}
