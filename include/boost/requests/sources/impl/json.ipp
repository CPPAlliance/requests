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

std::pair<std::size_t, bool> json_source::read_some(void * data_, std::size_t size, system::error_code & ec)
{
  auto n = ser.read(static_cast<char*>(data_), size);
  std::string str{static_cast<const char *>(data_), size};
  return {n.size(), !ser.done()};
}

json_source::~json_source() = default;


BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, boost::json::value value, container::pmr::memory_resource * res)
{
  return std::allocate_shared<json_source>(container::pmr::polymorphic_allocator<void>(res), std::move(value));

}

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, boost::json::object   obj, container::pmr::memory_resource * res)
{
  return std::allocate_shared<json_source>(container::pmr::polymorphic_allocator<void>(res), std::move(obj));
}
BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, boost::json::array    arr, container::pmr::memory_resource * res)
{
  return std::allocate_shared<json_source>(container::pmr::polymorphic_allocator<void>(res), std::move(arr));
}

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, const boost::json::string & arr, container::pmr::memory_resource * res)
{
  return std::allocate_shared<json_source>(container::pmr::polymorphic_allocator<void>(res), std::move(arr));
}


}
}

#endif // BOOST_REQUESTS_SOURCES_IMPL_JSON_IPP
