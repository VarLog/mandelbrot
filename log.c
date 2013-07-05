// simple log class

#include "log.h"

Timer Log::timer;
FILE *Log::f_log = NULL;
const char *Log::f_path = "./log.txt";
 
bool Log::open_file() {
  Log::f_log = fopen( Log::f_path, "w" );
  if( f_log ) {
    return true; 
  } 
  return false;
}

bool Log::out( const char *tag, const char *fmt, ... ) {
  if( ! f_log ) {
    Log::open_file();
  }

  fprintf( f_log, "%20.12f ", Log::timer.elapsed() );

  fprintf( f_log, "%s ", tag );

  va_list args;
  va_start( args, fmt );
  vfprintf( f_log, fmt, args );
  va_end( args );

  fprintf( f_log, "\n" );

  return true;
}


