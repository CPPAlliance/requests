//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_JSON_HPP
#define BOOST_REQUESTS_JSON_HPP


#include <boost/requests/detail/faux_coroutine.hpp>
#include <boost/requests/error.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/sources/json.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/json.hpp>
#include <boost/json/parser.hpp>
#include <boost/json/value.hpp>
#include <boost/system/result.hpp>

#include <boost/range.hpp>
#include <vector>

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>



namespace boost
{
namespace requests
{

inline auto as_json(const response & res,
                    json::storage_ptr ptr,
                    system::error_code & ec) -> json::value
{
  json::parser ps;
  ps.write(res.string_view(), ec);
  if (ec)
    return nullptr;
  else
    return ps.release();
}

inline auto as_json(const response & res, json::storage_ptr ptr = {}) -> json::value
{
  boost::system::error_code ec;
  auto rs = as_json(res, ptr, ec);
  if (ec)
    urls::detail::throw_system_error(ec);

  return rs;
}

inline auto as_json(const response & res, system::error_code & ec) -> json::value
{
  return as_json(res, json::storage_ptr(), ec);
}

namespace json
{

using empty = beast::http::empty_body::value_type;
using ::boost::json::value;
using ::boost::json::storage_ptr;

template<typename Value = value>
struct response : response_base
{
  using allocator_type = container::pmr::polymorphic_allocator<char>;
  using fields_type = http::fields;

  response(allocator_type alloc,         history_type history, Value && value) : response_base(std::move(alloc),  std::move(history)), value(std::move(value)) {}
  response(http::response_header header, history_type history, Value && value) : response_base(std::move(header), std::move(history)), value(std::move(value)) {}

  response(allocator_type alloc        , Value && value = {}) : response_base(std::move(alloc)),  value(std::move(value)) {}
  response(http::response_header header, Value && value = {}) : response_base(std::move(header)), value(std::move(value)) {}

  using value_type = Value;
  value_type value;

};

template<typename Value = value, typename Stream>
Value read_json(Stream & str, json::storage_ptr ptr = {})
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  while (!sp.done() && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer));
    sp.write_some(buffer, n);
  }
  sp.finish();
  return ::boost::json::value_to<Value>(sp.release());
}

template<typename Value = value,
         typename Stream>
Value read_json(Stream & str,
                json::storage_ptr ptr,
                system::error_code & ec)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];
  while (!sp.done() && !ec && !str.done())
  {
    const auto n = str.read_some(asio::buffer(buffer), ec);
    if (ec)
      break;
    sp.write_some(buffer, n, ec);
  }
  if (!ec)
    sp.finish(ec);
  if (ec)
    return Value();

  auto res =  ::boost::json::try_value_to<Value>(sp.release());
  if (res.has_error())
  {
    ec = res.error();
    return Value();
  }
  else
    return res.value();
}

template<typename Value = value, typename Stream>
optional<Value> read_optional_json(Stream & str, json::storage_ptr ptr = {})
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  auto n = str.read_some(asio::buffer(buffer));
  if (n == 0u && str.done())
    return boost::none;

  sp.write_some(buffer, n);
  while (!sp.done() && !str.done())
  {
    n = str.read_some(asio::buffer(buffer));
    sp.write_some(buffer, n);
  }
  sp.finish();
  if (std::is_same<Value, value>::value)
    return sp.release();

  return ::boost::json::value_to<Value>(sp.release());
}

template<typename Value = value,
          typename Stream>
optional<Value> read_optional_json(
                Stream & str,
                json::storage_ptr ptr,
                system::error_code & ec)
{
  ::boost::json::stream_parser sp{ptr};
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  auto n = str.read_some(asio::buffer(buffer));
  if (n == 0u && str.done())
    return boost::none;
  sp.write_some(buffer, n, ec);

  while (!sp.done() && !ec && !str.done())
  {
    n = str.read_some(asio::buffer(buffer), ec);
    if (ec)
      break;
    sp.write_some(buffer, n, ec);
  }
  if (!ec)
    sp.finish(ec);
  if (ec)
    return Value();

  if (std::is_same<Value, value>::value)
    return sp.release();

  auto res =  ::boost::json::try_value_to<Value>(sp.release());
  if (res.has_error())
  {
    ec = res.error();
    return Value();
  }
  else
    return res.value();
}

inline void set_accept_headers(http::fields & hd)
{
  if (hd.count(http::field::accept) == 0)
    hd.set(http::field::accept, "application/json");
}

inline void set_accept_headers(requests::request_parameters & hd)
{
  set_accept_headers(hd.fields);
}

template<typename Value = json::value,
         typename Connection>
auto get(Connection & conn,
         urls::url_view target,
         typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  // this might be a bet idea
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename Connection>
auto get(Connection & conn,
         urls::url_view target,
         typename Connection::request_type req,
         system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::get, target, empty{}, std::move(req), ec);

  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


using requests::head;
using requests::trace;

template<typename Value = json::value,
         typename RequestBody,
         typename Connection>
auto post(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto post(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}

template<typename Value = json::value,
         typename RequestBody,
         typename Connection>
auto patch(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr, ec),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto patch(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto put(Connection & conn,
         urls::url_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr, ec),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
         urls::url_view target,
         RequestBody && request_body,
         typename Connection::request_type req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
         urls::url_view target,
         RequestBody && request_body,
         typename Connection::request_type req,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr, ec),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody,
          typename Connection>
auto delete_(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename Connection>
auto options(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename Connection>
auto options(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req,
             system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = conn.ropen(http::verb::options, target, empty{}, std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value>
auto get(urls::url_view target,
         http::fields req = {}) -> response<Value>
{
  set_accept_headers(req);
  // this might be a bet idea
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::get, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value>
auto get(urls::url_view target,
         http::fields req,
         system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::get, target, empty{}, std::move(req), ec);

  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


using requests::head;
using requests::trace;

template<typename Value = json::value,
          typename RequestBody>
auto post(urls::url_view target,
          RequestBody && request_body,
          http::fields req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto post(urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::post, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}

template<typename Value = json::value,
          typename RequestBody>
auto patch(urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody>
auto patch(urls::url_view target,
           RequestBody && request_body,
           http::fields req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::patch, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value,
          typename RequestBody>
auto put(urls::url_view target,
         RequestBody && request_body,
         http::fields req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto put(urls::url_view target,
         RequestBody && request_body,
         http::fields req,
         system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::put, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}


template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             RequestBody && request_body,
             http::fields req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::delete_, target,
                      ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             http::fields req = {}) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr) };
}

template<typename Value = json::value,
          typename RequestBody>
auto delete_(urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response<optional<Value>>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::delete_, target,
                      empty{},
                      std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_optional_json<Value>(s, ptr, ec) };
}



template<typename Value = json::value,
          typename Connection>
auto options(urls::url_view target,
             http::fields req = {}) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::options, target, empty{}, std::move(req));
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr) };
}


template<typename Value = json::value>
auto options(urls::url_view target,
             http::fields req,
             system::error_code & ec) -> response<Value>
{
  set_accept_headers(req);
  json::storage_ptr ptr{req.get_allocator().resource()};
  auto s = default_session().ropen(http::verb::options, target, empty{}, std::move(req), ec);
  return { std::move(s).headers(),  std::move(s).history(), read_json<Value>(s, ptr, ec) };
}

namespace detail
{

template<typename Stream>
struct async_read_json_op : asio::coroutine
{
  using executor_type = typename Stream::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  using completion_signature_type = void(system::error_code, value);
  using step_signature_type       = void(system::error_code, std::size_t);

  Stream &str;
  ::boost::json::stream_parser sp;
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  async_read_json_op(Stream * str, json::storage_ptr ptr) : str(*str), sp{ptr} {}

  value resume(requests::detail::faux_token_t<step_signature_type> self,
               system::error_code & ec, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      while (!sp.done() && !str.done() && !ec)
      {
        BOOST_ASIO_CORO_YIELD str.async_read_some(asio::buffer(buffer), std::move(self));
        if (ec)
          return nullptr;
        sp.write_some(buffer, n, ec);
      }
      if (!ec)
        sp.finish(ec);
      if (ec)
        return nullptr;

      return sp.release();
    }
    return nullptr;
  }
};


template<typename Stream>
struct async_read_optional_json_op : asio::coroutine
{
  using executor_type = typename Stream::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  using completion_signature_type = void(system::error_code, response<optional<value>>);
  using step_signature_type       = void(system::error_code, std::size_t);

  Stream & str;
  ::boost::json::stream_parser sp;
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  async_read_optional_json_op(Stream * str, json::storage_ptr ptr) : str(*str), sp{ptr} {}

  optional<value> resume(requests::detail::faux_token_t<step_signature_type> self,
                         system::error_code & ec, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_ASIO_CORO_YIELD str.async_read_some(asio::buffer(buffer), std::move(self));
      if (ec || (n == 0  && str.done()))
        return boost::none;
      sp.write_some(buffer, n, ec);

      while (!sp.done() && !str.done())
      {
        BOOST_ASIO_CORO_YIELD str.async_read_some(asio::buffer(buffer), std::move(self));
        if (ec)
          return boost::none;
        sp.write_some(buffer, n, ec);
      }
      if (!ec)
        sp.finish(ec);
      if (ec)
        return boost::none;
      return sp.release();
    }
    return boost::none;
  }
};

}

template<typename Value = value,
         typename Stream,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, Value)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, Value))
async_read_json(Stream & str,
                json::storage_ptr ptr = {},
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return requests::detail::faux_run<
      detail::async_read_json_op<Stream>>(std::forward<CompletionToken>(completion_token), &str, ptr);
}


template<typename Value = value,
          typename Stream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, optional<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::Stream::error_code, optional<Value>))
async_read_optional_json(Stream & str,
                json::storage_ptr ptr = {},
                CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return requests::detail::faux_run<
      detail::async_read_optional_json_op<Stream>>(std::forward<CompletionToken>(completion_token), &str, ptr);
}

using requests::async_head;
using requests::async_trace;

namespace detail
{

template<typename Connection, typename Value = json::value, typename RequestBody = empty>
struct async_request_json_op : asio::coroutine
{
  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  using completion_signature_type = void(system::error_code, response<Value>);
  using step_signature_type       = void(system::error_code, variant2::variant<variant2::monostate,
                                                                               stream, value>);

  Connection & conn;
  http::verb method;
  urls::url_view target;
  RequestBody && request_body;
  typename Connection::request_type req;
  json::storage_ptr ptr{req.get_allocator().resource()};
  optional<stream> str_;

  response<Value> rb{req.get_allocator()};

  json::value value_from(RequestBody && request_body, std::false_type) const
  {
    return ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr);
  }

  empty value_from(RequestBody && request_body, std::true_type) const
  {
    return {};
  }

  async_request_json_op(Connection * conn,
       http::verb method,
       urls::url_view target,
       RequestBody && request_body,
       typename Connection::request_type req)
      : conn(*conn), method(method), target(target),
        request_body(std::forward<RequestBody>(request_body)), req(std::move(req)) {}

  response<Value> & resume(requests::detail::faux_token_t<step_signature_type> self,
                         system::error_code & ec,
                         variant2::variant<variant2::monostate, stream, value> s)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_ASIO_CORO_YIELD conn.async_ropen(method, target,
                             value_from(std::forward<RequestBody>(request_body),
                                        std::is_same<empty, std::decay_t<RequestBody>>{}),
                             std::move(req), std::move(self));
      if (!ec)
      {
        str_.emplace(std::move(variant2::get<1>(s)));
        rb.headers = std::move(*str_).headers();
        rb.history = std::move(*str_).history();
        BOOST_ASIO_CORO_YIELD async_read_json(*str_, ptr, std::move(self));
        if (ec)
          return rb;
      }
      else
      {
        rb.headers = std::move(std::move(variant2::get<1>(s))).headers();
        rb.history = std::move(std::move(variant2::get<1>(s))).history();
        break;
      }

      auto v = ::boost::json::try_value_to<Value>(variant2::get<2>(s));
      if (v)
        rb.value = std::move(*v);
      else
        ec = v.error();
    }
    return rb;
  }
};


template<typename Connection, typename Value = json::value, typename RequestBody = empty>
struct async_request_optional_json_op : asio::coroutine
{
  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  using completion_signature_type = void(system::error_code, response<optional<Value>>);
  using step_signature_type       = void(system::error_code, variant2::variant<variant2::monostate, stream, optional<value>>);

  Connection & conn;
  http::verb method;
  urls::url_view target;
  RequestBody && request_body;
  typename Connection::request_type req;
  json::storage_ptr ptr{req.get_allocator().resource()};
  optional<stream> str_;

  response<optional<Value>> rb{req.get_allocator()};

  template<typename RequestBody_>
  async_request_optional_json_op(Connection * conn,
                                 http::verb method,
                                 urls::url_view target,
                                 RequestBody_ && request_body,
                                 typename Connection::request_type req)
      : conn(*conn), method(method), target(target),
        request_body(std::forward<RequestBody_>(request_body)), req(std::move(req)) {}

  response<optional<Value>> & resume(requests::detail::faux_token_t<step_signature_type> self,
                          system::error_code & ec,
                          variant2::variant<variant2::monostate, stream, optional<value>> s)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_ASIO_CORO_YIELD conn.async_ropen(method, target,
                             ::boost::json::value_from(std::forward<RequestBody>(request_body), ptr),
                             std::move(req), std::move(self));
      if (!ec)
      {
        str_.emplace(std::move(variant2::get<1>(s)));
        rb.headers = std::move(*str_).headers();
        rb.history = std::move(*str_).history();
        BOOST_ASIO_CORO_YIELD async_read_json(*str_, ptr, std::move(self));
      }
      else
      {
        rb.headers = std::move(std::move(variant2::get<1>(s))).headers();
        rb.history = std::move(std::move(variant2::get<1>(s))).history();
        break;
      }


      if (variant2::get<2>(s))
      {
        auto v = ::boost::json::try_value_to<Value>(*variant2::get<2>(s));
        if (v)
          rb.value = std::move(*v);
        else
          ec = v.error();
      }
    }
    return rb;
  }
};

}

template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_get(Connection & conn,
          urls::url_view target,
          typename Connection::request_type req = {},
          CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_json_op<
      Connection, Value>>(std::forward<CompletionToken>(completion_token),
                          &conn, http::verb::get,  target, empty{}, std::move(req));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_post(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {},
           CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_json_op<
      Connection, Value, std::decay_t<RequestBody>>>(std::forward<CompletionToken>(completion_token),
                          &conn, http::verb::post,  target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_patch(Connection & conn,
           urls::url_view target,
           RequestBody && request_body,
           typename Connection::request_type req = {},
           CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_json_op<
       Connection, Value, std::decay_t<RequestBody>>>(std::forward<CompletionToken>(completion_token),
                           &conn, http::verb::patch,  target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<optional<Value>>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_put(Connection & conn,
          urls::url_view target,
          RequestBody && request_body,
          typename Connection::request_type req = {},
          CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_optional_json_op<
      Connection, Value, std::decay_t<RequestBody>>>(std::forward<CompletionToken>(completion_token),
                          &conn, http::verb::put,  target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(Connection & conn,
            urls::url_view target,
            RequestBody && request_body,
            typename Connection::request_type req = {},
            CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_json_op<
      Connection, Value, std::decay_t<RequestBody>>>(std::forward<CompletionToken>(completion_token),
                          &conn, http::verb::delete_,  target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(Connection & conn,
             urls::url_view target,
             typename Connection::request_type req = {},
             CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_json_op<
      Connection, Value>>(std::forward<CompletionToken>(completion_token),
                          &conn, http::verb::delete_,  target, empty{}, std::move(req));
}


template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Connection::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_options(Connection & conn,
              urls::url_view target,
              typename Connection::request_type req = {},
              CompletionToken && completion_token = typename Connection::default_token())
{
  return requests::detail::faux_run<detail::async_request_json_op<
      Connection, Value>>(std::forward<CompletionToken>(completion_token),
                          &conn, http::verb::options,  target, empty{}, std::move(req));
}

namespace detail
{

template<typename Value = value>
struct async_free_request_op
{
  template<typename Handler,
            typename RequestBody,
            typename Connection>
  void operator()(Handler && handler,
                  Connection * conn,
                  http::verb method,
                  urls::url_view target,
                  RequestBody && request_body,
                  typename Connection::request_type req)
  {
    auto & sess = default_session(asio::get_associated_executor(handler));
    return requests::detail::faux_run<
        async_request_json_op<decltype(sess), Value>>(
              std::forward<Handler>(handler),
              &sess, method, target,
              std::forward<RequestBody>(request_body),std::move(req));
  }
};

}

template<typename Value = value,
          typename Connection,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_get(urls::url_view target,
          http::fields req,
          CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, Value)>(
          detail::async_free_request_op<Value>{}, completion_token,
          http::verb::get,  target, empty{}, std::move(req));
}

template<typename Value = value,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_post(urls::url_view target,
           RequestBody && request_body,
           http::fields req,
           CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, Value)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::post,  target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_patch(urls::url_view target,
            RequestBody && request_body,
            http::fields req,
            CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::patch,  target, std::forward<RequestBody>(request_body), std::move(req));
}

template<typename Value = value,
          typename Connection,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_put(urls::url_view target,
          RequestBody && request_body,
          http::fields req,
          CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::put,  target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(urls::url_view target,
             RequestBody && request_body,
             http::fields req,
             CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::delete_,  target, std::forward<RequestBody>(request_body), std::move(req));
}


template<typename Value = value,
          typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_delete(urls::url_view target,
             http::fields req,
             CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::delete_,  target, empty{}, std::move(req));
}


template<typename Value = value,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<Value>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<Value>))
async_options(urls::url_view target,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code, response<Value>)>(
      detail::async_free_request_op<Value>{}, completion_token,
      http::verb::options,  target, empty{}, std::move(req));
}




}
}
}



#endif // BOOST_REQUESTS_JSON_HPP
