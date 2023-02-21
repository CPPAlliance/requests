// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_IMPL_PUBLIC_SUFFIX_IPP
#define BOOST_REQUESTS_COOKIES_IMPL_PUBLIC_SUFFIX_IPP

#include <boost/requests/public_suffix.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/grammar/alnum_chars.hpp>
#include <boost/url/grammar/literal_rule.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <boost/url/grammar/token_rule.hpp>

namespace boost {
namespace requests {

const public_suffix_list & default_public_suffix_list()
{
    const static public_suffix_list def_list = {
#include "public_suffix.inc"
    };
    return def_list;
}

public_suffix_list load_public_suffix_list(core::string_view map)
{
    public_suffix_list res;

    res.full_matches.reserve(std::count(map.begin(), map.end(), '\n'));
    res.whitelist.reserve(std::count(map.begin(), map.end(), '!'));
    res.wildcards.reserve(std::count(map.begin(), map.end(), '*'));

    std::size_t idx = 0u;
    while (idx != core::string_view::npos)
    {
        const auto nx = map.find('\n', idx);
        core::string_view line;
        if (nx == core::string_view::npos)
        {
            line = map.substr(idx);
            idx = nx;
        }
        else
        {
            line = map.substr(idx, nx - idx);
            idx = nx + 1;
        }

        if (line.empty())
            continue;
        if (line.starts_with("//"))
            continue;

        if (line.ends_with('\r'))
            line.remove_suffix(1u);

        if (line.starts_with('!'))
            res.whitelist.emplace(line.substr(1));
        else if (line.starts_with("*."))
            res.wildcards.emplace(line.substr(2));
        else
            res.full_matches.emplace(line);
    }

    return res;
}

bool is_public_suffix(core::string_view value,
                      const public_suffix_list & pse)
{
    if (pse.full_matches.count(value) > 0)
        return true;

    auto dot = value.find('.');

    if (pse.whitelist.count(value) > 0)
        return false;

    auto seg1 = dot == core::string_view::npos ? "" : value.substr(dot + 1);

    if (pse.wildcards.count(seg1) > 0)
        return true;

    return false;

}

}
}

#endif //BOOST_REQUESTS_COOKIES_IMPL_PUBLIC_SUFFIX_IPP
