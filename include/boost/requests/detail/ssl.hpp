//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_SSL_HPP
#define BOOST_REQUESTS_DETAIL_SSL_HPP

#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/requests/detail/config.hpp>

extern "C" { typedef struct ssl_st SSL; }

namespace boost {
namespace requests {
namespace detail {

template<typename Stream>
bool verify_host(Stream & str, const std::string &host)
{
  return verify_host_impl(get_ssl_layer(str), host);
}

struct ssl_context_service : asio::detail::execution_context_service_base<ssl_context_service>
{
  ssl_context_service(asio::execution_context & ctx)
    : asio::detail::execution_context_service_base<ssl_context_service>(ctx)
  {
    context.set_default_verify_paths();
  }


  asio::ssl::context context{asio::ssl::context_base::tlsv13_client};

  asio::ssl::context & get() {return context;}

  void shutdown() override
  {
  }
};



}
}
}

#endif // BOOST_REQUESTS_DETAIL_SSL_HPP
