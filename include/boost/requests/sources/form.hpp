// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_FORM_HPP
#define BOOST_REQUESTS_SOURCES_FORM_HPP

#include <boost/requests/source.hpp>
#include <boost/requests/form.hpp>
#include <boost/beast/core/file.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

BOOST_REQUESTS_DECL
std::array<char, 62> make_boundary_value();

}

struct form_source : source
{
  urls::url storage;
  urls::params_encoded_view param_view{storage.encoded_params()};
  std::size_t pos{0u};

  form_source(urls::params_encoded_view js) : param_view(js)
  {
  }
  form_source(urls::url storage) : storage(std::move(storage))
  {
  }

  form_source(form frm) : storage(std::move(frm.storage))
  {
  }

  ~form_source() = default;
  optional<std::size_t> size( ) const override
  {
    return param_view.buffer().size();
  };
  void reset() override
  {
    pos = 0u;
  }
  std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ec) override
  {
    const auto left = param_view.buffer().size() - pos;
    const auto sz = size;
    auto dst = static_cast<char*>(data);
    auto n = (std::min)(left, sz);
    std::char_traits<char>::copy(dst, param_view.buffer().data() + pos, n);
    pos += n;
    return {n, pos != param_view.buffer().size()};
  }

  core::string_view default_content_type() override { return "application/x-www-form-urlencoded"; }
};

struct multi_part_form_source final : source
{
  std::array<char, 62> boundary_and_type{detail::make_boundary_value()};
  multi_part_form mpf;
  typename std::vector<multi_part_form::form_data>::const_iterator current = mpf.storage.cbegin();

  boost::optional<asio::coroutine> coro_state;
  std::size_t remaining = 0;

  BOOST_REQUESTS_DECL
  ~multi_part_form_source();
  BOOST_REQUESTS_DECL
  multi_part_form_source(const multi_part_form_source & rhs);

  BOOST_REQUESTS_DECL
  multi_part_form_source(multi_part_form && mpf);
  BOOST_REQUESTS_DECL
  multi_part_form_source(const multi_part_form & mpf);

  BOOST_REQUESTS_DECL
  optional<std::size_t> size() const override;
  BOOST_REQUESTS_DECL
  void reset() override;
  BOOST_REQUESTS_DECL
  std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ec) override;
  BOOST_REQUESTS_DECL
  core::string_view default_content_type() override;
};

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, form form_, container::pmr::memory_resource * res);

BOOST_REQUESTS_DECL
source_ptr tag_invoke(make_source_tag, multi_part_form mpf, container::pmr::memory_resource * res);


}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/sources/impl/form.ipp>
#endif

#endif //BOOST_REQUESTS_SOURCES_FILE_HPP
