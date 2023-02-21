//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/json.hpp>
#include <boost/requests/connection.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/optional/optional_io.hpp>

#include "doctest.h"
#include "string_maker.hpp"


namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;
namespace beast = boost::beast;

using boost::system::error_code;

inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}


TEST_SUITE_BEGIN("json-connection");

void json_request_connection(bool https)
{
  auto url = httpbin();

  asio::io_context ctx;

  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};

  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();

  requests::connection hc(ctx.get_executor(), sslctx);

  hc.set_host(url);
  hc.use_ssl(https);
  CHECK(hc.uses_ssl() == https);

  asio::ip::tcp::resolver rslvr{ctx};
  asio::ip::tcp::endpoint ep = *rslvr.resolve(url, https ? "https" : "http").begin();

  hc.connect(ep);


  // headers
  {
    auto hdr = request(hc, requests::http::verb::get, urls::url_view("/headers"),
                       requests::empty{},
                       {requests::headers({{"Test-Header", "it works"}}), {false}});

    CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // stream
  {
    auto str = hc.ropen(requests::http::verb::get, urls::url_view("/get"),
                        requests::empty{}, {requests::headers({{"Test-Header", "it works"}}), {false}});
    CHECK_HTTP_RESULT(str.headers());
    json::stream_parser sp;

    char buf[32];

    error_code ec;
    while (!str.done() && !ec)
    {
      auto sz = str.read_some(asio::buffer(buf), ec);
      CHECK(ec == error_code{});
      sp.write_some(buf, sz, ec);
      CHECK(ec == error_code{});
    }

    auto hd = sp.release().at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // get-redirect
  {
    auto hdr = requests::json::get(hc, urls::url_view("/redirect-to?url=%2Fget"),
                                   {requests::headers({{"Test-Header", "it works"}}), {false}});
    CHECK_HTTP_RESULT(hdr.headers);
    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = hdr.value.at("headers");

    CHECK(hd.at("Host")        == json::value(url));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // too-many-redirects
  {
    error_code ec;
    auto res = requests::json::get(hc, urls::url_view("/redirect/10"),
                                   {{}, {false, requests::redirect_mode::private_domain, 5}}, ec);
    CHECK_HTTP_RESULT(res.headers);
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  // delete
  {
    auto hdr = requests::json::delete_(hc,  urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}});
    CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
    REQUIRE(js);
    CHECK(js->at("headers").at("Content-Type") == "application/json");
  }

  // patch
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::patch(hc, urls::url_view("/patch"), msg, {{}, {false}});
    CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::put(hc, urls::url_view("/put"), msg, {{}, {false}});
    CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;

    CHECK(hdr.headers.result() == beast::http::status::ok);
    REQUIRE(js);
    CHECK(js->at("headers").at("Content-Type") == "application/json");
    CHECK(js->at("json") == msg);
  }

  // post
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::json::post(hc, urls::url_view("/post"), msg, {{}, {false}});
    CHECK_HTTP_RESULT(hdr.headers);

    auto & js = hdr.value;
    CHECK(hdr.headers.result() == beast::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

}

TEST_CASE("sync-connection-request")
{
  SUBCASE("http")  { json_request_connection(false);}
  SUBCASE("https") { json_request_connection(true);}
}

void run_json_tests(error_code ec,
                    requests::connection & hc,
                    urls::url_view url)
{
  requests::json::async_get(
      hc, urls::url_view("/get"),
      requests::request_parameters{requests::headers({{"Test-Header", "it works"}}), {false}},
      tracker(
          [url](error_code ec, requests::json::response<> hdr)
          {
            check_ec(ec);
            auto & hd = hdr.value.at("headers");
            CHECK_HTTP_RESULT(hdr.headers);

            CHECK(hd.at("Host")        == json::value(url.encoded_host()));
            CHECK(hd.at("Test-Header") == json::value("it works"));
          }));


  requests::json::async_get(
      hc, urls::url_view("/redirect-to?url=%2Fget"),
      requests::request_parameters{requests::headers({{"Test-Header", "it works"}}), {false}},
      tracker(
        [url](error_code ec, requests::json::response<> hdr)
        {
          check_ec(ec);
          CHECK_HTTP_RESULT(hdr.headers);
          CHECK(hdr.history.size() == 1u);
          CHECK(hdr.history.at(0u).at(beast::http::field::location) == "/get");

          auto & hd = hdr.value.at("headers");

          CHECK(hd.at("Host")        == json::value(url.encoded_host()));
          CHECK(hd.at("Test-Header") == json::value("it works"));
        }));


  requests::json::async_get(
      hc, urls::url_view("/redirect/10"), {{}, {false, requests::redirect_mode::private_domain, 5}},
      tracker(
          [url](error_code ec, requests::json::response<> res)
          {
            CHECK_HTTP_RESULT(res.headers);
            CHECK(res.history.size() == 5);
            CHECK(res.headers.begin() == res.headers.end());
            CHECK(ec == requests::error::too_many_redirects);
          }));


  requests::json::async_delete(
      hc, urls::url_view("/delete"), json::value{{"test-key", "test-value"}}, {{}, {false}},
      tracker(
        [url](error_code ec, requests::json::response<> hdr)
        {
            check_ec(ec);
            CHECK_HTTP_RESULT(hdr.headers);
            auto & js = hdr.value;
            CHECK(beast::http::to_status_class(hdr.headers.result()) == beast::http::status_class::successful);
            CHECK(js.at("headers").at("Content-Type") == "application/json");
        }));

    requests::json::async_patch(
        hc, urls::url_view("/patch"), json::value {{"test-key", "test-value"}}, {{}, {false}},
        tracker(
          [url](error_code ec, requests::json::response<> hdr)
          {
            check_ec(ec);
            auto & js = hdr.value;
            CHECK_HTTP_RESULT(hdr.headers);
            CHECK(hdr.headers.result() == beast::http::status::ok);
            CHECK(js.at("headers").at("Content-Type") == "application/json");
            CHECK(js.at("json") == json::value{{"test-key", "test-value"}});
          }));

  requests::json::async_put(
        hc, urls::url_view("/put"), json::value{{"test-key", "test-value"}}, {{}, {false}},
        tracker(
            [url](error_code ec, requests::json::response<boost::optional<json::value>> hdr)
            {
              check_ec(ec);
              auto & js = hdr.value;
              CHECK_HTTP_RESULT(hdr.headers);
              CHECK(hdr.headers.result() == beast::http::status::ok);
              REQUIRE(js);
              CHECK(js->at("headers").at("Content-Type") == "application/json");
              CHECK(js->at("json") == json::value{{"test-key", "test-value"}});
            }));

  requests::json::async_post(
      hc, urls::url_view("/post"), json::value{{"test-key", "test-value"}}, {{}, {false}},
      tracker(
          [url](error_code ec, requests::json::response<> hdr)
          {
            check_ec(ec);
            auto & js = hdr.value;
            CHECK_HTTP_RESULT(hdr.headers);
            CHECK(hdr.headers.result() == beast::http::status::ok);
            CHECK(js.at("headers").at("Content-Type") == "application/json");
            CHECK(js.at("json") == json::value{{"test-key", "test-value"}});
          }));
}


TEST_CASE("async-json-request")
{
  urls::url url;
  url.set_host(httpbin());
  asio::io_context ctx;
  asio::ssl::context sslctx{asio::ssl::context_base::tls_client};
  sslctx.set_verify_mode(asio::ssl::verify_peer);
  sslctx.set_default_verify_paths();
  requests::connection conn(ctx, sslctx);

  auto do_the_thing = asio::deferred(
      [&](error_code ec, asio::ip::tcp::resolver::results_type res)
      {
        check_ec(ec);
        asio::ip::tcp::endpoint ep;
        if (!ec)
          ep = res.begin()->endpoint();
        else
          REQUIRE(!res.size());

        return asio::deferred.when(!ec)
            .then(conn.async_connect(ep, asio::deferred))
            .otherwise(asio::post(ctx, asio::append(asio::deferred, ec)));
      });

  SUBCASE("http")
  {
    conn = requests::connection(ctx);
    asio::ip::tcp::resolver rslvr{ctx};
    url.set_scheme("http");
    conn.use_ssl(false);
    CHECK(!conn.uses_ssl());
    conn.set_host(url.encoded_host());

    rslvr.async_resolve(std::string(url.encoded_host().data(), url.encoded_host().size()),
                        "80", do_the_thing)
                       (asio::append(&run_json_tests, std::ref(conn), urls::url_view(url)));
    ctx.run();
  }

  SUBCASE("https")
  {
    conn = requests::connection(ctx, sslctx);
    asio::ip::tcp::resolver rslvr{ctx};
    url.set_scheme("https");
    CHECK(conn.uses_ssl());
    conn.set_host(url.encoded_host());
    rslvr.async_resolve(std::string(url.encoded_host().data(), url.encoded_host().size()),
                        "443", do_the_thing)
                       (asio::append(&run_json_tests, std::ref(conn), urls::url_view(url)));
    ctx.run();
  }
}

TEST_SUITE_END();