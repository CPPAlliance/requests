// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_REQUEST_PARAMETERS_HPP
#define BOOST_REQUESTS_REQUEST_PARAMETERS_HPP

#include <boost/beast/core/detail/base64.hpp>
#include <boost/requests/cookie_jar.hpp>
#include <boost/requests/http.hpp>
#include <boost/requests/request_options.hpp>
#include <boost/requests/response.hpp>
#include <memory>

namespace boost {
namespace requests {

struct field_entry
{
  http::field field = http::field::unknown;
  core::string_view key;
  core::string_view value;
  std::string buffer;

  field_entry() = default;
  field_entry(http::field field, core::string_view value) : field(field), value(value) {}
  field_entry(core::string_view key, core::string_view value) : key(key), value(value) {}

};

inline field_entry basic_auth(core::string_view username,
                              core::string_view password)
{
  auto sz = beast::detail::base64::encoded_size(username.size() + 1 + password.size());
  std::string res;
  res.resize(sizeof("Basic") + sz);
  const beast::string_view prefix = "Basic ";
  auto itr = std::copy(prefix.begin(), prefix.end(), res.begin());
  const auto data = std::string(username) + ":" + std::string(password);
  beast::detail::base64::encode(&*itr, data.data(), data.size());

  field_entry fe;
  fe.field = http::field::authorization;
  fe.value = fe.buffer = std::move(res);
  return fe;
}


inline field_entry bearer(core::string_view token)
{
  field_entry fe;
  fe.field = http::field::authorization;
  fe.value = fe.buffer = "Bearer " + std::string(token);
  return fe;
}

inline auto headers(std::initializer_list<field_entry> fields,
                    boost::container::pmr::memory_resource * res = boost::container::pmr::get_default_resource())
  -> beast::http::basic_fields<boost::container::pmr::polymorphic_allocator<char>>
{
  using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
  beast::http::basic_fields<allocator_type> f{allocator_type{res}};
  for (const auto & init : fields)
    if (init.field != http::field::unknown)
      f.set(init.field, init.value);
    else
      f.set(init.key, init.value);
  return f;
}


struct request_parameters {
  //Allocator
  using allocator_type = boost::container::pmr::polymorphic_allocator<char>;
  allocator_type get_allocator() const {return fields.get_allocator();}
  using fields_type = beast::http::basic_fields<allocator_type>;
  fields_type fields;
  request_options opts{};
  cookie_jar * jar = nullptr;
};

}
}

#endif // BOOST_REQUESTS_REQUEST_PARAMETERS_HPP
