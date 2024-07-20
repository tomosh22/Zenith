
#include "zfstream.h"

gzfilebuf::gzfilebuf() :
  file(NULL),
  mode(0),
  own_file_descriptor(0)
{ }

gzfilebuf::~gzfilebuf() {

  sync();
  if ( own_file_descriptor )
    close();

}

gzfilebuf *gzfilebuf::open( const char *name,
                            int io_mode ) {

  if ( is_open() )
    return NULL;

  char char_mode[10];
  char *p = char_mode;

  if (io_mode & std::ios::in) {
      mode = std::ios::in;
    *p++ = 'r';
  } else if (io_mode & std::ios::app) {
      mode = std::ios::app;
    *p++ = 'a';
  } else {
      mode = std::ios::out;
    *p++ = 'w';
  }

  if (io_mode & std::ios::binary) {
      mode |= std::ios::binary;
    *p++ = 'b';
  }

  // Hard code the compression level
  if (io_mode & (std::ios::out | std::ios::app)) {
    *p++ = '9';
  }

  // Put the end-of-string indicator
  *p = '\0';

  if ( (file = gzopen(name, char_mode)) == NULL )
    return NULL;

  own_file_descriptor = 1;

  return this;

}

gzfilebuf *gzfilebuf::attach( int file_descriptor,
                              int io_mode ) {

  if ( is_open() )
    return NULL;

  char char_mode[10];
  char *p = char_mode;

  if (io_mode & std::ios::in) {
      mode = std::ios::in;
    *p++ = 'r';
  } else if (io_mode & std::ios::app) {
      mode = std::ios::app;
    *p++ = 'a';
  } else {
      mode = std::ios::out;
    *p++ = 'w';
  }

  if (io_mode & std::ios::binary) {
      mode |= std::ios::binary;
    *p++ = 'b';
  }

  // Hard code the compression level
  if (io_mode & (std::ios::out | std::ios::app)) {
    *p++ = '9';
  }

  // Put the end-of-string indicator
  *p = '\0';

  if ( (file = gzdopen(file_descriptor, char_mode)) == NULL )
    return NULL;

  own_file_descriptor = 0;

  return this;

}

gzfilebuf *gzfilebuf::close() {

  if ( is_open() ) {

    sync();
    gzclose( file );
    file = NULL;

  }

  return this;

}

int gzfilebuf::setcompressionlevel( int comp_level ) {

  return gzsetparams(file, comp_level, -2);

}

int gzfilebuf::setcompressionstrategy( int comp_strategy ) {

  return gzsetparams(file, -2, comp_strategy);

}


std::streampos gzfilebuf::seekoff(std::streamoff off, std::ios_base::seekdir dir, int which) {

  return std::streampos(EOF);

}

int gzfilebuf::underflow() {

  // If the file hasn't been opened for reading, error.
    if (!is_open() || !(mode & std::ios::in))
    return EOF;

  // if a buffer doesn't exists, allocate one.
  if ( !base() ) {

    if ( (allocate()) == EOF )
      return EOF;
    setp(0,0);

  } else {

    if ( in_avail() )
      return (unsigned char) *gptr();

    if ( out_waiting() ) {
      if ( flushbuf() == EOF )
        return EOF;
    }

  }

  // Attempt to fill the buffer.

  int result = fillbuf();
  if ( result == EOF ) {
    // disable get area
    setg(0,0,0);
    return EOF;
  }

  return (unsigned char) *gptr();

}

int gzfilebuf::overflow( int c ) {

  if (!is_open() || !(mode & std::ios::out))
    return EOF;

  if ( !base() ) {
    if ( allocate() == EOF )
      return EOF;
    setg(0,0,0);
  } else {
    if (in_avail()) {
        return EOF;
    }
    if (out_waiting()) {
      if (flushbuf() == EOF)
        return EOF;
    }
  }

  int bl = blen();
  setp( base(), base() + bl);

  if ( c != EOF ) {

    *pptr() = c;
    pbump(1);

  }

  return 0;

}

int gzfilebuf::sync() {

  if ( !is_open() )
    return EOF;

  if ( out_waiting() )
    return flushbuf();

  return 0;

}

int gzfilebuf::flushbuf() {

  int n;
  char *q;

  q = pbase();
  n = pptr() - q;

  if ( gzwrite( file, q, n) < n )
    return EOF;

  setp(0,0);

  return 0;

}

int gzfilebuf::fillbuf() {

  int required;
  char *p;

  p = base();

  required = blen();

  int t = gzread( file, p, required );

  if ( t <= 0) return EOF;

  setg( base(), base(), base()+t);

  return t;

}

gzfilestream_common::gzfilestream_common() :
        std::ios(gzfilestream_common::rdbuf()) { }

gzfilestream_common::~gzfilestream_common()
{ }

void gzfilestream_common::attach( int fd, int io_mode ) {

  if ( !buffer.attach( fd, io_mode) )
        clear(std::ios::failbit | std::ios::badbit);
  else
    clear();

}

void gzfilestream_common::open( const char *name, int io_mode ) {

  if ( !buffer.open( name, io_mode ) )
        clear(std::ios::failbit | std::ios::badbit);
  else
    clear();

}

void gzfilestream_common::close() {

  if ( !buffer.close() )
        clear(std::ios::failbit | std::ios::badbit);

}

gzfilebuf *gzfilestream_common::rdbuf()
{
  return &buffer;
}

gzifstream::gzifstream() :
        std::ios(gzfilestream_common::rdbuf()) {
    clear(std::ios::badbit);
}

gzifstream::gzifstream( const char *name, int io_mode ) :
        std::ios(gzfilestream_common::rdbuf()) {
  gzfilestream_common::open( name, io_mode );
}

gzifstream::gzifstream( int fd, int io_mode ) :
        std::ios(gzfilestream_common::rdbuf()) {
  gzfilestream_common::attach( fd, io_mode );
}

gzifstream::~gzifstream() { }

gzofstream::gzofstream() :
        std::ios(gzfilestream_common::rdbuf()) {
    clear(std::ios::badbit);
}

gzofstream::gzofstream( const char *name, int io_mode ) :
        std::ios(gzfilestream_common::rdbuf()) {
  gzfilestream_common::open( name, io_mode );
}

gzofstream::gzofstream( int fd, int io_mode ) :
        std::ios(gzfilestream_common::rdbuf()) {
  gzfilestream_common::attach( fd, io_mode );
}

gzofstream::~gzofstream() { }
