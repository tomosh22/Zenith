
#ifndef zfstream_h
#define zfstream_h

#include <fstream>
#include "zlib.h"

class gzfilebuf : public std::streambuf {

public:

  gzfilebuf( );
  virtual ~gzfilebuf();

  gzfilebuf *open( const char *name, int io_mode );
  gzfilebuf *attach( int file_descriptor, int io_mode );
  gzfilebuf *close();

  int setcompressionlevel( int comp_level );
  int setcompressionstrategy( int comp_strategy );

  inline int is_open() const { return (file !=NULL); }

  virtual std::streampos seekoff(std::streamoff, std::ios_base::seekdir, int);

  virtual int sync();

protected:

  virtual int underflow();
  virtual int overflow( int = EOF );

private:

  gzFile file;
  short mode;
  short own_file_descriptor;

  int flushbuf();
  int fillbuf();

};

class gzfilestream_common : virtual public std::ios {

  friend class gzifstream;
  friend class gzofstream;
  friend gzofstream &setcompressionlevel( gzofstream &, int );
  friend gzofstream &setcompressionstrategy( gzofstream &, int );

public:
  virtual ~gzfilestream_common();

  void attach( int fd, int io_mode );
  void open( const char *name, int io_mode );
  void close();

protected:
  gzfilestream_common();

private:
  gzfilebuf *rdbuf();

  gzfilebuf buffer;

};

class gzifstream : public gzfilestream_common, public std::istream {

public:

  gzifstream();
    gzifstream(const char *name, int io_mode = std::ios::in);
  gzifstream(int fd, int io_mode = std::ios::in);

  virtual ~gzifstream();

};

class gzofstream : public gzfilestream_common, public std::ostream {

public:

  gzofstream();
    gzofstream(const char *name, int io_mode = std::ios::out);
  gzofstream(int fd, int io_mode = std::ios::out);

  virtual ~gzofstream();

};

template<class T> class gzomanip {
  friend gzofstream &operator<<(gzofstream &, const gzomanip<T> &);
public:
  gzomanip(gzofstream &(*f)(gzofstream &, T), T v) : func(f), val(v) { }
private:
  gzofstream &(*func)(gzofstream &, T);
  T val;
};

template<class T> gzofstream &operator<<(gzofstream &s, const gzomanip<T> &m)
{
  return (*m.func)(s, m.val);
}

inline gzofstream &setcompressionlevel( gzofstream &s, int l )
{
  (s.rdbuf())->setcompressionlevel(l);
  return s;
}

inline gzofstream &setcompressionstrategy( gzofstream &s, int l )
{
  (s.rdbuf())->setcompressionstrategy(l);
  return s;
}

inline gzomanip<int> setcompressionlevel(int l)
{
  return gzomanip<int>(&setcompressionlevel,l);
}

inline gzomanip<int> setcompressionstrategy(int l)
{
  return gzomanip<int>(&setcompressionstrategy,l);
}

#endif
