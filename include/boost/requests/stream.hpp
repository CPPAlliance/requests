//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// this is on purpose
#ifndef BOOST_REQUESTS_STREAM_HPP

#define BOOST_REQUESTS_STREAM_HPP

#include <boost/requests/detail/connection_impl.hpp>
#include <boost/requests/detail/lock_guard.hpp>
#include <boost/asio/execution/bad_executor.hpp>
#include <boost/beast/http/basic_parser.hpp>
#include <boost/requests/detail/config.hpp>
#include <boost/requests/detail/faux_coroutine.hpp>
#include <boost/requests/detail/pmr.hpp>
#include <boost/requests/detail/tracker.hpp>
#include <boost/requests/fields/keep_alive.hpp>
#include <boost/requests/http.hpp>

namespace boost
{
namespace requests
{

struct stream
{
  /// The type of the executor associated with the object.
  typedef asio::any_io_executor executor_type;

  /// Get the executor
  executor_type get_executor() noexcept
  {
    return executor_;
  }

  /// This type with a defaulted completion token.
  template<typename Token>
  struct defaulted;

  /// Rebinds the socket type to another executor.
  template<typename Executor1, typename = void>
  struct rebind_executor
  {
    /// The socket type when rebound to the specified executor.
    using other = defaulted<Executor1>;
  };


  /// Check if the underlying connection is open.
  bool is_open() const
  {
    return impl_ && impl_->is_open() && !done();
  }

  /// Read some data from the request body.
  template<typename MutableBuffer>
  std::size_t read_some(const MutableBuffer & buffer)
  {
    boost::system::error_code ec;
    auto res = read_some(buffer, ec);
    if (ec)
      throw_exception(system::system_error(ec, "read_some"));
    return res;
  }

  /// Read some data from the request body.
  template<typename MutableBuffer>
  std::size_t read_some(const MutableBuffer & buffer, system::error_code & ec);

  /// Read some data from the request body.
  template<
      typename MutableBufferSequence,
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
  async_read_some(
      const MutableBufferSequence & buffers,
      CompletionToken && token);

  /// Read all the data from the request body.
  template<typename DynamicBuffer>
  std::size_t read(DynamicBuffer & buffer)
  {
    boost::system::error_code ec;
    auto res = read(buffer, ec);
    if (ec)
      throw_exception(system::system_error(ec, "read"));
    return res;
  }


  /// Read all the data from the request body.
  template<typename DynamicBuffer>
  std::size_t read(DynamicBuffer & buffer, system::error_code & ec);

  /// Read all the data from the request body.
  template<
      typename DynamicBuffer,
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::size_t)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::size_t))
  async_read(
      DynamicBuffer & buffers,
      CompletionToken && token);


  /// dump the rest of the data.
  void dump()
  {
    boost::system::error_code ec;
    dump(ec);
    if (ec)
      throw_exception(system::system_error(ec, "dump"));
  }
  BOOST_REQUESTS_DECL void dump(system::error_code & ec);

  /// Read some data from the request body.
  template<
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
          CompletionToken >
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code))
  async_dump(CompletionToken && token);

  stream(stream && lhs) = default;
  stream& operator=(stream && lhs) = default;

  stream           (const stream &) = delete;
  stream& operator=(const stream &) = delete;
  BOOST_REQUESTS_DECL ~stream();

  using history_type = response_base::history_type;

  const http::response_header &headers() const &
  {
    if (!parser_)
    {
      static http::response_header h;
      return h;
    }
    return parser_->get().base();
  }
  const history_type          &history() const & { return history_; }


  bool done() const {return !parser_ ||  parser_->is_done();}
  explicit stream(executor_type executor, std::nullptr_t ) : executor_{executor}, impl_(nullptr) {}
  explicit stream(executor_type executor, std::shared_ptr<detail::connection_impl> impl, detail::tracker t = {})
      : executor_{executor},
        impl_(std::move(impl)),
        t_(std::move(t))
  {}

  http::response_header &&headers() &&
  {
    if (!parser_)
    {
      static http::response_header h;
      return std::move(h);
    }

    return std::move(parser_->get().base());
  }
  history_type          &&history() && { return std::move(history_); }


  void prepend_history(history_type && pre_history)
  {
    history_.insert(history_.begin(),
                    std::make_move_iterator(history_.begin()),
                    std::make_move_iterator(history_.end()));
  }
 private:
  executor_type executor_;
  std::shared_ptr<detail::connection_impl> impl_;
  detail::lock_guard lock_;

  std::unique_ptr<http::response_parser<http::buffer_body>,
                  detail::pmr_deleter> parser_;
  history_type history_;
  detail::tracker t_;

  template<typename DynamicBuffer>
  struct async_read_op;
  struct async_dump_op;
  struct async_read_some_op;

  friend struct detail::connection_impl;
};

template <typename Executor1>
struct stream::rebind_executor<Executor1, void_t<typename Executor1::default_completion_token_type>>
{
  using other = defaulted<typename Executor1::default_completion_token_type>;
};

template<typename Token>
struct stream::defaulted : stream
{
  using stream::stream;
  using default_token = Token;
  defaulted(stream && lhs) :  stream(std::move(lhs)) {}

  template<typename MutableBufferSequence>
  auto async_read_some(const MutableBufferSequence & buffers)
  {
    return stream::async_read_some(buffers, default_token());
  }

  template<typename DynamicBuffer>
  auto async_read(DynamicBuffer & buffers)
  {
    return stream::async_read(buffers, default_token());
  }

  auto async_dump()
  {
    return stream::async_dump(default_token());
  }
};

}
}
#include <boost/requests/detail/connection_impl.hpp>

#include <boost/requests/impl/stream.hpp>

#endif // BOOST_REQUESTS_STREAM_HPP
