#ifndef LOG_H__
#define LOG_H__

#include <stdio.h>
#include <stdarg.h>

#include "timer.h"

// simple Log class
class Log {
  private:
    Log();

    static const char *f_path;
    static FILE *f_log;
    static Timer timer;

    static bool open_file();

  public:
    static bool out( const char *tag, const char *fmt, ... );
};

#endif //LOG_H__

