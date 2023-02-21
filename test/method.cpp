// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/json.hpp>
#include <boost/requests/download.hpp>
#include <boost/requests/form.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request.hpp>

#include "doctest.h"
#include "string_maker.hpp"

namespace requests = boost::requests;
namespace filesystem = requests::filesystem;
namespace asio = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;
namespace core = boost::core;

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
  MESSAGE(url);
  return url;
}

TEST_SUITE_BEGIN("method");


struct http_maker
{
  urls::url url;
  http_maker(urls::string_view target)
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
  requests::default_options().enforce_tls = false;
  requests::default_options().max_redirects = 5;

  // headers
  {
    auto hdr = requests::request(requests::http::verb::get,
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
    auto hdr = requests::get(u("/get"), requests::headers({{"Test-Header", "it works"}}));

    CHECK_HTTP_RESULT(hdr.headers);
    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // get-redirect
  {
    auto hdr = requests::get(u("/redirect-to?url=%2Fget"), requests::headers({{"Test-Header", "it works"}}));

    CHECK_HTTP_RESULT(hdr.headers);
    CHECK(hdr.history.size() == 1u);
    CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");

    auto hd = as_json(hdr).at("headers");

    CHECK(hd.at("Host")        == json::value(httpbin()));
    CHECK(hd.at("Test-Header") == json::value("it works"));
  }

  // too-many-redirects
  {
    boost::system::error_code ec;
    requests::default_session().options().max_redirects = 3;
    auto res = requests::get(u("/redirect/10"), {}, ec);
    CHECK_HTTP_RESULT(res.headers);
    CHECK(res.history.size() == 3);
    CHECK(res.headers.begin() == res.headers.end());
    CHECK(ec == requests::error::too_many_redirects);
  }

  // download
  {
    const auto target = filesystem::temp_directory_path() / "requests-test.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    auto res = requests::download(u("/image"), {}, target);
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
    auto res = requests::download(u("/redirect-to?url=%2Fimage"), {}, target);
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
    requests::default_options().max_redirects = 3;
    requests::default_session().options().max_redirects = 3;
    const auto target = filesystem::temp_directory_path() / "requests-test.html";
    if (filesystem::exists(target))
      filesystem::remove(target);
    auto res = requests::download(u("/redirect/10"), {}, target, ec);
    CHECK(res.history.size() == 3);
    CHECK_HTTP_RESULT(res.headers);
    CHECK(res.headers.begin() == res.headers.end());

    CHECK(ec == requests::error::too_many_redirects);
    CHECK(!filesystem::exists(target));
  }

  // delete
  {
    auto hdr = requests::delete_(u("/delete"), json::value{{"test-key", "test-value"}}, {});
    CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
  }

  // patch-json
  {
    json::value msg {{"test-key", "test-value"}};
    auto hdr = requests::patch(u("/patch"), msg, {});
    CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // patch-form
  {
    auto hdr = requests::patch(u("/patch"),
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
    auto hdr = requests::put(u("/put"), msg, {});
    CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // put-form
  {
    auto hdr = requests::put(u("/put"),
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
    auto hdr = requests::post(u("/post"), msg, {});
    CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/json");
    CHECK(js.at("json") == msg);
  }

  // post-form
  {
    auto hdr = requests::post(u("/post"),
                       requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
                       {});
    CHECK_HTTP_RESULT(hdr.headers);

    auto js = as_json(hdr);
    CHECK(hdr.headers.result() == requests::http::status::ok);
    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
  }

}


void async_http_pool_request(asio::any_io_executor exec,
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



  requests::async_request(
      http::verb::get, u("/headers"),
      requests::empty{}, requests::headers({{"Test-Header", "it works"}}),
      tracker(exec,
          [url](error_code ec, requests::response hdr)
          {
            // headers

            check_ec(ec);
            auto hd = as_json(hdr).at("headers");
            CHECK_HTTP_RESULT(hdr.headers);

            CHECK(hd.at("Host")        == json::value(url.host_name()));
            CHECK(hd.at("Test-Header") == json::value("it works"));
          }));

  requests::async_get(u("/get"),
            requests::headers({{"Test-Header", "it works"}}),
            tracker(exec,
                [url](error_code ec, requests::response hdr)
                {
                  // headers
                  check_ec(ec);
                  auto hd = as_json(hdr).at("headers");
                  CHECK_HTTP_RESULT(hdr.headers);

                  CHECK(hd.at("Host")        == json::value(url.host_name()));
                  CHECK(hd.at("Test-Header") == json::value("it works"));
                }));

  requests::async_get(u("/redirect-to", "url=/get"),
            requests::headers({{"Test-Header", "it works"}}),
            tracker(exec,
                [url](error_code ec, requests::response hdr)
                {
                  // get-redirect
                  check_ec(ec);
                  CHECK(hdr.history.size() == 1u);
                  CHECK(hdr.history.at(0u).at(requests::http::field::location) == "/get");
                  CHECK_HTTP_RESULT(hdr.headers);

                  auto hd = as_json(hdr).at("headers");

                  CHECK(hd.at("Host")        == json::value(url.host_name()));
                  CHECK(hd.at("Test-Header") == json::value("it works"));
                }));

  requests::async_get(u("/redirect/10"), {},
            tracker(exec,
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

    CHECK_MESSAGE(filesystem::exists(pt), pt);
    CHECK(!filesystem::exists(target));
    requests::async_download(u("/image"), {}, target,
                   tracker(exec,
                       [url, target](error_code ec, requests::response_base res)
                       {
                         // download
                         CHECK_HTTP_RESULT(res.headers);

                         check_ec(ec);
                         CHECK_MESSAGE(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u, res.headers);
                         CHECK_MESSAGE(res.headers.at(requests::http::field::content_type) == "image/png", res.headers);

                         CHECK_MESSAGE(filesystem::exists(target), target);
                         filesystem::remove(target, ec);
                       }));
  }

  {
    const auto target = filesystem::temp_directory_path() / "requests-test-2.png";
    if (filesystem::exists(target))
      filesystem::remove(target);

    CHECK(!filesystem::exists(target));
    requests::async_download(u("/redirect-to", "url=/image"), {}, target,
                   tracker(exec,
                       [url, target](error_code ec, requests::response_base res)
                       {
                         // download-redirect
                         CHECK_HTTP_RESULT(res.headers);

                         check_ec(ec);
                         CHECK(res.history.size() == 1u);
                         CHECK(res.history.at(0u).at(requests::http::field::location) == "/image");

                         CHECK(std::stoull(res.headers.at(requests::http::field::content_length)) > 0u);
                         CHECK(res.headers.at(requests::http::field::content_type) == "image/png");

                         CHECK_MESSAGE(filesystem::exists(target), target);
                         filesystem::remove(target, ec);

                       }));

  }

  requests::async_delete( u("/delete"), json::value{{"test-key", "test-value"}}, {},
               tracker(exec,
                   [url](error_code ec, requests::response hdr)
                   {
                     // delete
                       check_ec(ec);
                       CHECK_HTTP_RESULT(hdr.headers);
                       auto js = as_json(hdr);
                       CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                       CHECK(js.at("headers").at("Content-Type") == "application/json");
                   }));


  requests::async_patch(u("/patch"), json::value{{"test-key", "test-value"}}, {},
              tracker(exec,
                  [url](error_code ec, requests::response hdr)
                  {
                    // patch-json

                      CHECK_HTTP_RESULT(hdr.headers);
                      check_ec(ec);
                      auto js = as_json(hdr);
                      CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                      CHECK(js.at("headers").at("Content-Type") == "application/json");

                  }));



  requests::async_patch(u("/patch"),
              requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
              {},
              tracker(exec,
                  [url](error_code ec, requests::response hdr)
                  {
                    // patch-form
                    check_ec(ec);
                    auto js = as_json(hdr);
                    CHECK(hdr.headers.result() == requests::http::status::ok);
                    CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                    CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});
                    CHECK_HTTP_RESULT(hdr.headers);

                  }));


  requests::async_put(u("/put"), json::value{{"test-key", "test-value"}}, {},
            tracker(exec,
                [url](error_code ec, requests::response hdr)
                {
                  // put-json
                  CHECK_HTTP_RESULT(hdr.headers);

                  check_ec(ec);
                  auto js = as_json(hdr);
                  CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                  CHECK(js.at("headers").at("Content-Type") == "application/json");

                }));



  requests::async_put(u("/put"),
            requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
            {},
            tracker(exec,
                [url](error_code ec, requests::response hdr)
                {
                  // put-form
                  CHECK_HTTP_RESULT(hdr.headers);

                  check_ec(ec);
                  auto js = as_json(hdr);
                  CHECK(hdr.headers.result() == requests::http::status::ok);
                  CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                  CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});

                }));


  requests::async_post(u("/post"), json::value{{"test-key", "test-value"}}, {},
             tracker(exec,
                 [url](error_code ec, requests::response hdr)
                 {
                   // post-json
                   CHECK_HTTP_RESULT(hdr.headers);

                   check_ec(ec);
                   auto js = as_json(hdr);
                   CHECK(requests::http::to_status_class(hdr.headers.result()) == requests::http::status_class::successful);
                   CHECK(js.at("headers").at("Content-Type") == "application/json");

                 }));



  requests::async_post(u("/post"),
             requests::form{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}},
             {},
             tracker(exec,
                 [url](error_code ec, requests::response hdr)
                 {
                   // post-form

                   CHECK_HTTP_RESULT(hdr.headers);

                   check_ec(ec);
                   auto js = as_json(hdr);
                   CHECK(hdr.headers.result() == requests::http::status::ok);
                   CHECK(js.at("headers").at("Content-Type") == "application/x-www-form-urlencoded");
                   CHECK(js.at("form") == json::value{{"foo", "42"}, {"bar", "21"}, {"foo bar" , "23"}});

                 }));
}

TEST_CASE("async-request")
{
  urls::url url;
  url.set_host(httpbin());


  std::vector<urls::url> buffer;

  SUBCASE("http")
  {
    url.set_scheme("http");
    asio::thread_pool tp;
    requests::default_session(tp.get_executor()).options().enforce_tls = false;
    requests::default_session(tp.get_executor()).options().max_redirects = 3;
    async_http_pool_request(tp.get_executor(), url, buffer);
    tp.join();
  }

  SUBCASE("https")
  {
    url.set_scheme("https");
    asio::thread_pool tp;
    requests::default_session(tp.get_executor()).options().enforce_tls = false;
    requests::default_session(tp.get_executor()).options().max_redirects = 3;
    async_http_pool_request(tp.get_executor(), url, buffer);
    tp.join();
  }
}

TEST_SUITE_END();