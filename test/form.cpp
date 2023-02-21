//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/form.hpp>
#include <boost/requests/sources/form.hpp>

#include <boost/asio/read.hpp>
#include <boost/optional/optional_io.hpp>
#include <fstream>

#include "doctest.h"
#include "string_maker.hpp"

using namespace boost;

// language=http
static auto cmp =
"--01234567890123456789012345678901\r\n"
"Content-Disposition: form-data; name=\"text-field\"\r\n"
"Content-Type: text/plain; charset=utf-8\r\n"
"\r\n"
"Test\r\n"
"--01234567890123456789012345678901\r\n"
"Content-Disposition: form-data; name=\"box\"\r\n"
"Content-Type: text/plain; charset=utf-8\r\n"
"\r\n"
"on\r\n"
"--01234567890123456789012345678901\r\n"
"Content-Disposition: form-data; name=\"my-file\"; filename=\"form-test.txt\"\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"test-string2\r\n"
"--01234567890123456789012345678901--";

TEST_SUITE_BEGIN("form");
TEST_CASE("multi-part")
{
  filesystem::path pt = filesystem::temp_directory_path() / "form-test.txt";
  std::ofstream(pt.string()) << "test-string2";

  constexpr char boundary[33] = "01234567890123456789012345678901";

  requests::multi_part_form mpf{{"text-field", "Test"}, {"box", "on"}, {"my-file", pt}};
  requests::multi_part_form_source bd{mpf};
  std::copy(std::begin(boundary), std::end(boundary) - 1, bd.boundary_and_type.begin() + 30);

  REQUIRE(bd.current == bd.mpf.storage.cbegin());
  auto sz = bd.size();

  std::string data;

  for (;;)
  {
    char buf[4096];
    boost::system::error_code ec;
    auto rr = bd.read_some(&buf[0], sizeof(buf), ec);
    REQUIRE(ec == boost::system::error_code{});
    data.append(buf, rr.first);
    if (!rr.second)
      break;
  }

  CHECK(sz == data.size());
  CHECK(data.size() == string_view(cmp).size());
  CHECK(data == string_view(cmp));
}

TEST_SUITE_END();