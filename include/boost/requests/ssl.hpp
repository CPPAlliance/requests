// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_REQUESTS_SSL_HPP
#define BOOST_REQUESTS_SSL_HPP

#include <boost/requests/detail/ssl.hpp>

namespace boost
{
namespace requests
{


inline auto default_ssl_context(asio::any_io_executor exec = asio::system_executor()) -> asio::ssl::context &
{
  return asio::use_service<detail::ssl_context_service>(exec.context()).get();
}


}
}

#endif // BOOST_REQUESTS_SSL_HPP

