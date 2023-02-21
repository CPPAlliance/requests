// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_CONNECTION_POOL_HPP

#include <boost/requests/connection.hpp>

namespace boost {
namespace requests {

namespace detail {

struct endpoint_hash
{
  std::size_t operator()(const asio::generic::stream_protocol::endpoint & be) const
  {
    return hash_range(reinterpret_cast<const char*>(be.data()), // yuk
                      reinterpret_cast<const char*>(be.data()) + be.size());
  }
};


}

struct connection_pool
{
    /// The type of the executor associated with the object.
    typedef asio::any_io_executor executor_type;

    /// This type with a defaulted completion token.
    template<typename Token>
    struct defaulted;

    /// Rebinds the socket type to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        using other = connection_pool;
    };

    /// Get the executor
    executor_type get_executor() noexcept
    {
        return mutex_.get_executor();
    }

    /// The protocol-type of the lowest layer.
    using protocol_type = asio::generic::stream_protocol;

    /// The endpoint of the lowest lowest layer.
    using endpoint_type = typename protocol_type::endpoint;

    /// Construct a stream.
    /**
     * @param exec The executor or execution_context.
     *
     * Everything else will be default constructed
     */
    explicit connection_pool(asio::any_io_executor exec,
                             std::size_t limit = BOOST_REQUESTS_DEFAULT_POOL_SIZE)
        : context_(
              asio::use_service<detail::ssl_context_service>(
                  asio::query(exec, asio::execution::context)
              ).get()), mutex_(exec), limit_(limit) {}

    template<typename ExecutionContext>
    explicit connection_pool(ExecutionContext &context,
                             typename asio::constraint<
                                 asio::is_convertible<ExecutionContext &, asio::execution_context &>::value,
                                 std::size_t
                             >::type limit = BOOST_REQUESTS_DEFAULT_POOL_SIZE)
        : context_(
              asio::use_service<detail::ssl_context_service>(context).get()),
              mutex_(context), limit_(limit) {}

    /// Construct a stream.
    /**
     * @param exec The executor or execution_context.
     *
     * Everything else will be default constructed
     */
    template<typename Exec>
    explicit connection_pool(Exec && exec,
                             asio::ssl::context & ctx,
                             std::size_t limit = BOOST_REQUESTS_DEFAULT_POOL_SIZE)
        : context_(ctx), mutex_(std::forward<Exec>(exec)), limit_(limit) {}

    /// Move constructor
    connection_pool(connection_pool && ) = default;

    void lookup(urls::url_view av)
    {
      boost::system::error_code ec;
      lookup(av, ec);
      if (ec)
        urls::detail::throw_system_error(ec);
    }
    BOOST_REQUESTS_DECL void lookup(urls::url_view sv, system::error_code & ec);

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code))
    async_lookup(urls::url_view av,
                 CompletionToken && completion_token );


    std::size_t limit() const {return limit_;}
    std::size_t active() const {return conns_.size();}

    using request_type = request_parameters;

    BOOST_REQUESTS_DECL connection get_connection(error_code & ec);
    connection get_connection()
    {
      boost::system::error_code ec;
      auto res = get_connection(ec);
      if (ec)
        throw_exception(system::system_error(ec, "get_connection"));
      return res;
    }

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
      async_get_connection(CompletionToken && completion_token);

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body, request_parameters req,
               system::error_code & ec) -> stream
    {
      auto conn = get_connection(ec);
      if (!ec && !conn )
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
      if (ec)
        return stream{get_executor(), nullptr};

      BOOST_ASSERT(conn);
      return conn.ropen(method, path, std::forward<RequestBody>(body), std::move(req), ec);
    }

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body, request_parameters req) -> stream
    {
      boost::system::error_code ec;
      auto res = ropen(method, path, std::forward<RequestBody>(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "open"));
      return res;
    }

    auto ropen(beast::http::verb method,
               urls::pct_string_view path,
               http::fields & headers,
               source & src,
               request_options opt,
               cookie_jar * jar,
               system::error_code & ec) -> stream
    {
      auto conn = get_connection(ec);
      if (!ec && !conn)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
      if (ec)
        return stream{get_executor(), nullptr};

      BOOST_ASSERT(conn);
      return conn.ropen(method, path, headers, src, opt, jar, ec);
    }

    auto ropen(beast::http::verb method,
               urls::pct_string_view path,
               http::fields & headers,
               source & src,
               request_options opt,
               cookie_jar * jar) -> stream
    {
      boost::system::error_code ec;
      auto res = ropen(method, path, headers, src, opt, jar, ec);
      if (ec)
        throw_exception(system::system_error(ec, "open"));
      return res;
    }


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
                                       void (boost::system::error_code, stream))
    async_ropen(beast::http::verb method,
                urls::pct_string_view path,
                http::fields & headers,
                source & src,
                request_options opt,
                cookie_jar * jar,
                CompletionToken && completion_token);

    bool uses_ssl() const {return use_ssl_;}
  private:
    bool use_ssl_{true};
    asio::ssl::context & context_;
    detail::mutex mutex_;
    std::string host_;
    std::vector<endpoint_type> endpoints_;
    std::size_t limit_;

    boost::unordered_multimap<endpoint_type,
                              std::shared_ptr<detail::connection_impl>,
                              detail::endpoint_hash> conns_;

    struct async_lookup_op;
    struct async_get_connection_op;

    struct async_ropen_op_body;

    struct async_ropen_op_body_base;

    struct async_ropen_op;
};

template<typename Token>
struct connection_pool::defaulted : connection_pool
{
  using default_token = Token;
  using connection_pool::connection_pool;
  using stream     = typename requests::stream::defaulted<Token>;
  using connection = typename requests::connection::defaulted<Token>;

  using connection_pool::async_lookup;
  using connection_pool::async_get_connection;

  auto async_lookup(urls::url_view av)
  {
    return connection_pool::async_lookup(av, default_token());
  }


  auto
  async_get_connection()
  {
    return async_get_connection(default_token());
  }

  template<typename RequestBody,
            typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, stream))
  async_ropen(beast::http::verb method,
              urls::url_view path,
              RequestBody && body, request_parameters req,
              CompletionToken && completion_token)
  {
    return connection_pool::async_ropen(method, path, std::forward<RequestBody>(body), std::move(req),
                                        detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, stream))
  async_ropen(beast::http::verb method,
              urls::pct_string_view path,
              http::fields & headers,
              source & src,
              request_options opt,
              cookie_jar * jar,
              CompletionToken && completion_token )
  {
    return connection_pool::async_ropen(method, path, headers, src, std::move(opt), jar,
                                        detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename RequestBody>
  auto async_ropen(beast::http::verb method,
                   urls::url_view path,
                   RequestBody && body, request_parameters req)
  {
    return async_ropen(method, path, std::forward<RequestBody>(body), std::move(req), default_token());
  }

  template<typename RequestBody>
  auto async_ropen(http::request<RequestBody> & req,
                   request_options opt,
                   cookie_jar * jar = nullptr)
  {
    return async_ropen(req, std::move(opt), jar, default_token());
  }

};


}
}

#include <boost/requests/impl/connection_pool.hpp>

#endif //BOOST_REQUESTS_CONNECTION_POOL_HPP
