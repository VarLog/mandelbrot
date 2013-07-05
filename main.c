// This code computing Mandelbrot set in real time on INPE MEPhi cluster

#include <stdio.h>
#include <math.h>
#include <mpi.h>

#include <GL/freeglut.h>   // Header File For The GLUT Library 
#include <GL/gl.h>         // Header File For The OpenGL32 Library
#include <GL/glu.h>        // Header File For The GLu32 Library
#include <GL/glx.h>        // Header file fot the glx libraries.
#include <unistd.h>        // needed to sleep

#include "timer.h"
#include "log.h"

const int windowWidth = 800;
const int windowHeight = 600;

int InitMPI_AndIdentifyRank( int argc, char **argv )
{

  int size, rank;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  int namelen;
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  MPI_Get_processor_name(processor_name, &namelen);
  
  printf("Process %d on %s out of %d\n", rank, processor_name, size);
  
  return(rank);
}

int ShutdownMPI(void)
{
  int namelen;
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  MPI_Get_processor_name(processor_name, &namelen);
  
  printf("MPI_Finalize called on %s.\n", processor_name);
  
  MPI_Finalize();
  return 0;
}

void computeBlock( unsigned char *dataBuf, int rankOffset, int stride, int numRows )
{
  static float angle = 0.0f;
  const float angleCos = cosf( angle );
  const float angleSin = sinf( angle );

//#pragma omp parallel for schedule(dynamic,1)
  for ( int i = 0;  i < windowWidth;  i++ )
  {
    for ( int j = 0;  j < numRows;  j++ )
    {
      // calc the complex plane coordinates for this point

      // project the screen coordinate into the complex plane
      const float rCr = -2.0f + 3.0f * i / windowWidth;
      const float rCi = -1.0f + 2.0f * (j*stride + rankOffset) / windowHeight;

      // and rotate
      const float rotationCenterX = -0.5f;
      const float Cr = rotationCenterX + angleCos * (rCr - rotationCenterX) - angleSin * rCi;
      const float Ci = angleCos * rCi + angleSin * (rCr - rotationCenterX);

      float Zr = 0.0f;
      float Zi = 0.0f;
      
      // and run the Mandelbrot function
      int iter = 0;
      while ( iter < 19 )
      {
        float Zr_new = Zr * Zr - Zi * Zi + Cr;
        Zi = 2 * Zr * Zi + Ci;
        Zr = Zr_new;
        
        if ( ( Zi * Zi + Zr * Zr ) > 4.0f )
        {
          break;
        }

        ++iter;
      }
      
      dataBuf[ i + j * windowWidth ] = 19 - iter;
    }
  }

  // rotate slowly
  angle += 0.001f;
}

double times[64];
double times_comp[64];

int masterTick( int message, unsigned char *dataBuf )
{
  int buf[1024];
  int bufSize = 1024;
  
  int size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  buf[0] = message;

  static Timer timer;

  times[0] = timer.elapsed();

  int curWorker = 0;
  for ( int i = 0;  i < size;  i++ )
  {
    if ( i == rank )
    {
      continue;
    }

    times[2+curWorker] = timer.elapsed();
    MPI_Send( buf, bufSize, MPI_INT, i, 0, MPI_COMM_WORLD );
    ++curWorker;
  }

  if ( message >= 0 )
  {
    // handle the 0 block while the slaves are working on theirs
    times[1] = timer.elapsed();
    computeBlock( dataBuf, 0, 1, windowHeight / size ); // uneven compute load
//    computeBlock( dataBuf, rank, size, windowHeight / size ); // evenly distributed compute load
    times[1] = timer.elapsed() - times[1];
  
    times_comp[0] = times[1];

    // kick off non-blocking receives
    int dataBufSize = windowWidth*windowHeight/size;
    MPI_Request reqs[64], reqs_time[64];
    int curReq = 0;
    for ( int i = 0;  i < size;  i++ )
    {
      if ( i == rank )
      {
        continue;
      }
      
      MPI_Irecv( dataBuf + i * dataBufSize, dataBufSize, MPI_CHAR, i, 0, MPI_COMM_WORLD, &reqs[curReq] );

      // recv compulation time from slaves
      MPI_Irecv( &times_comp[i], 1, MPI_DOUBLE, i, 1, MPI_COMM_WORLD, &reqs_time[curReq] );

      ++curReq;
    }

    // wait for all the receives
    MPI_Status ress[64];
    for ( int i = 0;  i < size-1;  i++ )
    {
      int index;
      MPI_Waitany( size-1, reqs, &index, ress );
      times[2+index] = timer.elapsed() - times[2+index];

      MPI_Waitany( size-1, reqs_time, &index, ress );
    }

    times[0] = timer.elapsed() - times[0];
  }
  
  return 0;
}


int workerTick()
{
  int buf[1024];
  int bufSize = 1024;
  MPI_Status res;
  MPI_Recv( buf, bufSize, MPI_INT, 0, 0, MPI_COMM_WORLD, &res );
  if ( buf[0] == -1 )
  {
    return -1;
  }
  
  int size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  unsigned char dataBuf[windowWidth*windowHeight];
  int dataBufSize = windowWidth*windowHeight/size;
  
  int rankOffset = rank * windowHeight / size;

  Timer t;
  double dt;
  // measure compulation time
  dt = t.elapsed();
  
  computeBlock( dataBuf, rankOffset, 1, windowHeight / size ); // uneven compute load
//  computeBlock( dataBuf, rank, size, windowHeight / size ); // evenly distributed compute load
  
  dt = t.elapsed() - dt;
  MPI_Send( dataBuf, dataBufSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD );
  
  // send compulation time
  MPI_Send( &dt, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD );
  
  return 0;
}

int workerLoop()
{
  int res = -1;
  do
  {
    res = workerTick();
  } while ( res >= 0 );
  
  return 0;
}

/* ASCII code for the escape key. */
#define ESCAPE 27

/* The number of our GLUT window */
int window; 

/* rotation angle for the triangle. */
float rtri = 0.0f;

/* rotation angle for the quadrilateral. */
float rquad = 0.0f;


GLuint base;            // base display list for the font set.
GLfloat cnt1;           // 1st counter used to move text & for coloring.
GLfloat cnt2;           // 2nd counter used to move text & for coloring.


void BuildFont(void) 
{
    Display *dpy;
    XFontStruct *fontInfo;  // storage for our font.

    base = glGenLists(96);                      // storage for 96 characters.
    
    // load the font.  what fonts any of you have is going
    // to be system dependent, but on my system they are
    // in /usr/X11R6/lib/X11/fonts/*, with fonts.alias and
    // fonts.dir explaining what fonts the .pcf.gz files
    // are.  in any case, one of these 2 fonts should be
    // on your system...or you won't see any text.
    
    // get the current display.  This opens a second
    // connection to the display in the DISPLAY environment
    // value, and will be around only long enough to load 
    // the font. 
    dpy = XOpenDisplay(NULL); // default to DISPLAY env.   

    fontInfo = XLoadQueryFont(dpy, "-adobe-helvetica-medium-r-normal--20-*-*-*-p-*-iso8859-1");
    if (fontInfo == NULL)
    {
    fontInfo = XLoadQueryFont(dpy, "fixed");
    if (fontInfo == NULL)
    {
        printf("no X font available?\n");
    }
    }

    // after loading this font info, this would probably be the time
    // to rotate, scale, or otherwise twink your fonts.  

    // start at character 32 (space), get 96 characters (a few characters past z), and
    // store them starting at base.
    glXUseXFont(fontInfo->fid, 32, 96, base);

    // free that font's info now that we've got the 
    // display lists.
    XFreeFont(dpy, fontInfo);

    // close down the 2nd display connection.
    XCloseDisplay(dpy);
}

void KillFont(void)                         // delete the font.
{
    glDeleteLists(base, 96);                    // delete all 96 characters.
}

void glPrint(const char *text)                      // custom gl print routine.
{
    if (text == NULL) {                         // if there's no text, do nothing.
    return;
    }
    
    glPushAttrib(GL_LIST_BIT);                  // alert that we're about to offset the display lists with glListBase
    glListBase(base - 32);                      // sets the base character to 32.

    glCallLists(strlen(text), GL_UNSIGNED_BYTE, text); // draws the display list text.
    glPopAttrib();                              // undoes the glPushAttrib(GL_LIST_BIT);
}



/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)          // We call this right after our OpenGL window is created.
{
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);    // This Will Clear The Background Color To Black
  glClearDepth(1.0);        // Enables Clearing Of The Depth Buffer
  glDepthFunc(GL_LESS);              // The Type Of Depth Test To Do
  glEnable(GL_DEPTH_TEST);            // Enables Depth Testing
  glShadeModel(GL_SMOOTH);      // Enables Smooth Color Shading
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();        // Reset The Projection Matrix
  
  gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);  // Calculate The Aspect Ratio Of The Window
  
  glMatrixMode(GL_MODELVIEW);

  BuildFont();  
}

/* The function called when our window is resized (which shouldn't happen, because we're fullscreen) */
void ReSizeGLScene(int Width, int Height)
{
  if (Height==0)        // Prevent A Divide By Zero If The Window Is Too Small
    Height=1;
  
  glViewport(0, 0, Width, Height);    // Reset The Current Viewport And Perspective Transformation
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  
  gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);
  glMatrixMode(GL_MODELVIEW);
}

unsigned char dataBuf[windowWidth*windowHeight];

/* The main drawing function. */
void DrawGLScene()
{
  static Timer timer;

  masterTick(0, dataBuf);

  double drawTime = timer.elapsed();
  
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);  // Clear The Screen And The Depth Buffer
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-0.5, (windowWidth - 1) + 0.5, (windowHeight - 1) + 0.5, -0.5, -1.0, 1.0);

  glPointSize(1.0f);
  glDisable(GL_POINT_SMOOTH);

  glBegin(GL_POINTS);
  glColor3f(0.0f, 0.0f, 0.0f);

  double pixelComputeTime = timer.elapsed();
  unsigned short frameBuffer[ windowWidth * windowHeight];

  unsigned short *frameBufferPtr = frameBuffer;
  const unsigned char *dataBufPtr = dataBuf;
  for ( int j = 0;  j < windowHeight;  j++ )
  {
    for ( int i = 0;  i < windowWidth;  i++ )
    {
      const unsigned char mandelIter = *dataBufPtr;
      *frameBufferPtr = ( mandelIter << 11 ) | ( mandelIter << 6 ) | mandelIter;
      ++dataBufPtr;
      ++frameBufferPtr;
    }
  }

  pixelComputeTime = timer.elapsed() - pixelComputeTime;
  glEnd();

  glRasterPos2f(0.0f, windowHeight-1.0f);
  glDrawPixels( windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frameBuffer );
  
  // DISABLE DEPTH TEST!
  glDisable( GL_DEPTH_TEST );

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  /*
  // find the min/max time
  float minTime = 1000000000.0f;
  float maxTime = 0.0f;
  for ( int i = 1;  i <= 8;  i++ )
  {
    if ( times[i] > maxTime)
    {
      maxTime = times[i];
    }
    if ( times[i] < minTime)
    {
      minTime = times[i];
    }
  }
  */
    // 320 pixels (middle of the screen) will be 16 milliseconds
    // so 1 millisecond is 20 pixels
    // which means 20,000 multiplier

  // worker thread times overlayed as a multi-colored translucent histogram
 /* 
  for ( int i = 1;  i <= 8;  i++ )
  {
    glColor4f((times[i]-minTime)/(maxTime-minTime), 1.0f-(times[i]-minTime)/(maxTime-minTime), 0.0f, 0.3f);
    glVertex3f(0.0f, (i-1) * windowHeight/8, 0.4f);
    glVertex3f(times[i] * 20000.0f, (i-1) * windowHeight/8, 0.4f);
    glVertex3f(times[i] * 20000.0f, (i) * windowHeight/8, 0.4f);
    glVertex3f(0.0f, (i) * windowHeight/8, 0.4f);
  }
  glEnd();
  */
  
  for ( int i = 0;  i < 8;  i++ )
  {
    glColor4f(1.0f, 0.0f, 0.0f, 0.3f);
    glVertex3f(0.0f, (i) * windowHeight/8, 0.4f);
    glVertex3f(times[i+1] * 20000.0f, (i) * windowHeight/8, 0.4f);
    glVertex3f(times[i+1] * 20000.0f, (i+1) * windowHeight/8, 0.4f);
    glVertex3f(0.0f, (i+1) * windowHeight/8, 0.4f);
  }

  for ( int i = 0;  i < 8;  i++ )
  {
    glColor4f(0.0f, 1.0f, 0.0f, 0.3f);
    glVertex3f(0.0f, (i) * windowHeight/8, 0.4f);
    glVertex3f((times[i+1] - times_comp[i]) * 20000.0f, (i) * windowHeight/8, 0.4f);
    glVertex3f((times[i+1] - times_comp[i]) * 20000.0f, (i+1) * windowHeight/8, 0.4f);
    glVertex3f(0.0f, (i+1) * windowHeight/8, 0.4f);
  }
  glEnd();
  
  // print times into log file
  for( int i = 0;  i < 8;  i++ ) {
    char tag[7] = "times";
    tag[5] = '0' + i;
    tag[6] = '\0';
    Log::out( tag, "%17.12f %17.12f", times[i+1], ( times[i+1] - times_comp[i] ) );
  }
  
  // diff between times and times_comp
  /*
  for ( int i = 0;  i < 8;  i++ ) {
    printf( " %f |", times[i+1] - times_comp[i] );
  }
  printf( "\r" );
  fflush(stdout);
  */

  glBegin(GL_LINES);

    // draw the 16ms line in white
  glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(0.016f * 20000.0f, windowHeight-50.0f);
    glVertex2f(0.016f * 20000.0f, windowHeight);

  // draw the total masterTick time in yellow
  glColor3f(1.0f, 1.0f, 0.0f);
    glVertex2f(times[0] * 20000.0f, windowHeight-50.0f);
    glVertex2f(times[0] * 20000.0f, windowHeight);

  // pixel data generation time in magenta
  glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
    glVertex2f((times[0]+pixelComputeTime) * 20000.0f, windowHeight-50.0f);
    glVertex2f((times[0]+pixelComputeTime) * 20000.0f, windowHeight);

  // total draw time in green
  drawTime = timer.elapsed() - drawTime;
  glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f((times[0]+drawTime) * 20000.0f, windowHeight-50.0f);
    glVertex2f((times[0]+drawTime) * 20000.0f, windowHeight);

  glEnd();

  // Position The Text On The Screen
  glColor3f(1.0f, 1.0f, 0.0f);
  glRasterPos2f(0.0f, 20.0f);

  char str[1024];
    sprintf( str, "Time elapsed: %8.2lf seconds\n", timer.elapsed());
    glPrint(str); // print gl text to the screen.
  
  glRasterPos2f(0.0f, 40.0f);
  static int framesRendered = 0;
    sprintf( str, "Average FPS: %8.2lf\n", ++framesRendered / timer.elapsed());
    glPrint(str); // print gl text to the screen.

  // swap the buffers to display, since double buffering is used.
  glutSwapBuffers();
}

/* The function called whenever a key is pressed. */
void keyPressed(unsigned char key, int x, int y) 
{
  /* avoid thrashing this call */
  usleep(10);
  
  /* If escape is pressed, kill everything. */
  if (key == ESCAPE) 
  { 
    /* shut down our window */
    glutDestroyWindow(window); 
    glutLeaveMainLoop();
  }
}

int main(int argc, char **argv) 
{  
  if ( InitMPI_AndIdentifyRank( argc, argv ) == 0 )
  {
    /* Initialize GLUT state - glut will take any command line arguments that pertain to it or 
    X Windows - look at its documentation at http://reality.sgi.com/mjk/spec3/spec3.html */  
    glutInit(&argc, argv);  
    
    /* Select type of Display mode:   
    Double buffer 
    RGBA color
    Alpha components supported 
    Depth buffered for automatic clipping */  
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);  
    
    /* get a 640 x 480 window */
    glutInitWindowSize(windowWidth, windowHeight);  
    
    /* the window starts at the upper left corner of the screen */
    glutInitWindowPosition(0, 0);  
    
    /* Open a window */  
    window = glutCreateWindow("Mandelbrot set computed in real time on INPE MEPhI cluster");  
    
    /* Register the function to do all our OpenGL drawing. */
    glutDisplayFunc(&DrawGLScene);  
    
    /* Go fullscreen.  This is as soon as possible. */
    //  glutFullScreen();
    
    /* Even if there are no events, redraw our gl scene. */
    glutIdleFunc(&DrawGLScene);
    
    /* Register the function called when our window is resized. */
    glutReshapeFunc(&ReSizeGLScene);
    
    /* Register the function called when the keyboard is pressed. */
    glutKeyboardFunc(&keyPressed);
    
    /* Initialize our window. */
    InitGL(windowWidth, windowHeight);

    /* This will make sure that GLUT doesn't kill the program when the window is closed by the OS */
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,
                      GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    
    /* Start Event Processing Engine */  
    glutMainLoop();

    /* We fall through to here once ESC is pressed or the window closed by the OS */
    masterTick(-1, NULL);
  }
  else
  {
    workerLoop();
    // some worker machine specific code
    // NOTE: we'd need to remove MPI_Finalize call from the init function for this to be valid!
  }
  
  ShutdownMPI();
  
  return 0;
}
