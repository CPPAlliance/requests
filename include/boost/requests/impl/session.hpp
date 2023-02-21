//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_HPP
#define BOOST_REQUESTS_IMPL_SESSION_HPP

#include <boost/requests/session.hpp>
#include <boost/url/grammar/string_token.hpp>

namespace boost {
namespace requests {


struct session::async_get_pool_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session *this_;
  urls::url_view url;
  const bool is_https;

  detail::lock_guard lock;

  async_get_pool_op(session *this_, urls::url_view url)
      : this_(this_), url(url),
        is_https((url.scheme_id() == urls::scheme::https) || (url.scheme_id() == urls::scheme::wss))
  {}

  std::shared_ptr<connection_pool> p;

  using completion_signature_type = void(system::error_code, std::shared_ptr<connection_pool>);
  using step_signature_type       = void(system::error_code);

  BOOST_REQUESTS_DECL
  std::shared_ptr<connection_pool> resume(requests::detail::faux_token_t<step_signature_type>  self, error_code ec);
};

template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::shared_ptr<connection_pool>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
session::async_get_pool(urls::url_view url, CompletionToken && completion_token)
{
  return detail::faux_run<async_get_pool_op>(std::forward<CompletionToken>(completion_token), this, url);
}


template<typename RequestBody>
auto session::ropen(
    beast::http::verb method,
    urls::url_view url,
    RequestBody && body,
    http::fields fields,
    system::error_code & ec) -> stream
{
  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                      || (url.scheme_id() == urls::scheme::wss);

  if (!is_secure && options_.enforce_tls)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  auto src = make_source(body);
  return ropen(method, url, fields, *src, ec);
}

struct session::async_ropen_op : asio::coroutine
{
  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  session * this_;

  http::verb method;

  urls::url url;
  struct request_options opts;
  core::string_view default_mime_type;

  system::error_code ec_;

  bool is_secure = (url.scheme_id() == urls::scheme::https)
                || (url.scheme_id() == urls::scheme::wss);

  response_base::history_type history;

  http::fields & headers;
  source & src;

  urls::url url_cache;

  async_ropen_op(session * this_,
                 http::verb method,
                 urls::url_view path,
                 http::fields & headers,
                 source & src)
      : this_(this_), method(method), url(path), opts(this_->options_), headers(headers), src(src)
  {
  }

  using completion_signature_type = void(system::error_code, stream);
  using step_signature_type       = void(system::error_code, variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream>);

  BOOST_REQUESTS_DECL auto resume(requests::detail::faux_token_t<step_signature_type> self,
              system::error_code & ec, variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream> s) -> stream;
};


struct session::async_ropen_op_body_base
{
  source_ptr source_impl;
  http::fields headers;

  template<typename RequestBody>
  async_ropen_op_body_base(
      container::pmr::polymorphic_allocator<void> alloc,
      RequestBody && body, http::fields headers)
      : source_impl(requests::make_source(std::forward<RequestBody>(body), alloc.resource())),
        headers(std::move(headers))
  {
  }
};

struct session::async_ropen_op_body : async_ropen_op_body_base, async_ropen_op
{
  template<typename RequestBody>
  async_ropen_op_body(
      container::pmr::polymorphic_allocator<void> alloc,
      session * this_,
      beast::http::verb method,
      urls::url_view path,
      RequestBody && body,
      http::fields req)
      : async_ropen_op_body_base{alloc, std::forward<RequestBody>(body), std::move(req)},
        async_ropen_op(this_, method, path, async_ropen_op_body_base::headers,
                       *this->source_impl)
  {}
};


template<typename RequestBody,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, basic_stream<Executor>))
session::async_ropen(beast::http::verb method,
                     urls::url_view path,
                     RequestBody && body,
                     http::fields req,
                     CompletionToken && completion_token)
{
  return detail::faux_run_with_allocator<async_ropen_op_body>(
                  std::forward<CompletionToken>(completion_token),
                  this, method, path, std::forward<RequestBody>(body), std::move(req));
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, basic_stream<Executor>))
session::async_ropen(urls::url_view url,
                     http::verb method,
                     urls::url_view path,
                     http::fields & headers,
                     source & src,
                     CompletionToken && completion_token)
{
  return detail::faux_run<async_ropen_op>(std::forward<CompletionToken>(completion_token),
                                        this, url, method, path, headers, src);
}

}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/impl/session.ipp>
#endif

#endif // BOOST_REQUESTS_IMPL_SESSION_HPP
