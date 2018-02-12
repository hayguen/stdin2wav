
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

#define TESTLOGS  1

void usage( const char * argv0, const char * pathname, double srate, int nchan, double limitlen, int timeout )
{
  fprintf( stderr, "usage: %s [<format>] [no-stdout] [-v] [ <srate> [<nchan> [ <limit> [<stop|cont> [<timeout> [<filename>] ] ] ] ] ]\n", argv0 );
  fprintf( stderr, "  writes stdin to stdout and to different files.\n" );
  fprintf( stderr, "  filename: where to save each stream\n" );
  fprintf( stderr, "    might contain strftime format specifier\n" );
  fprintf( stderr, "    using default: '%s'\n", pathname );
  fprintf( stderr, "  format:  one of 'pcm16', 'pcm32', 'float',\n");
  fprintf( stderr, "             or 'flt2pcm8', 'flt2pcm16', 'flt2pcm24', 'flt2pcm32'\n");
  fprintf( stderr, "             while 'flt2..' variants are converting incoming float data to the .. format\n");
  fprintf( stderr, "  srate:   sample rate of stream. default: %.0f\n", srate );
  fprintf( stderr, "  nchan:   number of channels in stream. default: %d\n", nchan );
  fprintf( stderr, "  limit:   limit file length in seconds. default: %f\n", limitlen );
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
  double limitlen = 0;
  double srateDbl = 24000.0;
  int srateInt = 24000;
  int optidx = 1;
  int nchan = 1;
  int stop_after_limit = 0;
  int timeout = 1;
  int copyToStdout = 1;
  int verboseOutput = 0;
  int inpFormat = SF_FORMAT_PCM_16;
  int outFormat = SF_FORMAT_PCM_16;
  const ssize_t aFmtSize[ SF_FORMAT_FLOAT + 1 ] = { 0, 1, 2, 3, 4, 1, 4 };

  while ( optidx < argc )
  {
    if ( !strcmp(argv[optidx], "no-stdout") )
    {
      copyToStdout = 0;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "-v") )
    {
      ++verboseOutput;
      ++optidx;
    }

    else if ( !strcmp(argv[optidx], "pcm16") )
    {
      inpFormat = outFormat = SF_FORMAT_PCM_16;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "pcm16pcm8") )
    {
      inpFormat = SF_FORMAT_PCM_16;
      outFormat = SF_FORMAT_PCM_U8;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "pcm32") )
    {
      inpFormat = outFormat = SF_FORMAT_PCM_32;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "pcm32pcm24") )
    {
      inpFormat = SF_FORMAT_PCM_32;
      outFormat = SF_FORMAT_PCM_24;
      ++optidx;
    }

    else if ( !strcmp(argv[optidx], "float") )
    {
      inpFormat = outFormat = SF_FORMAT_FLOAT;
      ++optidx;
    }

    else if ( !strcmp(argv[optidx], "flt2pcm8") )
    {
      inpFormat = SF_FORMAT_FLOAT;
      outFormat = SF_FORMAT_PCM_U8;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "flt2pcm16") )
    {
      inpFormat = SF_FORMAT_FLOAT;
      outFormat = SF_FORMAT_PCM_16;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "flt2pcm24") )
    {
      inpFormat = SF_FORMAT_FLOAT;
      outFormat = SF_FORMAT_PCM_24;
      ++optidx;
    }
    else if ( !strcmp(argv[optidx], "flt2pcm32") )
    {
      inpFormat = SF_FORMAT_FLOAT;
      outFormat = SF_FORMAT_PCM_32;
      ++optidx;
    }
    else
      break;
  }

  if ( optidx < argc )
  {
    double sr = atof( argv[optidx] );
    if ( sr != 0.0 )
    {
      srateDbl = sr;
      srateInt = (int)( srateDbl + 0.5 );
    }
    else
      fprintf( stderr, "option srate must be number > 0! option is '%s'\n", argv[optidx] );
  }
  else
  {
    usage( argv[0], pathname, srateDbl, nchan, limitlen, timeout );
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
    limitlen = atof( argv[optidx] );
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

  const ssize_t inpBytesPerFrame = aFmtSize[ inpFormat ] * nchan;
  const ssize_t outBytesPerFrame = aFmtSize[ outFormat ] * nchan;
  const size_t rcvbufsize = srateInt * inpBytesPerFrame;
  char * rcvbuf = (char*)malloc( rcvbufsize * sizeof(char) );
  void * vp = &rcvbuf[0];
  char fnbuf_temp[1024];
  char fnbuf_final[1024];

  // install handler for catching Ctrl-C
  signal(SIGINT, sigint_handler);

  // main loop
  int emissionEnd = 0;
  SNDFILE * f = 0;
  ssize_t inpBuffered = 0;      // remaining bytes of a frame

  const int limitFrames = ( limitlen > 0 ) ? (int)( limitlen * srateDbl + 0.5 ) : 0;
  int numFramesInFile = 0;

  if ( verboseOutput )
    fprintf(stderr, "samplerate %f, %d channels, frameSize %d, limitFrames %d\n", srateDbl, nchan, (int)inpBytesPerFrame, limitFrames );

  while ( !endprog )
  {
    fd_set rfds;
    struct timeval tv;
    int retval;

#if TESTLOGS
    if ( verboseOutput >= 2 )
      fprintf(stderr, ";\n");
#endif

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
        sread = read( 0, rcvbuf+inpBuffered, rcvbufsize-inpBuffered );
        if ( sread == 0 )
        {
          fprintf( stderr, "EOF from stdin\n" );
          endprog = 1;
          break;
        }
        else if ( sread == -1 )
        {
          if ( errno == EAGAIN || errno == EWOULDBLOCK )
            continue;
          fprintf( stderr, "error on read() from stdin: %d '%s'\n", errno, strerror(errno) );
          endprog = 1;
          break;
        }

        if ( limitFrames && stop_after_limit && numFramesInFile >= limitFrames )
        {
#if TESTLOGS
          if ( verboseOutput >= 2 )
            fprintf(stderr, "-\n");
#endif
          continue;   // just skip recording when stop_after_limit and file limit is reached
        }

        if ( copyToStdout )
        {
          ssize_t ws = 0;
          while ( ws < sread )
          {
            ssize_t w = write( 1, rcvbuf+inpBuffered+ws, sread-ws );
            if ( w == -1 )
            {
              if ( errno == EAGAIN || errno == EWOULDBLOCK )
                continue;
              fprintf( stderr, "error on write() to stdout: %d '%s'\n", errno, strerror(errno) );
              endprog = 1;
              break;
            }
            ws += w;
          }
        }

        //fprintf( stderr, "read %d chars\n", (int)sread );

        // total num bytes read is inpBuffered
        inpBuffered += sread;
        const sf_count_t nframes = inpBuffered / inpBytesPerFrame;

#if TESTLOGS
        if ( verboseOutput )
          fprintf(stderr, "read %d, inpBuffered %d, nframes %d, numFramesInFile %d\n", (int)sread, (int)inpBuffered, (int)nframes, (int)numFramesInFile );
#endif

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
          sfi.samplerate = srateInt;
          sfi.channels = nchan;
          sfi.format = SF_FORMAT_WAV | outFormat;
          sfi.sections = 0;
          sfi.seekable = 0;
          f = sf_open(fnbuf_temp, SFM_WRITE, &sfi);
          fprintf( stderr, "%s: opened '%s' %s for write of %d frames\n"
                 , argv[0], fnbuf_temp, (f ? "successfully" : "error"), (int)nframes );
          numFramesInFile = 0;
        }
        if ( !nframes )
          break;

        if ( f )
        {
          switch (inpFormat)
          {
          case SF_FORMAT_PCM_16:
            sf_writef_short( f, (short*)vp, nframes );
            break;
          case SF_FORMAT_PCM_32:
            sf_writef_int( f, (int*)vp, nframes );
            break;
          case SF_FORMAT_FLOAT:
            sf_writef_float( f, (float*)vp, nframes );
            break;
          default:
            fprintf( stderr, "unknown input sample format!\n" );
            endprog = 1;
            break;
          }
          numFramesInFile += (int)nframes;
#if TESTLOGS
          if ( verboseOutput )
            fprintf(stderr, "%s: wrote %d frames inpBuffered %d bytes\n", argv[0], (int)nframes, (int)inpBuffered );
#endif
        }

        // move unprocessed input bytes to begin
        {
          ssize_t k = 0;
          const ssize_t firstNonSent = nframes * inpBytesPerFrame;
          for ( ssize_t i = firstNonSent; i < inpBuffered; ++i )
            rcvbuf[k++] = rcvbuf[i];
          inpBuffered = k;
        }

        if ( limitFrames && numFramesInFile >= limitFrames )
        {
#if TESTLOGS
          if ( verboseOutput >= 2 )
            fprintf(stderr, "-\n");
#endif
          if ( stop_after_limit )
            continue;   // just skip recording when stop_after_limit and file limit is reached
          else
            break;
        }

      }
      while ( !endprog );
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
      inpBuffered = 0;
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

