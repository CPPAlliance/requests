//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_future.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/service.hpp>

#include <iostream>

using namespace boost;

int main(int argc, char * argv[])
{
  asio::io_context ctx;

  auto tk = asio::bind_executor(ctx, asio::use_future);

  auto fr1 = requests::async_get(    urls::url_view{"https://api.github.com/events"},                           {}, tk);
  auto fr2 = requests::async_post(   urls::url_view{"https://httpbin.org/post"}, json::value{{"key", "value"}},{}, tk);
  auto fr3 = requests::async_put(    urls::url_view{"https://httpbin.org/put"}, json::value{{"key","value"}},   {}, tk);
  auto fr4 = requests::async_delete( urls::url_view{"https://httpbin.org/delete"},                           {}, tk);
  auto fr5 = requests::async_head(   urls::url_view{"https://httpbin.org/get"},                                {}, tk);
  auto fr6 = requests::async_options(urls::url_view{"https://httpbin.org/get"},                             {}, tk);

  ctx.run();

  auto r1 = fr1.get();
  auto r2 = fr2.get();
  auto r3 = fr3.get();
  auto r4 = fr4.get();
  auto r5 = fr5.get();
  auto r6 = fr6.get();

  std::cout << r1.headers << r1.string_view() << std::endl;
  std::cout << r2.headers << r2.string_view() << std::endl;
  std::cout << r3.headers << r3.string_view() << std::endl;
  std::cout << r4.headers << r4.string_view() << std::endl;
  std::cout << r5.headers << r5.string_view() << std::endl;
  std::cout << r6.headers << r6.string_view() << std::endl;
  return 0;
}