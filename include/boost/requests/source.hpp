// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCE_HPP
#define BOOST_REQUESTS_SOURCE_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/http.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/json/fwd.hpp>


namespace boost
{
namespace requests
{

struct source
{
  virtual ~source() = default;
  virtual optional<std::size_t> size() const = 0;
  virtual void reset() = 0;
  virtual std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ) = 0;
  virtual core::string_view default_content_type() {return "";}
};

struct make_source_tag {};

using source_ptr = std::shared_ptr<source>;

inline auto tag_invoke(const make_source_tag&, source_ptr s)
{
  return s;
}

inline auto tag_invoke(const make_source_tag&, source_ptr s,
                       container::pmr::memory_resource *)
{
  return s;
}


template<typename Source>
auto make_source(Source && source, container::pmr::memory_resource * = container::pmr::get_default_resource())
    -> decltype(tag_invoke(make_source_tag{}, std::declval<Source>()))
{
  return tag_invoke(make_source_tag{}, std::forward<Source>(source));
}

template<typename Source>
auto make_source(Source && source, container::pmr::memory_resource * res = container::pmr::get_default_resource())
    -> decltype(tag_invoke(make_source_tag{}, std::declval<Source>(), res))
{
  return tag_invoke(make_source_tag{}, std::forward<Source>(source), res);
}


template<typename Stream>
std::size_t write_request(
    Stream & stream,
    http::verb method,
    core::string_view target,
    http::fields& header,
    source& src,
    system::error_code & ec);


template<typename Stream>
std::size_t write_request(
    Stream & stream,
    http::verb method,
    core::string_view target,
    http::fields &header,
    source &src)
{
  boost::system::error_code ec;
  auto res = write_request(stream, method, target, header, src, ec);
  if (ec)
    throw_exception(system::system_error(ec, "write_request"));
  return res;
}


template<typename Stream,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t)) CompletionToken
           BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(system::error_code, std::size_t))
  async_write_request(
    Stream & stream,
    http::verb method,
    core::string_view target,
    http::fields & header,
    source &src,
    CompletionToken && token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(typename Stream::executor_type));

using empty = beast::http::empty_body::value_type;

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, asio::const_buffer cb, container::pmr::memory_resource * res);
BOOST_REQUESTS_DECL
source_ptr tag_invoke(const make_source_tag&, const empty &);
BOOST_REQUESTS_DECL
source_ptr tag_invoke(const make_source_tag&, const none_t &);

BOOST_REQUESTS_DECL
source_ptr tag_invoke(const make_source_tag&, const filesystem::path & path, container::pmr::memory_resource * res);
source_ptr tag_invoke(const make_source_tag&,       filesystem::path &&,     container::pmr::memory_resource * res) = delete;


BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, struct form form_, container::pmr::memory_resource * res);
BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, struct multi_part_form mpf, container::pmr::memory_resource * res);

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, boost::json::value value, container::pmr::memory_resource * res);
BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, boost::json::object  obj, container::pmr::memory_resource * res);
BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, boost::json::array   arr, container::pmr::memory_resource * res);
BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, const boost::json::string & arr,
                                                            container::pmr::memory_resource * res);
source_ptr tag_invoke(make_source_tag, boost::json::string  &&str, container::pmr::memory_resource * res) = delete;



}
}

#include <boost/requests/impl/source.hpp>
#include <boost/requests/sources/string.hpp>
#include <boost/requests/sources/string_view.hpp>

#endif //BOOST_REQUESTS_SOURCE_HPP
