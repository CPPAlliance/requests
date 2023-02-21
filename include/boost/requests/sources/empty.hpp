//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCES_EMPTY_HPP
#define BOOST_REQUESTS_SOURCES_EMPTY_HPP

#include <boost/requests/source.hpp>
#include <boost/beast/http/empty_body.hpp>

namespace boost
{
namespace requests
{

using empty = beast::http::empty_body::value_type;

struct empty_source : source
{
  empty_source() = default;
  ~empty_source() = default;

  optional<std::size_t> size() const override
  {
    return 0ull;
  }
  void reset() override
  {

  }
  std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ec) override
  {
    return {0ull, true};
  }
  core::string_view default_content_type() override
  {
    return "";
  }
};

BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, const empty &);
BOOST_REQUESTS_DECL source_ptr tag_invoke(const make_source_tag&, const none_t &);

}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/sources/impl/empty.ipp>
#endif

#endif // BOOST_REQUESTS_SOURCES_EMPTY_HPP
