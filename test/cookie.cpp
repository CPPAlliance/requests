// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/connection.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/json.hpp>
#include <boost/requests/download.hpp>
#include <boost/requests/form.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>

#include "doctest.h"
#include "string_maker.hpp"

#include <iostream>

using namespace boost;

inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}

TEST_SUITE_BEGIN("cookie");


void http_request_cookie_connection(bool https)
{
  auto url = httpbin();

  asio::io_context ctx;

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();
  auto hc = https ? requests::connection(ctx.get_executor(), sslctx) : requests::connection(ctx.get_executor());
  hc.set_host(url);
  hc.use_ssl(https);

  asio::ip::tcp::resolver rslvr{ctx};
  asio::ip::tcp::endpoint ep = *rslvr.resolve(url, https ? "https" : "http").begin();

  hc.connect(ep);

  requests::cookie_jar jar;
  auto res = requests::json::get(hc, urls::url_view{"/cookies"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  CHECK(res.value.at("cookies").as_object().empty());
  CHECK(jar.content.empty());

  res = requests::json::get(hc, urls::url_view{"/cookies/set?cookie-1=foo"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  CHECK(res.value.at("cookies") == json::object{{"cookie-1", "foo"}});
  REQUIRE(!jar.content.empty());
  auto citr = jar.content.begin();
  CHECK(citr->value == "foo");
  CHECK(citr->name == "cookie-1");
  CHECK(citr->secure_only_flag == false);
  CHECK(citr->path == "/");

  res = requests::json::get(hc, urls::url_view{"/cookies/set/cookie-2/bar"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  CHECK(res.value.at("cookies") == json::object{{"cookie-1", "foo"}, {"cookie-2", "bar"}});
  REQUIRE(jar.content.size() == 2u);
  citr = jar.content.begin();
  if (citr->name == "cookie-1")
  {
    CHECK(citr->value == "foo");
    CHECK(citr->secure_only_flag == false);
    CHECK(citr->path == "/");
    citr ++ ;
    CHECK(citr->value == "bar");
    CHECK(citr->secure_only_flag == false);
    CHECK(citr->path == "/");

  }
  else
  {
    CHECK(citr->value == "bar");
    CHECK(citr->secure_only_flag == false);
    CHECK(citr->path == "/");
    citr ++ ;
    CHECK(citr->value == "foo");
    CHECK(citr->secure_only_flag == false);
    CHECK(citr->path == "/");
  }
  res = requests::json::get(hc, urls::url_view{"/cookies/delete?cookie-1"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});

  CHECK(!jar.content.empty());
  REQUIRE(jar.content.size() == 1u);
  citr = jar.content.begin();
  CHECK(citr->value == "bar");
  CHECK(citr->name == "cookie-2");
  CHECK(citr->secure_only_flag == false);
  CHECK(citr->path == "/");

  res = requests::json::get(hc, urls::url_view{"/cookies/delete?cookie-2"}, {{}, /*.opts=*/{false}, /*.jar=*/&jar});
  CHECK(jar.content.empty());
}

TEST_CASE("sync-connection-cookie-request")
{
  SUBCASE("http") { http_request_cookie_connection(false);}
  SUBCASE("https") { http_request_cookie_connection(true);}
}

TEST_SUITE_END();