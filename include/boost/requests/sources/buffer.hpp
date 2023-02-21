// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_BUFFER_HPP
#define BOOST_REQUESTS_SOURCES_BUFFER_HPP

#include <boost/requests/source.hpp>
#include <boost/json/value.hpp>
#include <boost/json/serializer.hpp>

namespace boost
{
namespace requests
{

struct buffer_source : source
{
  asio::const_buffer buffer;
  asio::const_buffer current{buffer};

  buffer_source(asio::const_buffer buffer) : buffer(buffer)
  {
  }

  ~buffer_source() = default;
  optional<std::size_t> size() const override {return buffer.size();};
  void reset() override
  {
    current = buffer;
  }
  std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ec) override
  {
    auto n = asio::buffer_copy(asio::buffer(data, size), this->current);
    current += n;
    return {n, current.size() > 0u};
  }
  core::string_view default_content_type() override {return "application/octet-stream";}
};

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, asio::const_buffer cb, container::pmr::memory_resource * res);

}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/sources/impl/buffer.ipp>
#endif

#endif //BOOST_REQUESTS_SOURCES_BUFFER_HPP
