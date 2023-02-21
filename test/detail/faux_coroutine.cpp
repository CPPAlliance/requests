//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/faux_coroutine.hpp>

#include <boost/asio/connect_pipe.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>

#include "../doctest.h"

using namespace boost;

struct my_coro : asio::coroutine
{
  asio::readable_pipe source;
  asio::writable_pipe sink;

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return source.get_executor(); }

  char buf[4096];
  system::error_code ec_;

  my_coro(const my_coro  &) = delete;
  my_coro(      my_coro &&) = delete;

  my_coro(asio::writable_pipe &source, asio::readable_pipe &sink)
      : source(sink.get_executor()), sink(source.get_executor())
  {
    asio::connect_pipe(sink, this->sink, ec_);
    if (!ec_)
      asio::connect_pipe(this->source, source, ec_);
  }

  using completion_signature_type = void(system::error_code);
  using step_signature_type       = void(system::error_code, std::size_t);

  void resume(requests::detail::faux_token_t<step_signature_type> token,
              system::error_code & ec, std::size_t n = {})
  {
    BOOST_ASIO_CORO_REENTER(this)
    {
      if (ec_)
      {
        ec = ec_;
        break;
      }
      while (source.is_open())
      {
        BOOST_ASIO_CORO_YIELD source.async_read_some(asio::buffer(buf), std::move(token));
        if (ec && n == 0)
          break;
        BOOST_ASIO_CORO_YIELD sink.async_write_some(asio::buffer(buf, n), std::move(token));
        if (ec)
          break;
        BOOST_ASIO_CORO_YIELD {
          requests::detail::faux_token_t<void()> tt = std::move(token);
          asio::post(sink.get_executor(), std::move(tt));
        };
      }
    }
  }
};

TEST_SUITE_BEGIN("async_coro");
TEST_CASE("sanity-check")
{
  asio::io_context ctx;
  asio::writable_pipe source{ctx};
  asio::readable_pipe sink{ctx};

  requests::detail::faux_run<my_coro>(
      [](system::error_code ec)
      {
        printf("EC : %s\n", ec.message().c_str());
      }, source, sink);


  char read_buf[32];
  sink.async_read_some(asio::buffer(read_buf), asio::detached);

  asio::async_write(source, asio::buffer("FOOBAR", 6),
                    [&](system::error_code ec, std::size_t n)
                    {
                      source.close();
                    });

  ctx.run();
}

TEST_SUITE_END();