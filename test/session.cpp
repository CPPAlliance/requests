// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/requests/download.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/session.hpp>
#include <boost/requests/form.hpp>
#include <boost/json.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

#include "doctest.h"
#include "string_maker.hpp"

namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace core = boost::core;
namespace urls = boost::urls;

using boost::system::error_code;
#if defined(BOOST_REQUESTS_USE_STD_FS)
using fs_error_code = std::error_code;
#else
using fs_error_code = boost::system::error_code;
#endif



inline std::string httpbin()
{
  std::string url = "httpbin.org";
  if (auto p = ::getenv("BOOST_REQUEST_HTTPBIN"))
    url = p;
  return url;
}


TEST_SUITE_BEGIN("session");


struct http_maker
{
  urls::url url;
  http_maker(boost::core::string_view target)
  {
    url = urls::parse_uri("http://" + httpbin() + std::string{target}).value();

  }

  operator urls::url_view () const
  {
    return url;
  }

  operator json::value () const
  {
    return json::string(url.buffer());
  }
};

struct https_maker
{
  urls::url url;
  https_maker(boost::core::string_view target)
  {
    url = urls::parse_uri("https://" + httpbin() + std::string{target}).value();
  }

  operator urls::url_view () const
  {
    return url;
  }
  operator json::value () const
  {
    return json::string(url.buffer());
  }
};

TYPE_TO_STRING(http_maker);
TYPE_TO_STRING(https_maker);

TEST_CASE_TEMPLATE("sync-request", u, http_maker, https_maker)
{
  asio::io_context ctx;

  requests::session hc{ctx};
  hc.options().enforce_tls = false;
  hc.options().max_redirects = 5;

  // headers
  {
    auto hdr = request(hc, requests::http::verb::get,
                          u("/headers"),
                          requests::empty{},
                          requests::headers({{"Test-Header", "it works"}}));
    CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }


  // get
  {
    auto hdr = get(hc, u("/get"), requests::headers({{"Test-Header", "it works"}}));
    CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // get-redirect
  {
    auto hdr = get(hc, u("/redirect-to?url=%2Fget"), requests::headers({{"Test-Header", "it works"}}));

    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // too-many-redirects
  {
    error_code ec;
    auto res = get(hc, u("/redirect/10"), {}, ec);
    CHECK_HTTP_RESULT(res.headers);
    CHECK(res.history.size() == 5);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  // download
  {
    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    auto res = download(hc, u("/image"), {}, target);

    CHECK_HTTP_RESULT(res.headers);
    CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    CHECK_MESSAGE(filesystem::exists(target), target);
    fs_error_code ec;
    filesystem::remove(target, ec);
  }


  // download-redirect
  {

    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    auto res = download(hc, u("/redirect-to?url=%2Fimage"), {}, target);
    CHECK_HTTP_RESULT(res.headers);
    CHECK(res.history.size() == 1u);
    CHECK(res.history.at(0u).at(requests::http::field::location) == "/image");

    CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
    CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

    CHECK_MESSAGE(filesystem::exists(target), target);
    fs_error_code ec;
    filesystem::remove(target, ec);
  }


  // download-too-many-redirects
  {
    error_code ec;
    hc.options().max_redirects = 3;
    const auto target = filesystem::temp_directory_path() / "requests-test.html";
    if (filesystem::exists(target))
      filesystem::remove(target);
    auto res = download(hc, u("/redirect/10"), {}, target, ec);
    CHECK(res.history.size() == 3);
    CHECK_HTTP_RESULT(res.headers);
    CHECK(res.headers.begin() == res.headers.end());

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!filesystem::exists(target));
  }

  // delete
  {
    auto hdr = delete_(hc, u("/delete"), json::value{{"test-key", "test-value"}}, {});
    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // patch-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = patch(hc, u("/patch"), msg, {});
    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // patch-form
  {
    auto hdr = patch(hc, u("/patch"),
                        requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                        {});

    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  // put-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = put(hc, u("/put"), msg, {});

    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // put-form
  {
    auto hdr = put(hc, u("/put"),
                      requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                      {});

    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

  // post-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = post(hc, u("/post"), msg, {});

    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // post-form
  {
    auto hdr = post(hc, u("/post"),
                       requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                       {});

    CHECK_HTTP_RESULT(hdr.headers);
    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

}


void async_http_pool_request(requests::session & sess,
                             urls::url_view url,
                             std::vector<urls::url> & buffer)
{
  namespace http = requests::http;
  namespace filesystem = requests::filesystem;

  auto u =
      [&](core::string_view path, core::string_view query = "") -> urls::url_view
      {
        buffer.emplace_back(url);
        auto & u = buffer.back();
        u.set_path(path);
        u.set_query(query);
        return u;
      };

  async_request(
      sess,
      http::verb::get, u("/headers"),
      requests::empty{}, requests::headers({{"Test-Header", "it works"}}),
      tracker(
          [url](error_code ec, requests::response hdr)
          {
            // headers
            check_ec(ec);
            auto hd = as_json(hdr).at("headers");
            CHECK_HTTP_RESULT(hdr.headers);

            CHECK(hd.at("Host")        == json::value(url.host_name()));
            CHECK(hd.at("Test-Header") == json::value("it works"));
          }));

  async_get(sess,
            u("/get"),
            requests::headers({{"Test-Header", "it works"}}),
            tracker(
                [url](error_code ec, requests::response hdr)
                {
                  // headers
                  check_ec(ec);
                  CHECK_HTTP_RESULT(hdr.headers);
                  auto hd = as_json(hdr).at("headers");
                  CHECK_HTTP_RESULT(hdr.headers);

                  CHECK(hd.at("Host")        == json::value(url.host_name()));
                  CHECK(hd.at("Test-Header") == json::value("it works"));
                }));

  async_get(sess, u("/redirect-to", "url=/get"),
            requests::headers({{"Test-Header", "it works"}}),
            tracker(
                [url](error_code ec, requests::response hdr)
                {
                  // get-redirect
                  CHECK_HTTP_RESULT(hdr.headers);
                  CHECK(hdr.history.size() == 1u);
                  CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

                  auto hd = as_json(hdr).at("headers");

                  CHECK(hd.at("Host")        == json::value(url.host_name()));
                  CHECK(hd.at("Test-Header") == json::value("it works"));
                }));

  async_get(sess, u("/redirect/10"), {},
            tracker(
                [url](error_code ec, requests::response res)
                {
                  // too-many-redirects
                  CHECK_HTTP_RESULT(res.headers);

                  CHECK(res.history.size() == 3);
                  CHECK(res.headers.begin() == res.headers.end());
                  CHECK(ec == requests::error::too_many_redirects);

                }));

  {
    auto pt = filesystem::temp_directory_path();
    const auto target = pt / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    async_download(sess, u("/image"), {}, target,
                   tracker(
                       [url, target](error_code ec, requests::response_base res)
                       {
                         // download
                         CHECK_HTTP_RESULT(res.headers);
                         CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
                         CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

                         CHECK_MESSAGE(filesystem::exists(target), target);
                         fs_error_code ec_;
                         filesystem::remove(target, ec_);

                       }));
  }

  {
    const auto target = filesystem::temp_directory_path() / "requests-test-2.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    async_download(sess, u("/redirect-to", "url=/image"), {}, target,
                   tracker(
                       [url, target](error_code ec, requests::response_base res)
                       {
                         // download-redirect
                         CHECK_HTTP_RESULT(res.headers);

                         CHECK(res.history.size() == 1u);
                         CHECK(res.history.at(0u).at(requests::http::field::location) == "/image");

                         CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
                         CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

                         CHECK_MESSAGE(filesystem::exists(target), target);
                         fs_error_code ec_;
                         filesystem::remove(target, ec_);

                       }));

  }

  async_delete(sess,  u("/delete"), json::value{{"test-key", "test-value"}}, {},
               tracker(
                   [url](error_code ec, requests::response hdr)
                   {
                     // delete
                     CHECK_HTTP_RESULT(hdr.headers);

                     auto js = as_json(hdr);
                     CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                     CHECK(js.at("headers").at("Content-Type") == "application/json");

                   }));


  async_patch(sess, u("/patch"), json::value{{"test-key", "test-value"}}, {},
              tracker(
                  [url](error_code ec, requests::response hdr)
                  {
                    // patch-json
                    CHECK_HTTP_RESULT(hdr.headers);

                    check_ec(ec);
                    auto js = as_json(hdr);
                    CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                    CHECK(js.at("headers").at("Content-Type") == "application/json");

                  }));



  async_patch(sess, u("/patch"),
              requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
              {},
              tracker(
                  [url](error_code ec, requests::response hdr)
                  {
                    // patch-form
                    CHECK_HTTP_RESULT(hdr.headers);

                    check_ec(ec);
                    auto js = as_json(hdr);
                    CHECK(hdr.headers.result() == requests::http::status::ok);
                    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});

                  }));


  async_put(sess, u("/put"), json::value{{"test-key", "test-value"}}, {},
            tracker(
                [url](error_code ec, requests::response hdr)
                {
                  check_ec(ec);
                  // put-json
                  CHECK_HTTP_RESULT(hdr.headers);

                  check_ec(ec);
                  auto js = as_json(hdr);
                  CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                  CHECK(js.at("headers").at("Content-Type") == "application/json");

                }));



  async_put(sess, u("/put"),
            requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
            {},
            tracker(
                [url](error_code ec, requests::response hdr)
                {
                  check_ec(ec);
                  // put-form
                  CHECK_HTTP_RESULT(hdr.headers);

                  check_ec(ec);
                  auto js = as_json(hdr);
                  CHECK(hdr.headers.result() == requests::http::status::ok);
                  CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                  CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});

                }));


  async_post(sess, u("/post"), json::value{{"test-key", "test-value"}}, {},
             tracker(
                 [url](error_code ec, requests::response hdr)
                 {
                   check_ec(ec);
                   // post-json
                   CHECK_HTTP_RESULT(hdr.headers);
                   auto js = as_json(hdr);
                   CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                   CHECK(js.at("headers").at("Content-Type") == "application/json");

                 }));



  async_post(sess, u("/post"),
             requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
             {},
             tracker(
                 [url](error_code ec, requests::response hdr)
                 {
                   // post-form

                   check_ec(ec);
                   CHECK_HTTP_RESULT(hdr.headers);

                   auto js = as_json(hdr);
                   CHECK(hdr.headers.result() == requests::http::status::ok);
                   CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                   CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});

                 }));
}

TEST_CASE("async-session-request")
{
  urls::url url;
  url.set_host(httpbin());
  asio::io_context ctx;
  requests::session hc{ctx};
  hc.options().enforce_tls = false;
  hc.options().max_redirects = 3;

  std::vector<urls::url> buffer;

  SUBCASE("http")
  {
    url.set_scheme("http");
    async_http_pool_request(hc, url, buffer);
    ctx.run();
  }

  SUBCASE("https")
  {
    url.set_scheme("https");
    async_http_pool_request(hc, url, buffer);
    ctx.run();

  }
}

TEST_SUITE_END();