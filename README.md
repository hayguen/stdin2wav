# stdin2wav
tee for audio streams: pass stream from stdin, e.g. rtl_fm, to stdout and save into .wav file(s)

it's a quick hack, i'm using in combination with rtl_fm (of librtlsdr) and it's squelch option.
squelch: when signal level is below the threshold, rtl_fm does stop streaming to stdout. ideally the transmission has ended.
here comes stdin2wav, which realizes the stopped stream (after the timeout) and closes the .wav file.

usage: stdin2wav [ <srate> [<nchan> [ <limit> [<stop|cont> [<timeout> [<filename>] ] ] ] ] ]

  writes stdin to stdout and to different files.
  filename: where to save each stream
    might contain strftime format specifier
    using default: '%Y%m%d_%H%M%S_%Z.wav'
  srate:   sample rate of stream. default: 24000
  nchan:   number of channels in stream. default: 1
  limit:   limit file length in seconds. default: 0
  stop/cont: after limit: continue or stop recording. default: 'cont'
  timeout: timeout in sec to close file. default: 1

for compilation just hit 'make'.
besides gcc and make, libsndfile is required.
on debian try 'sudo apt-get install libsndfile-dev'

Hayati Ayguen <h_ayguen@web.de>
