//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request_parameters.hpp>
#include <boost/requests/service.hpp>

#include <iostream>

using namespace boost;


int main(int argc, char * argv[])
{
  urls::url u{"https://httpbin.org/get"};
  u.params() = {{"key1", "value1"}, {"key2", "value2"}};
  auto r = requests::get(u);

  std::cout << r.headers <<  r.string_view() << std::endl;
  return 0;
}