//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCES_IMPL_JSON_IPP
#define BOOST_REQUESTS_SOURCES_IMPL_JSON_IPP

#include <boost/requests/json.hpp>
#include <boost/requests/sources/json.hpp>

namespace boost
{
namespace requests
{

json_source::json_source(json::value data) : data(std::move(data))
{
  ser.reset(&this->data);
}

void json_source::reset()
{
  ser.reset(&data);

}
std::pair<std::size_t, bool> json_source::read_some(void * data, std::size_t size, system::error_code & ec)
{
  auto n = ser.read(static_cast<char*>(data), size);
  std::string str{static_cast<const char *>(data), size};
  return {n.size(), !ser.done()};
}


}
}

#endif // BOOST_REQUESTS_SOURCES_IMPL_JSON_IPP
