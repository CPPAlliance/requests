//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/requests/json.hpp>
#include <boost/requests/method.hpp>
#include <boost/requests/request_parameters.hpp>
#include <fstream>
#include <iostream>

using namespace boost;

int main(int argc, char * argv[])
{
  // The github API uses link to create linked lists, so this example uses this.

  auto at_link =
      [](const std::vector<requests::link> & le, core::string_view rel) -> urls::url_view
      {

        const auto itr = std::find_if(
              le.begin(), le.end(),
              [&](const requests::link & l)
              {
                auto v = l.rel().value();
                return std::find(v.begin(), v.end(), rel) != v.end();
              });
        if (itr == le.end())
          return {};
        return itr->url;
      };

  json::array event_array;

  auto u = urls::parse_uri_reference("https://api.github.com/events").value();
  auto r = requests::get(u);
  if (r.ok())
  {
    std::cerr << "Erorr getting events: " << r.headers << r.string_view() << std::endl;
    return 1;
  }

  {
    char buf[8096];
    container::pmr::monotonic_buffer_resource res{buf, 8096};

    auto j = as_json(r, &res);
    const json::array & arr = j.as_array();
    event_array.insert(event_array.end(), arr.begin(), arr.end());
  }

  urls::url next = at_link(r.link().value(), "next");
  const urls::url last = at_link(r.link().value(), "last");

  while (true)
  {
    r = requests::get(next);
    if (r.ok())
    {
      std::cerr << "Erorr getting events: " << r.headers << r.string_view() << std::endl;
      return 1;
    }

    char buf[8096];
    container::pmr::monotonic_buffer_resource res{buf, 8096};

    auto j = as_json(r, &res);
    const json::array & arr = j.as_array();
    event_array.insert(event_array.end(), arr.begin(), arr.end());

    if (next == last)
      break;

    next = at_link(r.link().value(), "next");
  }

  std::ofstream ofs{"events.json"};
  ofs << event_array;

  return 0;
}