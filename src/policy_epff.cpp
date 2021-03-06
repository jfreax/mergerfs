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

#include <string>
#include <vector>

#include "fs.hpp"
#include "fs_path.hpp"
#include "policy.hpp"

using std::string;
using std::vector;
using mergerfs::Category;

static
int
_epff_create(const vector<string>  &basepaths,
             const char            *fusepath,
             const uint64_t         minfreespace,
             vector<const string*> &paths)
{
  string fullpath;
  const string *fallback;

  fallback = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      bool readonly;
      uint64_t spaceavail;
      uint64_t _spaceused;
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      if(!fs::exists(fullpath))
        continue;
      if(!fs::info(*basepath,readonly,spaceavail,_spaceused))
        continue;
      if(readonly)
        continue;
      if(fallback == NULL)
        fallback = basepath;
      if(spaceavail < minfreespace)
        continue;

      paths.push_back(basepath);

      return POLICY_SUCCESS;
    }

  if(fallback == NULL)
    return POLICY_FAIL_ENOENT;

  paths.push_back(fallback);

  return POLICY_SUCCESS;
}

static
int
_epff_other(const vector<string>  &basepaths,
            const char            *fusepath,
            vector<const string*> &paths)
{
  string fullpath;

  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      if(!fs::exists(fullpath))
        continue;

      paths.push_back(basepath);

      return POLICY_SUCCESS;
    }

  return POLICY_FAIL_ENOENT;
}

static
int
_epff(const Category::Enum::Type  type,
      const vector<string>       &basepaths,
      const char                 *fusepath,
      const uint64_t              minfreespace,
      vector<const string*>      &paths)
{
  if(type == Category::Enum::create)
    return _epff_create(basepaths,fusepath,minfreespace,paths);

  return _epff_other(basepaths,fusepath,paths);
}


namespace mergerfs
{
  int
  Policy::Func::epff(const Category::Enum::Type  type,
                     const vector<string>       &basepaths,
                     const char                 *fusepath,
                     const uint64_t              minfreespace,
                     vector<const string*>      &paths)
  {
    int rv;

    rv = _epff(type,basepaths,fusepath,minfreespace,paths);
    if(POLICY_FAILED(rv))
      rv = Policy::Func::ff(type,basepaths,fusepath,minfreespace,paths);

    return rv;
  }
}
