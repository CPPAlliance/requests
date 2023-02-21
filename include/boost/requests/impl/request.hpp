//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_REQUEST_HPP
#define BOOST_REQUESTS_IMPL_REQUEST_HPP

#include <boost/requests/http.hpp>
#include <boost/requests/service.hpp>
#include <boost/requests/session.hpp>

namespace boost
{
namespace requests
{


template<typename Connection, typename RequestBody>
auto request(Connection & conn,
             beast::http::verb method,
             urls::url_view target,
             RequestBody && body,
             typename Connection::request_type  req,
             system::error_code & ec) -> response
{
  auto s = conn.ropen(method, target, std::forward<RequestBody>(body), std::move(req), ec);
  response rb{req.get_allocator()};

  if (!ec)
    s.read( rb.buffer,  ec);
  rb.headers = std::move(s).headers();
  rb.history = std::move(s).history();

  return rb;
}

namespace detail
{

template<typename Connection, typename RequestBody>
struct async_request_op : asio::coroutine
{
  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  using completion_signature_type = void(system::error_code, response);
  using step_signature_type       = void(system::error_code, variant2::variant<std::size_t, stream>);

  Connection & conn;
  http::verb method;
  urls::url_view target;
  RequestBody && request_body;
  typename Connection::request_type req;
  optional<stream> str_;

  response rb{req.get_allocator()};

  template<typename RequestBody_>
  async_request_op(Connection * conn,
                   http::verb method,
                   urls::url_view target,
                   RequestBody_ && request_body,
                   typename Connection::request_type req)
      : conn(*conn), method(method), target(target),
        request_body(static_cast<RequestBody&&>(request_body)), req(std::move(req)) {}

  response & resume(requests::detail::faux_token_t<step_signature_type> self,
                   system::error_code & ec,
                   variant2::variant<std::size_t, stream> s)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_ASIO_CORO_YIELD conn.async_ropen(method, target,
                             std::forward<RequestBody>(request_body),
                             std::move(req), std::move(self));
      str_.emplace(std::move(variant2::get<1>(s)));
      if (!ec)
      {
        BOOST_ASIO_CORO_YIELD str_->async_read( rb.buffer, std::move(self));
      }

      rb.headers = std::move(*str_).headers();
      rb.history = std::move(*str_).history();
    }
    return rb;
  }
};


struct async_free_request_op
{
  template<typename Handler,
           typename RequestBody,
           typename Path>
  void operator()(Handler && handler,
                  beast::http::verb method,
                  Path path,
                  RequestBody && body,
                  http::fields req)
  {
    return async_request(default_session(asio::get_associated_executor(handler)), method, path,
                                         std::forward<RequestBody>(body),
                                         std::move(req), std::move(handler));
  }
};

}

template<typename Connection,
         typename RequestBody,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, response))
async_request(Connection & conn,
              beast::http::verb method,
              urls::url_view target,
              RequestBody && body,
              typename Connection::request_type req,
              CompletionToken && completion_token)
{
  return requests::detail::faux_run<detail::async_request_op<Connection, RequestBody>>(
                          std::forward<CompletionToken>(completion_token),
                          &conn, method, target, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
async_request(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void(boost::system::error_code,
                                   response)>(
          detail::async_free_request_op{}, completion_token,
          method, path, std::forward<RequestBody>(body), std::move(req));
}

template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                        response))
async_request(beast::http::verb method,
              core::string_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void (boost::system::error_code,
                                   response)>(
      detail::async_free_request_op{},
      completion_token, method, path, std::forward<RequestBody>(body), std::move(req));
}


}
}


#endif // BOOST_REQUESTS_REQUEST_HPP
