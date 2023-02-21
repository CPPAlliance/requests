//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_FORM_HPP
#define BOOST_REQUESTS_FORM_HPP

#include <boost/url/url.hpp>
#include <initializer_list>
#include <boost/requests/source.hpp>

namespace boost {
namespace requests {


struct form
{
  urls::url storage;

  explicit form(std::initializer_list<urls::param_view> params)
  {
    storage.params().assign(std::move(params));
  }

  form(form && ) = default;
  form(const form & ) = default;

  template<typename Container>
  explicit form(Container && ct, decltype(std::begin(ct)) * = nullptr)
  {
    storage.params().assign(std::begin(ct), std::end(ct));
  }

};

struct multi_part_form
{
  struct form_data
  {
    core::string_view name;
    source_ptr source;

    form_data(core::string_view name, source_ptr source) : name(name), source(std::move(source)) {}

    template<typename Source>
    form_data(core::string_view name,
              Source && source,
              container::pmr::memory_resource * resource = container::pmr::get_default_resource(),
              decltype(make_source(std::declval<Source>())) * = nullptr)
      : name(name), source(make_source(std::forward<Source>(source), resource))
    {
    }
  };

  std::vector<form_data> storage;

  multi_part_form(multi_part_form && ) = default;
  multi_part_form(const multi_part_form & ) = default;

  explicit multi_part_form(std::initializer_list<form_data> params) : storage(std::move(params))
  {
  }

  template<typename Container>
  explicit multi_part_form(Container && ct, decltype(std::begin(ct)) * = nullptr)
      : storage(std::begin(ct), std::end(ct))
  {
  }
};

}
}

#endif // BOOST_REQUESTS_FORM_HPP
