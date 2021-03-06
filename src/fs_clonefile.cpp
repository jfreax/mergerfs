/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "fs_attr.hpp"
#include "fs_fallocate.hpp"
#include "fs_sendfile.hpp"
#include "fs_time.hpp"
#include "fs_xattr.hpp"

using std::string;
using std::vector;

namespace fs
{
  int
  writen(const int     fd,
         const char   *buf,
         const size_t  count)
  {
    size_t nleft;
    ssize_t nwritten;

    nleft = count;
    while(nleft > 0)
      {
        nwritten = ::write(fd,buf,nleft);
        if(nwritten == -1)
          {
            if(errno == EINTR)
              continue;
            return -1;
          }

        nleft -= nwritten;
        buf   += nwritten;
      }

    return count;
  }

  static
  int
  copyfile_rw(const int    fdin,
              const int    fdout,
              const size_t count,
              const size_t blocksize)
  {
    ssize_t nr;
    ssize_t nw;
    ssize_t bufsize;
    size_t  totalwritten;
    vector<char> buf;

    bufsize = (blocksize * 16);
    buf.resize(bufsize);

    ::lseek(fdin,0,SEEK_SET);

    totalwritten = 0;
    while(totalwritten < count)
      {
        nr = ::read(fdin,&buf[0],bufsize);
        if(nr == -1)
          {
            if(errno == EINTR)
              continue;
            return -1;
          }

        nw = writen(fdout,&buf[0],nr);
        if(nw == -1)
          return -1;

        totalwritten += nw;
      }

    return count;
  }

  static
  int
  copydata(const int    fdin,
           const int    fdout,
           const size_t count,
           const size_t blocksize)
  {
    int rv;

    ::posix_fadvise(fdin,0,count,POSIX_FADV_WILLNEED);
    ::posix_fadvise(fdin,0,count,POSIX_FADV_SEQUENTIAL);

    fs::fallocate(fdout,0,0,count);

    rv = fs::sendfile(fdin,fdout,count);
    if((rv == -1) && ((errno == EINVAL) || (errno == ENOSYS)))
      return fs::copyfile_rw(fdin,fdout,count,blocksize);

    return rv;
  }

  static
  bool
  ignorable_error(const int err)
  {
    switch(err)
      {
      case ENOTTY:
      case ENOTSUP:
#if ENOTSUP != EOPNOTSUPP
      case EOPNOTSUPP:
#endif
        return true;
      }

    return false;
  }

  int
  clonefile(const int fdin,
            const int fdout)
  {
    int rv;
    struct stat stin;

    rv = ::fstat(fdin,&stin);
    if(rv == -1)
      return -1;

    rv = copydata(fdin,fdout,stin.st_size,stin.st_blksize);
    if(rv == -1)
      return -1;

    rv = fs::attr::copy(fdin,fdout);
    if(rv == -1 && !ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(fdin,fdout);
    if(rv == -1 && !ignorable_error(errno))
      return -1;

    rv = ::fchown(fdout,stin.st_uid,stin.st_gid);
    if(rv == -1)
      return -1;

    rv = ::fchmod(fdout,stin.st_mode);
    if(rv == -1)
      return -1;

    rv = fs::utimes(fdout,stin);
    if(rv == -1)
      return -1;

    return 0;
  }

  int
  clonefile(const string &in,
            const string &out)
  {
    int rv;
    int fdin;
    int fdout;
    int error;

    fdin = ::open(in.c_str(),O_RDONLY|O_NOFOLLOW);
    if(fdin == -1)
      return -1;

    const int flags = O_CREAT|O_LARGEFILE|O_NOATIME|O_NOFOLLOW|O_TRUNC|O_WRONLY;
    const int mode  = S_IWUSR;
    fdout = ::open(out.c_str(),flags,mode);
    if(fdout == -1)
      return -1;

    rv = fs::clonefile(fdin,fdout);
    error = errno;

    ::close(fdin);
    ::close(fdout);

    errno = error;
    return rv;
  }
}
