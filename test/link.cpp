//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/fields/link.hpp>
#include <boost/url/parse.hpp>
#include <vector>
#include "doctest.h"
#include "string_maker.hpp"


TEST_SUITE_BEGIN("rfc.link");

TEST_CASE("link")
{
  using namespace boost;

  namespace br = boost::requests;

  SUBCASE("single")
  {
    auto txt = R"(<https://one.example.com>)";
    auto lst = br::parse_link_field(txt);
    CHECK_MESSAGE(!lst.has_error(), lst.error());
    REQUIRE(lst->size() == 1u);
    CHECK((*lst->begin()).attributes.empty());
    CHECK((*lst->begin()).url == urls::parse_uri("https://one.example.com"));
  }

  SUBCASE("multiple")
  {
    auto txt = R"(<https://one.example.com>, <https://two.example.com>)";
    auto lst = br::parse_link_field(txt);
    CHECK_MESSAGE(!lst.has_error(), lst.error());

    REQUIRE(lst->size() == 2u);

    CHECK((*lst->begin()).attributes.empty());
    CHECK((*lst->begin()).url == urls::parse_uri("https://one.example.com").value());

    CHECK((*std::next(lst->begin())).attributes.empty());
    CHECK((*std::next(lst->begin())).url == urls::parse_uri("https://two.example.com").value());
  }


  SUBCASE("single with param")
  {
    auto txt = R"(<https://one.example.com>; foobar)";
    auto lst = br::parse_link_field(txt);
    CHECK_MESSAGE(!lst.has_error(), lst.error());

    CHECK(lst->size() == 1u);

    auto val = *lst->begin();
    CHECK(val.attributes.begin() != val.attributes.end());

    std::vector<br::link::field> vec;
    vec.assign(val.attributes.begin(), val.attributes.end());
    CHECK(vec.size() == 1u);
    if (!vec.empty())
    {
      CHECK(val.url == urls::parse_uri("https://one.example.com"));
      CHECK(vec.front().key == "foobar");
      CHECK(vec.front().value.empty());
    }
  }
  
  SUBCASE("single with param")
  {
    auto txt = R"(<https://one.example.com>; xyz=ctl)";
    auto lst = br::parse_link_field(txt);
    CHECK_MESSAGE(!lst.has_error(), lst.error());

    CHECK(lst->size() == 1u);
    auto  val = *lst->begin();
    CHECK(val.attributes.begin() != val.attributes.end());

    std::vector<br::link::field> vec;
    vec.assign(val.attributes.begin(), val.attributes.end());
    CHECK(vec.size() == 1u);
    if (!vec.empty())
    {
      CHECK(val.url == urls::parse_uri("https://one.example.com"));
      CHECK(vec.front().key == "xyz");
      CHECK(vec.front().value == "ctl");
    }
  }


  SUBCASE("single with param")
  {
    auto txt = R"(<https://one.example.com>; rel="preconnect")";
    auto lst = br::parse_link_field(txt);
    CHECK_MESSAGE(!lst.has_error(), lst.error());


    CHECK(lst->size() == 1u);
    auto  val = *lst->begin();
    CHECK(val.attributes.begin() != val.attributes.end());

    auto rel = val.rel();
    CHECK_MESSAGE(!rel.has_error(), rel.error());
    CHECK(rel->size() == 1);
  }

  SUBCASE("multiple with param")
  {
    auto txt =
        R"(<https://one.example.com>; rel="preconnect next", )"
        R"(<https://two.example.com>; rel="preconnect", )"
        R"(<https://three.example.com>; rel="preconnect")";
    auto lst = br::parse_link_field(txt);
    CHECK_MESSAGE(!lst.has_error(), lst.error());
  }

}

TEST_SUITE_END();