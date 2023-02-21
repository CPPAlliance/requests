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
/*
 >>> import requests
>>> r = requests.get('https://httpbin.org/basic-auth/user/pass', auth=('user', 'pass'))
>>> r.status_code
200
>>> r.headers['content-type']
'application/json; charset=utf8'
>>> r.encoding
'utf-8'
>>> r.text
'{"authenticated": true, ...'
>>> r.json()
{'authenticated': True, ...}
*/

int main(int argc, char * argv[])
{
  asio::io_context ctx;
  auto fr = requests::async_get(urls::url_view("https://httpbin.org/basic-auth/user/pass"),
                                requests::headers({requests::basic_auth("user", "pass")}),
                                asio::bind_executor(ctx, asio::use_future));

  ctx.run();
  auto r = fr.get();

  std::cout << r.result_code() << std::endl;
  // 200
  std::cout << r.headers["Content-Type"] << std::endl;
  // 'application/json; charset=utf8'

  std::cout << r.string_view() << std::endl;
  // {"authenticated": true, ...

  std::cout << as_json(r) << std::endl;
  // {'authenticated': True, ...}

  return 0;
}