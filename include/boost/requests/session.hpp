// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_BASIC_SESSION_HPP
#define BOOST_REQUESTS_BASIC_SESSION_HPP

#include <boost/requests/connection_pool.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/string.hpp>
#include <boost/container/pmr/synchronized_pool_resource.hpp>


namespace boost
{
namespace requests
{
namespace detail
{
struct url_hash
{
  std::size_t operator()( urls::url_view url ) const
  {
    return boost::hash<urls::string_view>()( url.buffer() );
  }
};

}

struct session
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
      using other = session;
    };

    /// Constructor.
    explicit session(const executor_type &ex) : mutex_(ex)
    {
      sslctx_.set_verify_mode(asio::ssl::verify_peer);
      sslctx_.set_default_verify_paths();
    }

    /// Constructor.
    template<typename ExecutionContext>
    explicit session(ExecutionContext &context,
                     typename asio::constraint<
                                   asio::is_convertible<ExecutionContext &, asio::execution_context &>::value
                           >::type = 0)
            : mutex_(context.get_executor())
    {
      sslctx_.set_verify_mode(asio::ssl::verify_peer);
      sslctx_.set_default_verify_paths();
    }

    /// Get the executor associated with the object.
    executor_type get_executor() BOOST_ASIO_NOEXCEPT
    {
        return mutex_.get_executor();
    }

          struct request_options & options()       {return options_;}
    const struct request_options & options() const {return options_;}

    using request_type = http::fields;

    // possibly make it a distinct return type.
    BOOST_REQUESTS_DECL std::shared_ptr<connection_pool>
                                     get_pool(urls::url_view url, error_code & ec);
    std::shared_ptr<connection_pool> get_pool(urls::url_view url)
    {
      boost::system::error_code ec;
      auto res = get_pool(url, ec);
      if (ec)
        throw_exception(system::system_error(ec, "get_pool"));
      return res;
    }

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::shared_ptr<connection_pool>)) CompletionToken
                  BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
    async_get_pool(urls::url_view path,
                   CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

    void shutdown()
    {
      pools_.clear();
    }

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body,
               http::fields req,
               system::error_code & ec) -> stream;

    template<typename RequestBody>
    auto ropen(beast::http::verb method,
               urls::url_view path,
               RequestBody && body,
               http::fields req) -> stream
    {
      boost::system::error_code ec;
      auto res = ropen(method, path, std::move(body), std::move(req), ec);
      if (ec)
        throw_exception(system::system_error(ec, "ropen"));
      return res;
    }

    BOOST_REQUESTS_DECL auto ropen(
              beast::http::verb method,
              urls::url_view url,
              http::fields & headers,
              source & src,
              system::error_code & ec) -> stream;

    auto ropen(beast::http::verb method,
               urls::url_view url,
               http::fields & headers,
               source & src) -> stream
    {
      boost::system::error_code ec;
      auto res = ropen(method, url, headers, src, ec);
      if (ec)
        throw_exception(system::system_error(ec, "ropen"));
      return res;
    }

    template<typename RequestBody,
              BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, stream))
    async_ropen(beast::http::verb method,
                urls::url_view path,
                RequestBody && body,
                http::fields req,
                CompletionToken && completion_token);

    template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, stream)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                       void (boost::system::error_code, stream))
    async_ropen(urls::url_view url,
                http::verb method,
                urls::url_view path,
                http::fields & headers,
                source & src,
                CompletionToken && completion_token);

  private:
    asio::ssl::context sslctx_{asio::ssl::context_base::tls_client};
    detail::mutex mutex_;

    struct request_options options_{default_options()};

    boost::unordered_map<urls::url,
                         std::shared_ptr<connection_pool>,
                         detail::url_hash> pools_;

    // this isn't great
    boost::container::pmr::synchronized_pool_resource pmr_;
    cookie_jar jar_{boost::container::pmr::polymorphic_allocator<char>(&pmr_)};

    struct async_get_pool_op;

    struct async_ropen_op;

    struct async_ropen_op_body;

    struct async_ropen_op_body_base;

    BOOST_REQUESTS_DECL auto make_request_(http::fields fields) -> requests::request_parameters;
    BOOST_REQUESTS_DECL static urls::url normalize_(urls::url_view in);
};

template<typename Token>
struct session::defaulted : session
{
  using default_token = Token;
  using session::session;

  auto async_get_pool(urls::url_view path)
  {
    return session::async_get_pool(path, default_token());
  };

  using session::async_ropen;
  using session::async_get_pool;

  template<typename RequestBody,
            typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, stream))
  async_ropen(beast::http::verb method,
              urls::url_view path,
              RequestBody && body,
              http::fields req,
              CompletionToken && completion_token)
  {
    return session::async_ropen(method, path, std::forward<RequestBody>(body), std::move(req),
                                        detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, stream))
  async_ropen(urls::url_view url,
              http::verb method,
              urls::url_view path,
              http::fields & headers,
              source & src,
              CompletionToken && completion_token )
  {
    return session::async_ropen(url, method, path, headers, src,
                                detail::with_defaulted_token<Token>(std::forward<CompletionToken>(completion_token)));
  }

  template<typename RequestBody>
  auto async_ropen(beast::http::verb method,
                   urls::url_view path,
                   RequestBody && body,
                   http::fields req)
  {
    return async_ropen(method, path, std::forward<RequestBody>(body), std::move(req), default_token());
  }

  auto async_ropen(urls::url_view url,
                   http::verb method,
                   urls::url_view path,
                   http::fields & headers,
                   source & src)
  {
    return async_ropen(url, method, path, headers, src, default_token());
  }


};


}
}

#include <boost/requests/impl/session.hpp>

#endif //BOOST_REQUESTS_BASIC_SESSION_HPP
