//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RESPONSE_HPP
#define BOOST_REQUESTS_RESPONSE_HPP

#include <boost/asio/coroutine.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/basic_dynamic_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/config.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/core/span.hpp>
#include <boost/requests/error.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/requests/http.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <memory>
#include <scoped_allocator>
#include <string>
#include <type_traits>

namespace boost
{
namespace requests
{

struct response_base
{
  using allocator_type = container::pmr::polymorphic_allocator<char>;
  using buffer_type    = beast::basic_flat_buffer<allocator_type>;
  using body_type      = beast::http::basic_dynamic_body<buffer_type>;

  http::response_header headers;

  int          result_code() const {return headers.result_int(); }
  http::status result()      const {return headers.result(); }

  using string_body_type = typename beast::http::basic_string_body<char, std::char_traits<char>, allocator_type>;
  using vector_alloc = boost::container::pmr::polymorphic_allocator<typename http::response<body_type>>;
  using history_type = std::vector<typename http::response<body_type>, vector_alloc>;
  history_type history{vector_alloc{headers.get_allocator()}};

  response_base(allocator_type alloc,         history_type history) : headers(alloc),             history(std::move(history)) {}
  response_base(http::response_header header, history_type history) : headers(std::move(header)), history(std::move(history)) {}

  response_base(allocator_type alloc        ) : headers(alloc),             history (vector_alloc{alloc}) {}
  response_base(http::response_header header) : headers(std::move(header)), history (vector_alloc{headers.get_allocator()}) {}

  ~response_base() = default;

  response_base(const response_base & ) = default;
  response_base(response_base && lhs) noexcept : headers(std::move(lhs.headers)), history(std::move(lhs.history)) {}

  response_base& operator=(const response_base & ) = default;
  response_base& operator=(response_base && lhs) noexcept
  {
    headers = std::move(lhs.headers);
    history = std::move(lhs.history);
    return *this;
  }

  bool ok () const
  {
    using namespace beast::http;
    const auto s = to_status_class(headers.result());
    return s == status_class::client_error || s == status_class::server_error;
  }
  explicit operator bool() const { return ok(); }

  bool is_redirect() const
  {
    using s = beast::http::status;
    switch (headers.result())
    {
    case s::moved_permanently: return true;
    case s::found: return true;
    case s::temporary_redirect: return true;
    case s::permanent_redirect: return true;
    default : return false;
    }
  }
  bool is_permanent_redirect() const
  {
    using s = beast::http::status;
    switch (headers.result())
    {
    case s::moved_permanently: return true;
    case s::permanent_redirect: return true;
    default : return false;
    }
  }

  system::error_code status_as_error() const
  {
    system::error_code res;
    res.assign(headers.result_int(), http_status_category());
    return res;
  }

  void throw_status_if_error() const
  {
    auto ec = status_as_error();
    if (ec)
      boost::throw_exception(system::system_error(ec));
  }

  system::result<std::vector<struct link>> link() const
  {
    std::vector<struct link> res;

    for (const auto & l : boost::make_iterator_range(headers.equal_range(beast::http::field::link)))
    {
      auto ll = parse_link_field(l.value());
      if (ll.has_error())
        return ll.error();

      res.insert(res.end(), ll->begin(), ll->end());
    }
    return res;
  }
};


struct response : response_base
{
  buffer_type buffer{headers.get_allocator()};

  response(allocator_type alloc = {}) : response_base(alloc), buffer(alloc) {}
  response(http::response_header header, buffer_type buffer) : response_base(std::move(header)), buffer(std::move(buffer)) {}
  response(response_base         header, buffer_type buffer) : response_base(std::move(header)), buffer(std::move(buffer)) {}

  response(http::response_header header, history_type history, buffer_type buffer) : response_base(std::move(header), std::move(history)), buffer(std::move(buffer)) {}

  response(const response & ) = default;
  response(response && lhs) noexcept  : response_base(std::move(lhs)), buffer(std::move(lhs.buffer)) {}

  response& operator=(const response & ) = default;
  response& operator=(response && lhs) noexcept
  {
    response_base::operator=(std::move(lhs));
    buffer = std::move(lhs.buffer);
    return *this;
  }

  template<typename Char = char,
           typename CharTraits = std::char_traits<char>>
  auto string_view() const -> basic_string_view<Char, CharTraits>
  {
    const auto cd = buffer.cdata();
    return basic_string_view<Char, CharTraits>(static_cast<const char*>(cd.data()), cd.size());
  }

  template<typename Char = char,
            typename CharTraits = std::char_traits<char>,
            typename Allocator_>
  auto string(Allocator_ && alloc) const -> std::basic_string<Char, CharTraits, std::decay_t<Allocator_>>
  {
    return string_view<Char, CharTraits>().to_string(std::forward<Allocator_>(alloc));
  }

  template<typename Char = char,
           typename CharTraits = std::char_traits<char>>
  auto string() const -> std::basic_string<Char, CharTraits, allocator_type>
  {
    return string_view<Char, CharTraits>().to_string(headers.get_allocator());
  }

  template<typename Byte = unsigned char>
  auto raw() const -> span<Byte>
  {
    const auto cd = buffer.cdata();
    return span<Byte>(static_cast<const Byte*>(cd.data()), cd.size() / sizeof(Byte));
  }
};

}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/response.ipp>
#endif

#endif // BOOST_REQUESTS_RESPONSE_HPP
