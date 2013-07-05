// simple Timer class, Jan 2010, thomas{AT}thomasfischer{DOT}biz
// tested under windows and linux
// license: do whatever you want to do with it ;)

#include "timer.h"

Timer::Timer() {
  restart();
}

double Timer::elapsed() const {
#ifdef WIN32
  LARGE_INTEGER tick, ticksPerSecond;
  QueryPerformanceFrequency(&ticksPerSecond);
  QueryPerformanceCounter(&tick);
  return ((double)tick.QuadPart - (double)start.QuadPart) / (double)ticksPerSecond.QuadPart;
#else
  struct timeval now;
  gettimeofday(&now, (struct timezone *)NULL);
  return (now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec)/1000000.0;
#endif
}

void Timer::restart() {
#ifdef WIN32
  QueryPerformanceCounter(&start);       
#else
  gettimeofday(&start, (struct timezone *)NULL);
#endif
}

