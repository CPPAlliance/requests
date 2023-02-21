//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCES_STRING_VIEW_HPP
#define BOOST_REQUESTS_SOURCES_STRING_VIEW_HPP

#include <boost/requests/source.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/utility/string_view.hpp>

namespace boost
{
namespace requests
{


template<typename Char>
struct basic_string_view_source final : source
{
  core::basic_string_view<Char> data;
  std::size_t pos{0};

  basic_string_view_source(core::basic_string_view<Char> data) : data(data) {}

  optional<std::size_t> size() const final
  {
    return data.size();
  }

  void reset() final
  {
    pos = 0;
  }

  std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ) final
  {
    const auto left = this->data.size() - pos;
    const auto sz = size / sizeof(Char);
    auto dst = static_cast<char*>(data);
    auto cnt = (std::min)(left, sz);
    std::char_traits<Char>::copy(dst, this->data.data() + pos, cnt);
    std::size_t n = cnt * sizeof(Char);
    pos += n;
    return {n, pos != this->data.size()};
  }

  core::string_view default_content_type() final
  {
    BOOST_IF_CONSTEXPR (sizeof(Char) == 1)
      return "text/plain; charset=utf-8";
    else BOOST_IF_CONSTEXPR (sizeof(Char) == 2)
      return "text/plain; charset=utf-16";
    else BOOST_IF_CONSTEXPR (sizeof(Char) == 4)
      return "text/plain; charset=utf-32";
    return "";
  }
};

template<std::size_t N>
source_ptr tag_invoke(make_source_tag, const char (&data)[N], container::pmr::memory_resource * res)
{
  return std::allocate_shared<basic_string_view_source<char>>(
      container::pmr::polymorphic_allocator<void>(res), data);
}

template<typename Char>
source_ptr tag_invoke(make_source_tag, const core::basic_string_view<Char> & data,
                      container::pmr::memory_resource * res)
{
  return std::allocate_shared<basic_string_view_source<Char>>(
      container::pmr::polymorphic_allocator<void>(res), std::move(data));
}

template<typename Char>
source_ptr tag_invoke(make_source_tag, core::basic_string_view<Char> && data,
                      container::pmr::memory_resource * res) = delete;

template<typename Char, typename Traits>
source_ptr tag_invoke(make_source_tag, const boost::basic_string_view<Char, Traits> & data,
                      container::pmr::memory_resource * res)
{
  return std::allocate_shared<basic_string_view_source<Char>>(
      container::pmr::polymorphic_allocator<void>(res), std::move(data));
}

template<typename Char, typename Traits>
source_ptr tag_invoke(make_source_tag, boost::basic_string_view<Char, Traits> && data,
                      container::pmr::memory_resource * res) = delete;

#if defined(__cpp_lib_string_view)
template<typename Char, typename Traits>
source_ptr  tag_invoke(make_source_tag, const std::basic_string_view<Char, Traits> & data,
                       container::pmr::memory_resource * res)
{
  return std::allocate_shared<basic_string_view_source<Char>>(
      container::pmr::polymorphic_allocator<void>(res),
      core::basic_string_view<Char>(data.data(), data.size()));
}

template<typename Char, typename Traits>
source_ptr tag_invoke(make_source_tag, std::basic_string_view<Char, Traits> && data,
                      container::pmr::memory_resource * res) = delete;
#endif

}
}

#endif // BOOST_REQUESTS_SOURCES_STRING_VIEW_HPP
