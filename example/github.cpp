//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "github-issues.hpp"

#include <iostream>

// printing functions
void list_issues(std::ostream & ostr, std::vector<github::issue> & issues)
{
  for (const auto& issue : issues)
    ostr << issue.url << ": " << issue.title << std::endl;
}

int main(int argc, char * argv[])
{

  auto token = getenv("GITHUB_TOKEN");
  if (token == nullptr)
  {
    printf("Set your github token as an environment variable name 'GITHUB_TOKEN'\n\n"
           "You can get it from https://github.com/settings/tokens\n\n");
    return 1;
  }

  boost::asio::io_context ctx;
  github::issue_client cl{ctx, token};

  for (const auto& issue : cl.list_issues().value)
    std::cout << issue.url << ": " << issue.title << std::endl;

  return 0;
}