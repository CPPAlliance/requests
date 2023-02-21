//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RFC_IMPL_QUOTED_STRING_IPP
#define BOOST_REQUESTS_RFC_IMPL_QUOTED_STRING_IPP

#include <boost/requests/grammar/ptokenchar.hpp>
#include <boost/requests/rfc/link.hpp>
#include <boost/requests/rfc/quoted_string.hpp>

namespace boost {
namespace requests {
namespace rfc {

auto
quoted_string_t::parse(
    char const*& it,
    char const* end
) const noexcept ->
    urls::error_types::result<value_type>
{
  namespace ug = boost::urls::grammar;

  if (it == end)
    BOOST_REQUESTS_RETURN_EC(urls::grammar::error::need_more);
  const auto it0 = it;

  if (*it != '"')
    BOOST_REQUESTS_RETURN_EC(urls::grammar::error::mismatch);
  it++;
  while (it != end && *it != '"')
  {
    if (*it == '\\')
      it++;

    it++;
  }

  if (*it != '"')
    BOOST_REQUESTS_RETURN_EC(urls::grammar::error::mismatch);

  return value_type(it0, ++it);
}

bool is_quoted_string(core::string_view sv)
{
  return !sv.empty() && sv.front() == '"' && sv.back() == '"';
}

bool unquoted_size(core::string_view sv)
{
  if (sv.empty())
    return 0u;
  else if (sv.front() != '"' || sv.back() == '"')
    return sv.size();
  else
  {
    auto ss = sv.substr(1u, sv.size() - 2u);
    std::size_t res = 0u;

    for (auto itr = ss.begin(); itr != ss.end(); itr++)
    {
      if (*itr == '\\')
        itr++;
      res++;
    }
    return res;
  }

}



}
}
}
#endif // BOOST_REQUESTS_RFC_IMPL_QUOTED_STRING_IPP
