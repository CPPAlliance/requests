// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/fields/set_cookie.hpp>
#include <boost/requests/grammar/any_char_except_ctl_semicolon.hpp>
#include <boost/requests/grammar/domain.hpp>
#include <boost/requests/grammar/fixed_token_rule.hpp>
#include <boost/requests/grammar/non_zero_digit.hpp>
#include <array>
#include <boost/requests/rfc/dates.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/grammar/literal_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/string_view.hpp>
#include "string_maker.hpp"

using namespace boost;

#include "boost/requests/cookie.hpp"
#include "doctest.h"
#include "string_maker.hpp"

TEST_SUITE_BEGIN("cookie-grammar");

TEST_CASE("non-zero-digit")
{
    CHECK(urls::grammar::parse("1234", urls::grammar::token_rule(requests::grammar::non_zero_digit)));
}

TEST_CASE("any-char-except")
{
    CHECK(urls::grammar::parse("1234", urls::grammar::token_rule(requests::grammar::any_char_except_ctl_semicolon)));

    auto res = urls::grammar::parse("1234;", urls::grammar::token_rule(requests::grammar::any_char_except_ctl_semicolon));
    CHECK(res == urls::grammar::error::leftover);
}

TEST_CASE("path-av")
{
    constexpr auto rule = urls::grammar::tuple_rule(
            urls::grammar::squelch(urls::grammar::literal_rule("Path=")),
            urls::grammar::token_rule(requests::grammar::any_char_except_ctl_semicolon)
            );


    CHECK(!urls::grammar::parse("1234", rule));

    auto res = urls::grammar::parse("Path=foobar;", rule);
    CHECK(res == urls::grammar::error::leftover);

    res = urls::grammar::parse("Path=foobar", rule);
    CHECK(res.value() == "foobar");
}


TEST_CASE("sane-cookie-date")
{
    CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(784111777)) ==
          urls::grammar::parse("Sun, 06 Nov 1994 08:49:37 GMT", requests::rfc::sane_cookie_date));
    CHECK(urls::grammar::error::mismatch == urls::grammar::parse("Mon, 06 Nov 1994 08:49:37 GMT", requests::rfc::sane_cookie_date));

    CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1696335075)) ==
          urls::grammar::parse("Tue, 03 Oct 2023 12:11:15 GMT", requests::rfc::sane_cookie_date));

    CHECK(urls::grammar::error::mismatch ==
          urls::grammar::parse("Sun, 03 Oct 2023 12:11:15 GMT", requests::rfc::sane_cookie_date));

    CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)) ==
          urls::grammar::parse("Wed, 09 Jun 2021 10:18:14 GMT", requests::rfc::sane_cookie_date));

    CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
          urls::grammar::parse("Thu, 01 Jan 1970 00:00:01 GMT",  requests::rfc::sane_cookie_date));

    CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(1)) ==
          urls::grammar::parse("Thu, 01 Jan 1970 00:00:01 GMT",  requests::rfc::sane_cookie_date));

    CHECK(std::chrono::system_clock::time_point(std::chrono::seconds(0)) ==
          urls::grammar::parse("Thu, 01-Jan-1970 00:00:00 GMT",  requests::rfc::sane_cookie_date));

    CHECK(std::chrono::system_clock::time_point(std::chrono::hours(1)) ==
          urls::grammar::parse("Thu, 01-Jan-1970 01:00:00 GMT",  requests::rfc::sane_cookie_date));

}


TEST_CASE("fixed-token")
{
    constexpr auto rule =
            requests::grammar::fixed_token_rule<2>(urls::grammar::digit_chars);

    CHECK("12" == urls::grammar::parse("12", rule));
    CHECK(urls::grammar::error::leftover == urls::grammar::parse("123", rule   ));
    CHECK(urls::grammar::error::need_more == urls::grammar::parse("1", rule   ));
}

TEST_CASE("domain")
{
    constexpr auto & rule = requests::grammar::domain;

    CHECK(!urls::grammar::parse("12", rule));
    CHECK("b12" == urls::grammar::parse("b12", rule));
    CHECK("foo.bar" == urls::grammar::parse("foo.bar", rule));
    CHECK(!urls::grammar::parse("foo.bar;", rule));
    CHECK("foo.bar-23-2" == urls::grammar::parse("foo.bar-23-2", rule));
}

TEST_CASE("set-cookie")
{
    auto v1 = requests::parse_set_cookie_field("theme=light");
    CHECK(v1);
    CHECK(v1->name == "theme");
    CHECK(v1->value == "light");
    CHECK(v1->extensions().empty());
    CHECK(v1->expires == std::chrono::system_clock::time_point::max());
    CHECK(v1->max_age == std::chrono::seconds::max());
    CHECK(v1->domain == "");
    CHECK(v1->path == "");
    CHECK(!v1->secure);
    CHECK(!v1->http_only);

    auto v2 = requests::parse_set_cookie_field("sessionToken=abc123; Expires=Wed, 09 Jun 2021 10:18:14 GMT");
    CHECK(v2);
    CHECK(v2->name == "sessionToken");
    CHECK(v2->value == "abc123");
    CHECK(v2->extensions().empty());
    CHECK(v2->expires == std::chrono::system_clock::time_point(std::chrono::seconds(1623233894)));
    CHECK(v2->max_age == std::chrono::seconds::max());
    CHECK(v2->domain == "");
    CHECK(v2->path == "");
    CHECK(!v2->secure);
    CHECK(!v2->http_only);


    auto v4 = requests::parse_set_cookie_field("LSID=DQAAAKEaem_vYg; Path=/accounts; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly");
    CHECK(v4);
    CHECK(v4->name == "LSID");
    CHECK(v4->value == "DQAAAKEaem_vYg");
    CHECK(v4->extensions().empty());
    CHECK(v4->expires == std::chrono::system_clock::time_point(std::chrono::seconds(1610576581)));
    CHECK(v4->max_age == std::chrono::seconds::max());
    CHECK(v4->domain == "");
    CHECK(v4->path == "/accounts");
    CHECK(v4->secure);
    CHECK(v4->http_only);

    auto v5 = requests::parse_set_cookie_field("HSID=AYQEVnDKrdst; Domain=.foo.com; Path=/; Expires=Wed, 13 Jan 2021 22:23:01 GMT; HttpOnly");
    CHECK(v5);
    CHECK(v5->name == "HSID");
    CHECK(v5->value == "AYQEVnDKrdst");
    CHECK(v5->extensions().empty());
    CHECK(v5->expires == std::chrono::system_clock::time_point(std::chrono::seconds(1610576581)));
    CHECK(v5->max_age == std::chrono::seconds::max());
    CHECK(v5->domain == "foo.com");
    CHECK(v5->path == "/");
    CHECK(!v5->secure);
    CHECK(v5->http_only);

    auto v6 = requests::parse_set_cookie_field("SSID=Ap4PGTEq; Domain=foo.com; Path=/; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly");
    CHECK(v6);
    CHECK(v6->name == "SSID");
    CHECK(v6->value == "Ap4PGTEq");
    CHECK(v6->extensions().empty());
    CHECK(v6->expires == std::chrono::system_clock::time_point(std::chrono::seconds(1610576581)));
    CHECK(v6->max_age == std::chrono::seconds::max());
    CHECK(v6->domain == "foo.com");
    CHECK(v6->path == "/");
    CHECK(v6->secure);
    CHECK(v6->http_only);

    auto v7 = requests::parse_set_cookie_field("lu=Rg3vHJZnehYLjVg7qi3bZjzg; Expires=Tue, 15 Jan 2013 21:47:38 GMT; Path=/; Domain=.example.com; HttpOnly");
    CHECK(v7);
    CHECK(v7->name == "lu");
    CHECK(v7->value == "Rg3vHJZnehYLjVg7qi3bZjzg");
    CHECK(v7->extensions().empty());
    CHECK(v7->expires == std::chrono::system_clock::time_point(std::chrono::seconds(1358286458)));
    CHECK(v7->max_age == std::chrono::seconds::max());
    CHECK(v7->domain == "example.com");
    CHECK(v7->path == "/");
    CHECK(!v7->secure);
    CHECK(v7->http_only);


    auto v8 = requests::parse_set_cookie_field("made_write_conn=1295214458; Path=/; Domain=.example.com");
    CHECK(v8);
    CHECK(v8->name == "made_write_conn");
    CHECK(v8->value == "1295214458");
    CHECK(v8->extensions().empty());
    CHECK(v8->expires == std::chrono::system_clock::time_point::max());
    CHECK(v8->max_age == std::chrono::seconds::max());
    CHECK(v8->domain == "example.com");
    CHECK(v8->path == "/");
    CHECK(!v8->secure);
    CHECK(!v8->http_only);


    auto v9 = requests::parse_set_cookie_field("reg_fb_gate=deleted; Expires=Thu, 01 Jan 1970 00:00:01 GMT; Path=/; Domain=.example.thingy; HttpOnly");
    CHECK(v9);
    CHECK(v9->name == "reg_fb_gate");
    CHECK(v9->value == "deleted");
    CHECK(v9->extensions().empty());
    CHECK(v9->expires == std::chrono::system_clock::time_point(std::chrono::seconds(1)));
    CHECK(v9->max_age == std::chrono::seconds::max());
    CHECK(v9->domain == "example.thingy");
    CHECK(v9->path == "/");
    CHECK(!v9->secure);
    CHECK(v9->http_only);

    std::array<requests::set_cookie, 8> cks = {*v1, *v2, *v4, *v5, *v6, *v7, *v8, *v9};

    CHECK(requests::detail::make_cookie_field(cks)
        == "theme=light; sessionToken=abc123; LSID=DQAAAKEaem_vYg; HSID=AYQEVnDKrdst; SSID=Ap4PGTEq; lu=Rg3vHJZnehYLjVg7qi3bZjzg; made_write_conn=1295214458; reg_fb_gate=deleted"
    );
}


TEST_SUITE_END();
