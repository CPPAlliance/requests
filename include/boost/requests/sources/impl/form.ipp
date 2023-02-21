//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCE_IMPL_FORM_IPP
#define BOOST_REQUESTS_SOURCE_IMPL_FORM_IPP

#include <boost/requests/sources/form.hpp>
#include <boost/requests/sources/file.hpp>
#include <random>

namespace boost
{
namespace requests
{
namespace detail
{

std::array<char, 62> make_boundary_value()
{
  std::array<char, 62> res{};
  static std::random_device rd;

  const char prefix[31] = "multipart/form-data; boundary=";

  constexpr static char values[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::uniform_int_distribution<int> dist(0, sizeof(values) - 2);

  auto itr = std::copy(std::begin(prefix), std::prev(std::end(prefix)), res.begin());
  std::generate(itr, res.end(), [&]{return values[dist(rd)];});
  return res;
}

}

source_ptr tag_invoke(make_source_tag, form form_, container::pmr::memory_resource * res)
{
  return std::allocate_shared<form_source>(
      container::pmr::polymorphic_allocator<void>(res),
      std::move(form_));
}

source_ptr tag_invoke(make_source_tag, multi_part_form mpf, container::pmr::memory_resource * res)
{
  return std::allocate_shared<multi_part_form_source>(
      container::pmr::polymorphic_allocator<void>(res),
      std::move(mpf));
}

optional<std::size_t> multi_part_form_source::size() const
{
  std::size_t res = 0u;

  for (auto & mm : mpf.storage)
  {
    res += 2; // --
    res += 32; // boundary
    res += sizeof("\r\nContent-Disposition: form-data; name=\"") - 1;
    res += mm.name.size();

    auto fsrc = dynamic_cast<file_source*>(mm.source.get());
    if (fsrc != nullptr)
    {
      res += sizeof("\"; filename=\"") - 1;
      res += fsrc->path.filename().string().size();
    }
    res += sizeof("\"\r\nContent-Type: ") - 1;
    res += mm.source->default_content_type().size();
    res += 4; // \r\n\r\n;

    auto sz = mm.source->size();
    if (!sz.has_value())
      return sz;

    res += *sz;
    res += 2; // \r\n;
  }
  res += 2; // --
  res += 32; // boundary
  res += 2; // --
  return res;
}


core::string_view multi_part_form_source::default_content_type()
{
  return core::string_view{boundary_and_type.data(), boundary_and_type.size()};
}

void multi_part_form_source::reset()
{
  coro_state.reset();

  while (current != mpf.storage.begin())
  {
    if (current != mpf.storage.end())
      current->source->reset();
    current--;
  }
}

std::pair<std::size_t, bool> multi_part_form_source::read_some(void * data, std::size_t size, system::error_code & ec)
{
  if (!coro_state)
    coro_state.emplace();
  std::size_t written = 0u;

  auto puts =
      [&](core::string_view s)
      {
          auto cc = static_cast<char*>(data);
          auto len = (std::min)(s.size(), size);
          std::memcpy(data, s.data(), len);
          data = cc+len;
          size -= len;
          written += len;
          return len;
      };

  core::string_view msg;

#define BOOST_REQUESTS_CORO_PUTS(Msg)                      \
  remaining = core::string_view(Msg).size();               \
  while (remaining > 0u)                                   \
  {                                                        \
    msg = Msg;                                             \
    remaining -= puts(msg.substr(msg.size() - remaining)); \
    if (size == 0) { BOOST_ASIO_CORO_YIELD ; }             \
  }

  const core::string_view boundary{&*boundary_and_type.end() - 32, 32};

  file_source * fsrc = nullptr;
  if (current != mpf.storage.cend())
    fsrc = dynamic_cast<file_source*>(current->source.get());

  std::pair<std::size_t, bool> tmp = {0u, true};
  BOOST_ASIO_CORO_REENTER(&*coro_state)
  {
    while (current != mpf.storage.cend())
    {
      if (size == 0) { BOOST_ASIO_CORO_YIELD; }
      BOOST_REQUESTS_CORO_PUTS("--");
      BOOST_REQUESTS_CORO_PUTS(boundary)
      BOOST_REQUESTS_CORO_PUTS("\r\nContent-Disposition: form-data; name=\"")
      BOOST_REQUESTS_CORO_PUTS(current->name)

      fsrc = dynamic_cast<file_source*>(current->source.get());
      if (fsrc != nullptr)
      {
        BOOST_REQUESTS_CORO_PUTS("\"; filename=\"")
        BOOST_REQUESTS_CORO_PUTS(fsrc->path.filename().string())
      }
      BOOST_REQUESTS_CORO_PUTS("\"\r\nContent-Type: ")
      BOOST_REQUESTS_CORO_PUTS(current->source->default_content_type())
      BOOST_REQUESTS_CORO_PUTS("\r\n\r\n")

      tmp.second = true;
      while (tmp.second)
      {
        tmp = current->source->read_some(data, size, ec);
        size -= tmp.first;
        written += tmp.first;
        data = static_cast<char*>(data) + tmp.first;
        if (ec)
          return {written, true};
        if (size == 0) {BOOST_ASIO_CORO_YIELD; }
      }
      BOOST_REQUESTS_CORO_PUTS("\r\n")
      current ++;
    }

    BOOST_REQUESTS_CORO_PUTS("--")
    BOOST_REQUESTS_CORO_PUTS(boundary)
    BOOST_REQUESTS_CORO_PUTS("--")
  }

  return {written, !coro_state->is_complete()};
}

#undef BOOST_REQUESTS_CORO_PUTS

multi_part_form_source::~multi_part_form_source() = default;
multi_part_form_source::multi_part_form_source(const multi_part_form_source & rhs)= default;

multi_part_form_source::multi_part_form_source(multi_part_form && mpf) : mpf(std::move(mpf)) {}
multi_part_form_source::multi_part_form_source(const multi_part_form & mpf) : mpf(mpf) {}

}
}

#endif // BOOST_REQUESTS_SOURCE_IMPL_FORM_IPP
