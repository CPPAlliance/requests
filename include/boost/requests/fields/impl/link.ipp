//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_FIELDS_IMPL_LINK_IPP
#define BOOST_REQUESTS_FIELDS_IMPL_LINK_IPP

#include <boost/requests/grammar/alternate_rule.hpp>
#include <boost/requests/fields/link.hpp>
#include <boost/requests/grammar/raw_string.hpp>
#include <boost/requests/grammar/token_rule.hpp>
#include <boost/requests/rfc/link.hpp>
#include <boost/url/rfc/uri_reference_rule.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/parse.hpp>

namespace boost {
namespace requests {

bool link::extensions_only::operator()(const core::string_view &ra) const
{
  namespace ug = urls::grammar;
  return !ug::ci_is_equal(ra, "rel")
      && !ug::ci_is_equal(ra, "anchor")
      && !ug::ci_is_equal(ra, "rev")
      && !ug::ci_is_equal(ra, "hreflang")
      && !ug::ci_is_equal(ra, "media")
      && !ug::ci_is_equal(ra, "title")
      && !ug::ci_is_equal(ra, "title*")
      && !ug::ci_is_equal(ra, "type");
}

system::result<urls::grammar::range<link>> parse_link_field(core::string_view input)
{
  namespace ug = urls::grammar;

  auto res = urls::grammar::parse(
          input,
          ug::range_rule(
            rfc::link_value,
            ug::tuple_rule(
              ug::squelch(
                ug::tuple_rule(
                  ug::delim_rule(','),
                  ug::range_rule(ug::delim_rule(' ')))
                  ),
              rfc::link_value
            )
          )
        );

  return res;
}

namespace detail
{

inline system::result<urls::grammar::range<core::string_view>> parse_rel_type(core::string_view value)
{
  namespace ug = urls::grammar;

  // this is not strictly compliant - shuold be lower-case only
  constexpr auto reg_rel_type = grammar::token_rule( ug::alpha_chars, ug::lut_chars(".-") + ug::alnum_chars);
  constexpr auto ext_rel_type = urls::uri_reference_rule;
  constexpr auto relation_type = grammar::raw_string(ug::variant_rule(ext_rel_type, reg_rel_type));

  constexpr auto single = ug::range_rule(relation_type, 1, 1);
  constexpr auto multi  =
      ug::tuple_rule(
          ug::squelch(ug::delim_rule('"')),
          ug::range_rule(
              relation_type,
              ug::tuple_rule(ug::squelch(ug::delim_rule(' ')), relation_type)
                  ),
          ug::squelch(ug::delim_rule('"')));
  constexpr auto relation_types = grammar::alternate_rule(single, multi);
  return ug::parse(value, relation_types);

}


}

system::result<urls::grammar::range<core::string_view>> link::rel() const
{
  const auto begin = attributes.begin();
  const auto end = attributes.end();

  const auto itr = std::find_if(begin,end,
                                [&](const field & attr){
                                  return urls::grammar::ci_is_equal(attr.key, "rel");
                                });

  if (itr == end)
    return urls::grammar::range<core::string_view>();


  return detail::parse_rel_type((*itr).value);
}


system::result<urls::grammar::range<core::string_view>> link::rev() const
{
  const auto begin = attributes.begin();
  const auto end = attributes.end();

  const auto itr = std::find_if(begin,end,
                                [&](const field & attr){
                                  return urls::grammar::ci_is_equal(attr.key, "rev");
                                });

  if (itr == end)
    return urls::grammar::range<core::string_view>();

  return detail::parse_rel_type((*itr).value);
}

system::result<urls::url_view> link::anchor() const
{
  const auto begin = attributes.begin();
  const auto end = attributes.end();

  const auto itr = std::find_if(begin,end,
                                [&](const field & attr){
                                  return urls::grammar::ci_is_equal(attr.key, "anchor");
                                });

  if (itr == end)
    return urls::grammar::error::out_of_range;

  return urls::parse_uri((*itr).value);
}



}
}

#endif // BOOST_REQUESTS_FIELDS_IMPL_LINK_IPP
