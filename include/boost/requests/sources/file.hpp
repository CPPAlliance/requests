// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_FILE_HPP
#define BOOST_REQUESTS_SOURCES_FILE_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/source.hpp>
#include <boost/requests/mime_types.hpp>
#include <boost/beast/core/file.hpp>

#if defined(__cpp_lib_filesystem)
#include <filesystem>
#endif

namespace boost
{

namespace filesystem
{
class path;
}

namespace requests
{

struct file_source : source
{
  requests::filesystem::path path;
  beast::file file;
  system::error_code ec;

  BOOST_REQUESTS_DECL file_source(const boost::filesystem::path & file);
#if defined(__cpp_lib_filesystem)
  BOOST_REQUESTS_DECL file_source(const std::filesystem::path & file);
#endif
  file_source(const file_source & );

  ~file_source() = default;
  optional<std::size_t> size() const override
  {
    boost::system::error_code ec;
    auto n = file.size(ec);
    if (ec)
      return none;
    else
      return n;
  };
  void reset() override
  {
    file.seek(0, ec);
  }
  std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ec) override
  {
    if (this->ec)
    {
      ec = this->ec;
      return {0u, true};
    }
    auto n = file.read(data, size, ec);
    return {n, file.pos(ec) != file.size(ec)};
  }
  core::string_view default_content_type() override
  {
    const auto & mp = default_mime_type_map();
    const auto ext = path.extension().string();
    auto itr = mp.find(ext);
    if (itr != mp.end())
      return itr->second;
    else
      return "text/plain";
  }
};


}
}


#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/sources/impl/file.ipp>
#endif

#endif //BOOST_REQUESTS_SOURCES_FILE_HPP
