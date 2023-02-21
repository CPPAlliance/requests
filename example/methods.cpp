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
  auto r = requests::get(urls::url_view{"https://api.github.com/events"});
  r = requests::post(urls::url_view{"https://httpbin.org/post"}, json::value{{"key", "value"}});
  std::cout << r.headers << r.string_view() << std::endl;

  r = requests::put(urls::url_view{"https://httpbin.org/put"}, json::value{{"key","value"}});
  std::cout << r.headers << r.string_view() << std::endl;

  r = requests::delete_(urls::url_view{"https://httpbin.org/delete"});
  std::cout << r.headers << r.string_view() << std::endl;

  r = requests::head(urls::url_view{"https://httpbin.org/get"});
  std::cout << r.headers << r.string_view() << std::endl;

  r = requests::options(urls::url_view{"https://httpbin.org/get"});
  std::cout << r.headers << r.string_view() << std::endl;
  return 0;
}