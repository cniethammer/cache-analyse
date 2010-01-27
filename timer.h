#ifndef TIMER_H
#define TIMER_H

#include "sys/time.h"

inline double timer(){
  struct timeval timer;
  gettimeofday(&timer, NULL);
  return (double) timer.tv_sec + (double) timer.tv_usec / 1.0e6;
}

#endif
