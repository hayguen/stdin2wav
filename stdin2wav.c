
/* (c)2015 Hayati Ayguen <h_ayguen@web.de>
 * License: MIT, see LICENSE file
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sndfile.h>

void usage( const char * argv0, const char * pathname, int srate, int nchan, int limitlen, int timeout )
{
  fprintf( stderr, "usage: %s [ <srate> [<nchan> [ <limit> [<stop|cont> [<timeout> [<filename>] ] ] ] ] ]\n", argv0 );
  fprintf( stderr, "  writes stdin to stdout and to different files.\n" );
  fprintf( stderr, "  filename: where to save each stream\n" );
  fprintf( stderr, "    might contain strftime format specifier\n" );
  fprintf( stderr, "    using default: '%s'\n", pathname );
  fprintf( stderr, "  srate:   sample rate of stream. default: %d\n", srate );
  fprintf( stderr, "  nchan:   number of channels in stream. default: %d\n", nchan );
  fprintf( stderr, "  limit:   limit file length in seconds. default: %d\n", limitlen );
  fprintf( stderr, "  stop/cont: after limit: continue or stop recording. default: 'cont'\n" );
  fprintf( stderr, "  timeout: timeout in sec to close file. default: %d\n", timeout );
}

volatile int endprog = 0;

void sigint_handler(int sig)
{
  endprog = 1;
}


int main( int argc, char * argv[] )
{
  const char * pathname = "%Y%m%d_%H%M%S_%Z.wav";
  int optidx = 1;
  int srate = 24000;
  int nchan = 1;
  int limitlen = 0;
  int stop_after_limit = 0;
  int timeout = 1;
  if ( optidx < argc )
  {
    int sr = atoi( argv[optidx] );
    if ( sr )
      srate = sr;
    else
      fprintf( stderr, "option srate must be number > 0! option is '%s'\n", argv[optidx] );
  }
  else
  {
    usage( argv[0], pathname, srate, nchan, limitlen, timeout );
    return 0;
  }

  ++optidx;
  if ( optidx < argc )
  {
    int nc = atoi( argv[optidx] );
    if ( nc >= 1 && nc <= 2 )
      nchan = nc;
    else
      fprintf( stderr, "option nchan must be 1 or 2 ! option is '%s'\n", argv[optidx] );
  }

  ++optidx;
  if ( optidx < argc )
  {
    limitlen = atoi( argv[optidx] );
  }

  ++optidx;
  if ( optidx < argc )
  {
    stop_after_limit = ( !strcmp(argv[optidx], "stop" ) ) ? 1 : 0;
    int cont_after_limit = ( !strcmp(argv[optidx], "cont" ) ) ? 1 : 0;
    if ( !stop_after_limit && !cont_after_limit )
      --optidx;
  }

  ++optidx;
  if ( optidx < argc )
  {
    int t = atoi( argv[optidx] );
    if ( t >= 1 )
      timeout = t;
    else
      fprintf( stderr, "option timeout must be >= 1! option is '%s'\n", argv[optidx] );
  }

  ++optidx;
  if ( optidx < argc )
  {
    pathname = argv[optidx];
    if ( !strcmp(argv[optidx], "-") )
      pathname = 0;
  }

  // get default flags for stdin
  const int flag_dfl = fcntl(0, F_GETFL);
  if ( flag_dfl == -1 )
  {
    perror("error getting flags");
    exit(EXIT_FAILURE);
  }

  if ( isatty(0) )
  {
    struct termios term, term_orig;
    if ( tcgetattr(0, &term_orig) )
    {
      fprintf( stderr, "tcgetattr failed: %d '%s'\n", errno, strerror(errno) );
      exit(-1);
    }
    term = term_orig;
    term.c_lflag &= ~ICANON;
    //term.c_lflag |= ECHO;
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &term))
    {
      fprintf( stderr, "tcsetattr failed: %d '%s'\n", errno, strerror(errno) );
      exit(-1);
    }
  }

  size_t rcvbufsize = srate * 2 * 2;
  char * rcvbuf = (char*)malloc( rcvbufsize * sizeof(char) );
  char fnbuf_temp[1024];
  char fnbuf_final[1024];

  // install handler for catching Ctrl-C
  signal(SIGINT, sigint_handler);

  // main loop
  int emissionEnd = 0;
  SNDFILE * f = 0;
  ssize_t buffered = 0;
  ssize_t bytesPerFrame = 2 /* 16 Bit */ * nchan;
  ssize_t frameMask = bytesPerFrame -1;
  const int limitFrames = ( limitlen > 0 ) ? ( limitlen * srate ) : 0;
  int numFramesInFile = 0;

  while ( !endprog )
  {
    fd_set rfds;
    struct timeval tv;
    int retval;

    // back to blocking i/o for stdin
    int flag = fcntl(0, F_SETFL, flag_dfl);
    if ( flag == -1 )
    {
      perror("error getting flags");
      break;
    }

    // wait for input on stdin
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = timeout;      // 1 sec timeout
    tv.tv_usec = 0;
    retval = select( 1, &rfds, NULL, NULL, &tv );
    if ( retval == -1 )
    {
      perror("select()");
    }
    else if ( retval )  // stdin has something new
    {
      // set non-blocking i/o for stdin
      //fprintf(stderr, "x\n");
      flag = fcntl(0, F_SETFL, flag_dfl | O_NONBLOCK);
      if ( flag == -1 )
      {
        perror("error getting flags");
        break;
      }
      ssize_t sread = 0;
      do
      {
        sread = read( 0, rcvbuf+buffered, rcvbufsize-buffered );
        if ( sread == 0 )
        {
          fprintf( stderr, "EOF\n" );
          endprog = 1;
          break;
        }
        else if ( sread < 0 )
        {
          if ( errno == 11 )
            break;
          fprintf( stderr, "error on read(): %d '%s'\n", errno, strerror(errno) );
          endprog = 1;
          break;
        }

        if ( limitFrames && stop_after_limit && numFramesInFile >= limitFrames )
          continue;   // just skip recording when stop_after_limit and file limit is reached

        //fprintf( stderr, "read %d chars\n", (int)sread );
        write( 1, rcvbuf, sread );
        if ( !f && pathname )
        {
          time_t rawtime;
          struct tm * timeinfo;
          time(&rawtime);
          timeinfo = localtime(&rawtime);
          strftime( fnbuf_final,sizeof(fnbuf_final),pathname, timeinfo );
          snprintf( fnbuf_temp, sizeof(fnbuf_temp), "%s%s", fnbuf_final, ".tmp" );
          SF_INFO sfi;
          sfi.frames = 0;
          sfi.samplerate = srate;
          sfi.channels = nchan;
          sfi.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
          sfi.sections = 0;
          sfi.seekable = 0;
          f = sf_open(fnbuf_temp, SFM_WRITE, &sfi);
          fprintf( stderr, "%s: opened '%s' %s for write of %d bytes\n"
                 , argv[0], fnbuf_temp, (f ? "successfully" : "error"), (int)sread );
          numFramesInFile = 0;
        }
        if ( f && sread )
        {
          //sf_write_raw( f, buf, sread );
          void * vp = &rcvbuf[0];
          sf_count_t nframes = (sread+buffered) / bytesPerFrame;
          sf_writef_short( f, (short*)vp, nframes );
          numFramesInFile += (int)nframes;
#if 0
          fprintf(stderr, "%s: wrote %d frames of %d read bytes, buffered %d bytes\n"
                  , argv[0], (int)nframes, (int)sread, (int)buffered );
#endif
          ssize_t firstNonSent = nframes * bytesPerFrame;
          for ( ssize_t i = firstNonSent; i < sread+buffered; ++i )
            rcvbuf[i - firstNonSent] = rcvbuf[i];
          buffered = (sread+buffered) & frameMask;
        }
      }
      while ( sread > 0 );
    }
    else
    {
      emissionEnd = 1;
    }

    if ( endprog || emissionEnd || ( limitFrames && numFramesInFile >= limitFrames ) )
    {
      if (f)
      {
        sf_close( f );
        fprintf( stderr, "%s: closed file for write\n", argv[0] );
        int r = rename( fnbuf_temp, fnbuf_final );
        if ( r )
          fprintf( stderr, "%s: error %d '%s' renaming'%s' to '%s'\n"
                  , argv[0], errno, strerror(errno), fnbuf_temp, fnbuf_final );
        else
          fprintf( stderr, "%s: renamed '%s' to '%s'\n"
                  , argv[0], fnbuf_temp, fnbuf_final );
      }
      f = 0;
      buffered = 0;
      if ( emissionEnd && numFramesInFile ) // keep numFramesInFile >= limitFrames until emission end
      {
        fprintf( stderr, "%s: end of emission\n", argv[0] );
        numFramesInFile = 0;
      }
      emissionEnd = 0;
    }
  }

  free( rcvbuf );
  return 0;
}

