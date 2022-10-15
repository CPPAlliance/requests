// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_CONNECTION_HPP
#define BOOST_REQUESTS_CONNECTION_HPP

#include "boost/requests/fields/keep_alive.hpp"
#include <boost/asem/guarded.hpp>
#include <boost/asem/st.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/requests/options.hpp>
#include <boost/requests/traits.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/url/url_view.hpp>

namespace boost
{
namespace requests
{

template<typename Stream>
struct basic_connection
{
    /// The type of the next layer.
    typedef typename std::remove_reference<Stream>::type next_layer_type;

    /// The type of the executor associated with the object.
    typedef typename next_layer_type::executor_type executor_type;

    /// The type of the executor associated with the object.
    typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

    /// Rebinds the socket type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_connection<typename next_layer_type::template rebind_executor<Executor1>::other> other;
    };

    /// Get the executor
    executor_type get_executor() noexcept
    {
        return next_layer_.get_executor();
    }
    /// Get the underlying stream
    const next_layer_type &next_layer() const noexcept
    {
        return next_layer_;
    }

    /// Get the underlying stream
    next_layer_type &next_layer() noexcept
    {
        return next_layer_;
    }

    /// The protocol-type of the lowest layer.
    using protocol_type = typename beast::lowest_layer_type<next_layer_type>::protocol_type;

    /// The endpoint of the lowest lowest layer.
    using endpoint_type = typename protocol_type::endpoint;

    /// Construct a stream.
    /**
     * @param arg The argument to be passed to initialise the underlying stream.
     *
     * Everything else will be default constructed
     */

    explicit basic_connection(const executor_type & ex) : next_layer_(ex) {}
    explicit basic_connection(const executor_type & ex, asio::ssl::context& ctx_) : next_layer_(ex, ctx_) {}

    template<typename Arg>
    explicit basic_connection(Arg && arg) : next_layer_(std::forward<Arg>(arg)) {}


    template<typename NextLayer>
    explicit basic_connection(basic_connection<NextLayer> && prev) : next_layer_(std::move(prev.next_layer())) {}


    template <typename ExecutionContext>
    explicit basic_connection(ExecutionContext& context,
                    typename asio::constraint<
                            asio::is_convertible<ExecutionContext&, asio::execution_context&>::value
                    >::type = 0)
            : next_layer_(context)
    {
    }

    template <typename ExecutionContext>
    explicit basic_connection(ExecutionContext& context,
                    asio::ssl::context& ctx_,
                    typename asio::constraint<
                            is_ssl_stream<next_layer_type>::value &&
                            asio::is_convertible<ExecutionContext&, asio::execution_context&>::value
                    >::type = 0)
            : next_layer_(context, ctx_)
    {
    }

    void connect(endpoint_type ep)
    {
      boost::system::error_code ec;
      connect(ep, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    void connect(endpoint_type ep,
                 system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_connect(endpoint_type ep,
                  CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));


    void close()
    {
      boost::system::error_code ec;
      close(ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    void close(system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken
                 BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_close(CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    template<typename RequestBody, typename RequestAllocator,
             typename ResponseBody, typename ResponseAllocator>
    void single_request(
            beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> & req,
            beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & res)
    {
      boost::system::error_code ec;
      single_request(req, res, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }

    template<typename RequestBody, typename RequestAllocator,
              typename ResponseBody, typename ResponseAllocator>
    void single_request(
        beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> &req,
        beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & res,
        system::error_code & ec);

    template<typename RequestBody, typename RequestAllocator,
             typename ResponseBody, typename ResponseAllocator,
             BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken
             BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_single_request(beast::http::request<RequestBody, beast::http::basic_fields<RequestAllocator>> &req,
                         beast::http::response<ResponseBody, beast::http::basic_fields<ResponseAllocator>> & res,
                         CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    bool is_open() const
    {
      return beast::get_lowest_layer(next_layer_).is_open();
    }

    // Endpoint
    const endpoint_type & endpoint() const {return endpoint_;}

    // Timeout of the connection-alive
    std::chrono::system_clock::time_point timeout() const
    {
      return keep_alive_set_.timeout;
    }

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

    void set_host(core::string_view sv, system::error_code & ec);

    const std::string & host() const {return host_;}
  private:

    Stream next_layer_;
    asem::basic_mutex<boost::asem::st, executor_type>
            read_mtx_{next_layer_.get_executor()},
            write_mtx_{next_layer_.get_executor()};

    std::string host_;
    beast::flat_buffer buffer_;
    std::size_t ongoing_requests_{0u};
    keep_alive keep_alive_set_;
    endpoint_type endpoint_;

    struct async_close_op;
    struct async_connect_op;
    template<typename RequestBody, typename RequestAllocator,
             typename ResponseBody, typename ResponseAllocator>
    struct async_single_request_op;
};

template<typename Executor = asio::any_io_executor>
using basic_http_connection  = basic_connection<asio::basic_stream_socket<asio::ip::tcp, Executor>>;

template<typename Executor = asio::any_io_executor>
using basic_https_connection = basic_connection<asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>>>;


using http_connection  = basic_http_connection<>;
using https_connection = basic_https_connection<>;

}
}

#include <boost/requests/impl/connection.hpp>

#endif //BOOST_REQUESTS_CONNECTION_HPP
