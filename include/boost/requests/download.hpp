//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DOWNLOAD_HPP
#define BOOST_REQUESTS_DOWNLOAD_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/faux_coroutine.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/service.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/url/url_view.hpp>

#if defined(BOOST_ASIO_HAS_FILE)
#include <boost/asio/basic_stream_file.hpp>
#else
#include <boost/beast/core/file.hpp>
#endif


namespace boost
{
namespace requests
{

#if defined(BOOST_ASIO_HAS_FILE)

template<typename Stream>
std::size_t write_to_file(Stream && str, const filesystem::path & file, system::error_code & ec)
{
  asio::basic_stream_file<typename std::decay_t<Stream>::executor_type> f{str.get_executor()};
  f.open(file.string().c_str(), asio::file_base::write_only | asio::file_base::create, ec);
  if (ec)
    return 0u;

  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  std::size_t written = 0;

  while (!str.done() && !ec)
  {
    system::error_code ec_read;
    std::size_t n = str.read_some(asio::buffer(buffer), ec_read);

    if (n == 0 && ec_read)
    {
      ec = ec_read;
      return written;
    }

    written += f.write_some(asio::buffer(buffer, n), ec);

    if (ec_read && !ec)
      ec = ec_read;
  }

  return written;
}


namespace detail
{


template<typename Stream>
struct async_write_to_file_op : asio::coroutine
{
  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);

  Stream str;
  asio::basic_stream_file<typename std::decay_t<Stream>::executor_type>  f{str.get_executor()};
  const std::filesystem::path & file;
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  std::size_t written = 0u;
  system::error_code ec_read;

  template<typename Stream_>
  async_write_to_file_op(Stream_ && str, const std::filesystem::path & pt)
      : str(std::forward<Stream>(str)), file(pt)
  {
  }

  std::size_t resume(requests::detail::faux_token_t<step_signature_type> self,
                     system::error_code & ec, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      f.open(file.string().c_str(), asio::file_base::write_only | asio::file_base::create, ec);
      if (ec)
        return 0u;

      while (!str.done() && !ec)
      {
        // KDM: this could be in parallel to write using parallel_group.
        BOOST_ASIO_CORO_YIELD {
          auto b = asio::buffer(buffer);
          str.async_read_some(b, std::move(self));
        }

        if (n == 0 && ec)
          return written;

        ec_read = exchange(ec, {});
        BOOST_ASIO_CORO_YIELD asio::async_write(f, asio::buffer(buffer, n), std::move(self));

        written += n;
        if (ec_read && !ec)
          ec = ec_read;
      }
    }
    return written;
  }
};

}

#else

template<typename Stream>
std::size_t write_to_file(Stream && str, const filesystem::path & file, system::error_code & ec)
{
  beast::file f;
  f.open(file.string().c_str(), beast::file_mode::write_new, ec);
  if (ec)
    return 0u;

  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  std::size_t written = 0;

  while (!str.done() && !ec)
  {
    system::error_code ec_read;
    std::size_t n = str.read_some(asio::buffer(buffer), ec_read);

    if (n == 0 && ec_read)
    {
      ec = ec_read;
      return written;
    }

    written += f.write(buffer, n, ec);

    if (ec_read && !ec)
      ec = ec_read;
  }

  return written;
}

namespace detail
{


template<typename Stream>
struct async_write_to_file_op : asio::coroutine
{
  using executor_type = typename std::decay_t<Stream>::executor_type;
  executor_type get_executor() {return str.get_executor(); }

  using completion_signature_type = void(system::error_code, std::size_t);
  using step_signature_type       = void(system::error_code, std::size_t);

  Stream &str;
  beast::file f;
  const filesystem::path & file;
  char buffer[BOOST_REQUESTS_CHUNK_SIZE];

  std::size_t written = 0u;

  async_write_to_file_op(Stream * str, const filesystem::path & pt) : str(*str), file(pt) {}

  std::size_t resume(requests::detail::faux_token_t<step_signature_type> self,
                     system::error_code & ec, std::size_t n = 0u)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      f.open(file.string().c_str(), beast::file_mode::write_new, ec);
      if (ec)
        return 0u;

      while (!ec && !str.done())
      {
        BOOST_ASIO_CORO_YIELD str.async_read_some(asio::buffer(buffer), std::move(self));

        if (n == 0 && ec)
          return written;

        system::error_code ec_read = exchange(ec, {});
        written += f.write(buffer, n, ec);

        if (ec_read && !ec)
          ec = ec_read;
      }
    }
    return written;
  }
};

}



#endif

template<typename Stream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::size_t)) CompletionToken
              BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename Stream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, std::size_t))
async_write_to_file(Stream & str, const filesystem::path & file,
                    CompletionToken && completion_token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN( typename Stream::executor_type))
{
  return requests::detail::faux_run<
      detail::async_write_to_file_op<Stream>>(std::forward<CompletionToken>(completion_token), &str, file);
}

template<typename Stream>
std::size_t write_to_file(Stream && str, const filesystem::path & file)
{
  boost::system::error_code ec;
  auto res = write_to_file(std::forward<Stream>(str), file, ec);
  if (ec)
    throw_exception(system::system_error(ec, "write_to_file"));
  return res;
}

struct download_response : response_base
{
  filesystem::path download_path;

  download_response(allocator_type allocator = {}) : response_base(allocator) {}

  download_response(http::response_header header, response_base::history_type history, filesystem::path file) : response_base(std::move(header), std::move(history)), download_path(std::move(file)) {}
  download_response(response_base         header, filesystem::path file) : response_base(std::move(header)), download_path(std::move(file)) {}

  download_response(const download_response & ) = default;
  download_response(download_response && ) noexcept = default;

  download_response& operator=(const download_response & ) = default;
  download_response& operator=(download_response && ) noexcept = default;
};


template<typename Connection>
inline auto download(Connection & conn,
                     urls::url_view target,
                     typename Connection::request_type req,
                     filesystem::path download_path,
                     system::error_code & ec) -> download_response
{
  auto ro = conn.ropen(http::verb::get, target, empty{}, std::move(req), ec);
  if (ec)
    return download_response{std::move(ro).headers(), std::move(ro).history(), {}};

  if (filesystem::exists(download_path, ec) && filesystem::is_directory(download_path, ec) && !target.empty())
    download_path /= target.segments().back(); // so we can download to a folder
  ec.clear();
  if (!ec)
    write_to_file(ro, download_path, ec);
  return download_response{std::move(ro).headers(), std::move(ro).history(), std::move(download_path)};
}


template<typename Connection>
inline auto download(Connection & conn,
                     urls::url_view target,
                     typename Connection::request_type req,
                     filesystem::path download_path) -> download_response
{
  boost::system::error_code ec;
  auto res = download(conn, target, std::move(req), std::move(download_path), ec);
  if (ec)
    throw_exception(system::system_error(ec, "download"));
  return res;
}

inline auto download(urls::url_view path,
                     http::fields req,
                     filesystem::path download_path,
                     system::error_code & ec) -> download_response
{
  return download(default_session(), path, std::move(req), std::move(download_path), ec);
}


inline auto download(urls::url_view path,
                     http::fields req,
                     filesystem::path download_path) -> download_response
{
  return download(default_session(), path, std::move(req), std::move(download_path));
}


namespace detail
{

template<typename Connection>
struct async_download_op : asio::coroutine
{
  using executor_type = typename Connection::executor_type;
  executor_type get_executor() {return conn.get_executor(); }

  Connection & conn;
  urls::url_view target;
  typename Connection::request_type req;
  filesystem::path download_path;

  async_download_op(Connection * conn,
                    urls::url_view target,
                    typename Connection::request_type req,
                    filesystem::path download_path)
      : conn(*conn), target(target), req(std::move(req)), download_path(std::move(download_path)) {}

  download_response rb{req.get_allocator()};
  optional<stream> str_;

  using completion_signature_type = void(system::error_code, download_response);
  using step_signature_type       = void(system::error_code, optional<stream>);

  download_response & resume(requests::detail::faux_token_t<step_signature_type> self,
                          system::error_code & ec,
                          optional<stream> s = none)
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_ASIO_CORO_YIELD conn.async_ropen(http::verb::get, target, empty{}, std::move(req), std::move(self));
      if (ec)
      {
        rb.history = std::move(*s).history();
        rb.headers = std::move(*s).headers();
        return rb;
      }
      str_ = std::move(s);
      if (filesystem::exists(download_path, ec) && filesystem::is_directory(download_path, ec) && !target.segments().empty())
        rb.download_path = download_path / target.segments().back(); // so we can download to a folder
      else
        rb.download_path = std::move(download_path);
      ec.clear();
      if (!ec)
      {
        BOOST_ASIO_CORO_YIELD async_write_to_file(*str_, rb.download_path,
                          asio::deferred([](system::error_code ec, std::size_t){return asio::deferred.values(ec);}))
                (std::move(self));
      }

      rb.history = std::move(*str_).history();
      rb.headers = std::move(*str_).headers();
    }
    return rb;
  }
};

}


template<typename Connection,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, download_response)) CompletionToken
              = typename Connection::default_token>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, download_response))
async_download(Connection & conn,
               urls::url_view target,
               typename Connection::request_type req,
               filesystem::path download_path,
               CompletionToken && completion_token = typename Connection::default_token())
{
  return detail::faux_run<detail::async_download_op<Connection>>(
          std::forward<CompletionToken>(completion_token),
          &conn, target, std::move(req), std::move(download_path));
}

namespace detail
{

struct async_download_op_free
{
  template<typename Handler>
  void operator()(Handler && handler,
                  urls::url_view path,
                  http::fields req,
                  filesystem::path download_path)
  {
    return async_download(default_session(asio::get_associated_executor(handler)),
                          path, std::move(req), std::move(download_path), std::move(handler));
  }
};

}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, download_response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code, download_response))
async_download(urls::url_view path,
               http::fields req,
               filesystem::path download_path,
               CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken,
                              void (boost::system::error_code, download_response)>(
      detail::async_download_op_free{},
      completion_token, path, std::move(req), std::move(download_path));
}


}
}


#endif // BOOST_REQUESTS_DOWNLOAD_HPP
