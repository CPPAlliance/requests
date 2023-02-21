// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_DETAIL_CONNECTION_IMPL_HPP
#define BOOST_REQUESTS_DETAIL_CONNECTION_IMPL_HPP

#include <boost/requests/fields/keep_alive.hpp>
#include <boost/requests/redirect.hpp>
#include <boost/requests/request_options.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/response.hpp>
#include <boost/requests/source.hpp>
#include <boost/requests/detail/defaulted.hpp>
#include <boost/requests/detail/faux_coroutine.hpp>
#include <boost/requests/detail/lock_guard.hpp>
#include <boost/requests/detail/ssl.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/url/url_view.hpp>

namespace boost {
namespace requests {

struct stream;

namespace detail {

struct connection_impl : std::enable_shared_from_this<connection_impl>
{
    typedef asio::ssl::stream<asio::generic::stream_protocol::socket> next_layer_type;
    typedef typename next_layer_type::executor_type executor_type;
    typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

    executor_type get_executor() noexcept
    {
        return next_layer_.get_executor();
    }
    const next_layer_type &next_layer() const noexcept
    {
        return next_layer_;
    }

    next_layer_type &next_layer() noexcept
    {
        return next_layer_;
    }

    using protocol_type = asio::generic::stream_protocol;
    using endpoint_type = typename protocol_type::endpoint;

    connection_impl(connection_impl && lhs) = delete;
    connection_impl & operator=(connection_impl && lhs) = delete;
    template<typename ExecutorOrContext>
    explicit connection_impl(ExecutorOrContext && exec_or_ctx, asio::ssl::context & ctx)
        : next_layer_(std::forward<ExecutorOrContext>(exec_or_ctx), ctx), use_ssl_{true} {}

    template<typename ExecutionContext>
    explicit connection_impl(ExecutionContext &context,
                        typename asio::constraint<
                            asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                        >::type = 0)
        : next_layer_(
            context,
            asio::use_service<detail::ssl_context_service>(context).get()) {}

    explicit connection_impl(asio::any_io_executor exec)
      : next_layer_(
      exec,
      asio::use_service<detail::ssl_context_service>(
          asio::query(exec, asio::execution::context)
          ).get()) {}

    void connect(endpoint_type ep)
    {
      boost::system::error_code ec;
      connect(ep, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    BOOST_REQUESTS_DECL void connect(endpoint_type ep,
                                     system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,  void (boost::system::error_code))
    async_connect(endpoint_type ep,
                  CompletionToken && completion_token);

    void close()
    {
      boost::system::error_code ec;
      close(ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    BOOST_REQUESTS_DECL void close(system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_close(CompletionToken && completion_token);

    bool is_open() const
    {
      return beast::get_lowest_layer(next_layer_).is_open();
    }

    // Endpoint
    endpoint_type endpoint() const {return endpoint_;}

    std::size_t working_requests() const { return ongoing_requests_; }

    // Reserve memory for the internal buffer.
    void reserve(std::size_t size)
    {
      buffer_.reserve(size);
    }

    void set_host(core::string_view sv)
    {
      boost::system::error_code ec;
      set_host(sv, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    BOOST_REQUESTS_DECL void set_host(core::string_view sv, system::error_code & ec);
    core::string_view host() const {return host_;}
    constexpr static redirect_mode supported_redirect_mode() {return redirect_mode::endpoint;}

    using request_type = request_parameters;

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body, request_parameters req,
               system::error_code & ec) -> stream;

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body, request_parameters req) -> stream;

    BOOST_REQUESTS_DECL
    auto ropen(beast::http::verb method,
               urls::pct_string_view path,
               http::fields & headers,
               source & src,
               request_options opt,
               cookie_jar * jar,
               system::error_code & ec) -> stream;

    BOOST_REQUESTS_DECL
    auto ropen(beast::http::verb method,
               urls::pct_string_view path,
               http::fields & headers,
               source & src,
               request_options opt,
               cookie_jar * jar) -> stream;

    template<typename RequestBody,
             typename CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, stream))
    async_ropen(beast::http::verb method,
                urls::url_view path,
                RequestBody && body, request_parameters req,
                CompletionToken && completion_token);

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
                CompletionToken && completion_token);
    bool uses_ssl() const {return use_ssl_;}
    void use_ssl(bool use_ssl = true) {use_ssl_ = use_ssl;}
  private:

    next_layer_type next_layer_;
    bool use_ssl_{true};
    detail::mutex read_mtx_{next_layer_.get_executor()},
                 write_mtx_{next_layer_.get_executor()};

    std::string host_;
    beast::flat_buffer buffer_;
    std::atomic<std::size_t> ongoing_requests_{0u};
    endpoint_type endpoint_;

    struct async_close_op;
    struct async_connect_op;

    struct async_ropen_op;

    struct async_ropen_op_body;

    struct async_ropen_op_body_base;

    BOOST_REQUESTS_DECL std::size_t do_read_some_(beast::http::basic_parser<false> & parser);
    BOOST_REQUESTS_DECL std::size_t do_read_some_(beast::http::basic_parser<false> & parser, system::error_code & ec) ;
    BOOST_REQUESTS_DECL void do_async_read_some_(beast::http::basic_parser<false> & parser, detail::faux_token_t<void(system::error_code, std::size_t)>) ;
    BOOST_REQUESTS_DECL void do_async_close_(detail::faux_token_t<void(system::error_code)>);
    BOOST_REQUESTS_DECL void do_close_(system::error_code & ec);

    friend struct boost::requests::stream;
};


}
}
}

#include <boost/requests/detail/impl/connection_impl.hpp>

#endif //BOOST_REQUESTS_DETAIL_CONNECTION_IMPL_HPP
