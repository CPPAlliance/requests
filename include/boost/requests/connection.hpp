//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_CONNECTION_HPP
#define BOOST_REQUESTS_CONNECTION_HPP

#include <boost/requests/detail/connection_impl.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/stream.hpp>


namespace boost {
namespace requests {
struct stream;

struct connection
{
  /// The type of the next layer.
  typedef asio::ssl::stream<asio::generic::stream_protocol::socket> next_layer_type;

  /// The type of the executor associated with the object.
  typedef typename next_layer_type::executor_type executor_type;

  /// The type of the executor associated with the object.
  typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

  /// This type with a defaulted completion token.
  template<typename Token>
  struct defaulted;

  /// Rebinds the socket type to another executor.
  template<typename Executor1, typename = void>
  struct rebind_executor
  {
    /// The socket type when rebound to the specified executor.
    using other = connection;
  };

  /// Get the executor
  executor_type get_executor() noexcept
  {
    return impl_->get_executor();
  }
  /// Get the underlying stream
  const next_layer_type &next_layer() const noexcept
  {
    return impl_->next_layer();
  }

  /// Get the underlying stream
  next_layer_type &next_layer() noexcept
  {
    return impl_->next_layer();
  }

  /// The protocol-type of the lowest layer.
  using protocol_type = asio::generic::stream_protocol;

  /// The endpoint of the lowest lowest layer.
  using endpoint_type = typename protocol_type::endpoint;


  explicit connection(std::shared_ptr<detail::connection_impl> impl) : impl_(std::move(impl)) {}

  connection() = default;
  connection(const connection & lhs) = default;
  connection & operator=(const connection & lhs) = default;

  connection(connection && lhs) = default;
  connection & operator=(connection && lhs) = default;

  /// Construct a stream.
  template<typename ExecutorOrContext>
  explicit connection(ExecutorOrContext && exec_or_ctx, asio::ssl::context & ctx)
      : impl_(std::make_shared<detail::connection_impl>(std::forward<ExecutorOrContext>(exec_or_ctx), ctx)) {}

  template<typename ExecutionContext>
  explicit connection(ExecutionContext &context,
                      typename asio::constraint<
                          asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                          >::type = 0)
      : impl_(std::make_shared<detail::connection_impl>(context)) {}

  explicit connection(asio::any_io_executor exec) : impl_(std::make_shared<detail::connection_impl>(exec)) {}

  void connect(endpoint_type ep)
  {
    boost::system::error_code ec;
    connect(ep, ec);
    if (ec)
      urls::detail::throw_system_error(ec);
  }

  BOOST_REQUESTS_DECL void connect(endpoint_type ep, system::error_code & ec)
  {
    return impl_->connect(ep, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code))
  async_connect(endpoint_type ep,
                CompletionToken && completion_token)

  {
    return impl_->async_connect(ep, std::forward<CompletionToken>(completion_token));
  }

  void close()
  {
    boost::system::error_code ec;
    close(ec);
    if (ec)
      urls::detail::throw_system_error(ec);
  }

  BOOST_REQUESTS_DECL void close(system::error_code & ec)
  {
    return impl_->close(ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_coe))
  async_close(CompletionToken && completion_token)
  {
    return impl_->async_close(std::forward<CompletionToken>(completion_token));
  }

  bool is_open() const { return impl_ && impl_->is_open(); }
  endpoint_type endpoint() const { return impl_->endpoint(); }


  std::size_t working_requests() const { return impl_->working_requests(); }
  void reserve(std::size_t size) { impl_->reserve(size); }

  void set_host(core::string_view sv)
  {
    boost::system::error_code ec;
    set_host(sv, ec);
    if (ec)
      urls::detail::throw_system_error(ec);
  }

  BOOST_REQUESTS_DECL void set_host(core::string_view sv, system::error_code & ec)
  {
    impl_->set_host(sv, ec);
  }
  core::string_view host() const {return impl_->host();}
  constexpr static redirect_mode supported_redirect_mode() {return redirect_mode::endpoint;}

  using request_type = request_parameters;

  template<typename RequestBody>
  auto ropen(beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             request_parameters req,
             system::error_code & ec) -> stream
  {
    return impl_->ropen(method, path, std::forward<RequestBody>(body), std::move(req), ec);
  }

  template<typename RequestBody>
  auto ropen(beast::http::verb method,
             urls::url_view path,
             RequestBody && body,
             request_parameters req) -> stream
  {
    return impl_->ropen(method, path, std::forward<RequestBody>(body), std::move(req));
  }

  BOOST_REQUESTS_DECL
  auto ropen(beast::http::verb method,
             urls::pct_string_view path,
             http::fields & headers,
             source & src,
             request_options opt,
             cookie_jar * jar,
             system::error_code & ec) -> stream
  {
    return impl_->ropen(method, path, headers, src, std::move(opt), jar, ec);
  }

  BOOST_REQUESTS_DECL
  auto ropen(beast::http::verb method,
             urls::pct_string_view path,
             http::fields & headers,
             source & src,
             request_options opt,
             cookie_jar * jar) -> stream
  {
    return impl_->ropen(method, path, headers, src, std::move(opt), jar);
  }

  template<typename RequestBody,
            typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code, stream))
  async_ropen(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              request_parameters req,
              CompletionToken && completion_token)
  {
    return impl_->async_ropen(method, path, std::forward<RequestBody>(body),
                             std::move(req), std::forward<CompletionToken>(completion_token));
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                     void (boost::system::error_code,
                                          stream))
  async_ropen(beast::http::verb method,
              urls::pct_string_view path,
              http::fields & headers,
              source & src,
              request_options opt,
              cookie_jar * jar,
              CompletionToken && completion_token)
  {
    return impl_->async_ropen(method, path, headers, src, std::move(opt), jar, std::forward<CompletionToken>(completion_token));
  }
  bool uses_ssl() const {return impl_->uses_ssl();}
  void use_ssl(bool use_ssl = true) {impl_->use_ssl(use_ssl);}

  operator bool() const {return impl_ != nullptr;}
private:
  std::shared_ptr<detail::connection_impl> impl_;

  friend struct stream;
};


template <typename Executor1>
struct connection::rebind_executor<Executor1, void_t<typename Executor1::default_completion_token_type>>
{
  using other = defaulted<typename Executor1::default_completion_token_type>;
};

namespace detail
{
template<typename Stream, typename Token>
struct defaulted_helper
{
  using type = typename Stream::template defaulted<Token>;
};

}


template<typename Token>
struct connection::defaulted : connection
{
  using default_token = Token;
  using connection::connection;
  defaulted(connection && lhs) :  connection(std::move(lhs)) {}
  using connection::async_connect;
  using connection::async_close;

  auto async_connect(endpoint_type ep)
  {
    return connection::async_connect(std::move(ep), default_token());
  }
  auto async_close()
  {
    return connection::async_close(default_token());
  }

  template<typename RequestBody,
            typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, typename detail::defaulted_helper<stream, Token>::type))
  async_ropen(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              request_parameters req,
              CompletionToken && completion_token)
  {
    return connection::async_ropen(method, path, std::forward<RequestBody>(body), std::move(req),
                                   detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, typename detail::defaulted_helper<stream, Token>::type))
  async_ropen(beast::http::verb method,
              urls::pct_string_view path,
              http::fields & headers,
              source & src,
              request_options opt,
              cookie_jar * jar,
              CompletionToken && completion_token )
  {
    return connection::async_ropen(method, path, headers, src, std::move(opt), jar,
                                   detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename RequestBody>
  auto async_ropen(beast::http::verb method,
                   urls::url_view path,
                   RequestBody && body,
                   request_parameters req)
  {
    return this->async_ropen(method, path, std::forward<RequestBody>(body), std::move(req), default_token());
  }

  template<typename RequestBody>
  auto async_ropen(http::request<RequestBody> & req,
                   request_options opt,
                   cookie_jar * jar = nullptr)
  {
    return this->async_ropen(req, std::move(opt), jar, default_token());
  }

};

}
}


#endif // BOOST_REQUESTS_CONNECTION_HPP
