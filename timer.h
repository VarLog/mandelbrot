// simple Timer class, Jan 2010, thomas{AT}thomasfischer{DOT}biz
// tested under windows and linux
// license: do whatever you want to do with it ;)
#ifndef TIMER_H__
#define TIMER_H__

// boost timer is awful, measures cpu time on linux only...
// thus we have to hack together some cross platform timer :(

#ifndef WIN32
#include <sys/time.h>
#else
#include <windows.h>
#endif

#ifndef NULL
#define NULL (void *)0
#endif

class Timer
{
protected:
#ifdef WIN32
    LARGE_INTEGER start;
#else
    struct timeval start;
#endif

public:
    Timer();
    double elapsed() const;
    void restart();
};

#endif //TIMER_H__

