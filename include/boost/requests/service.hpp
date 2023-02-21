//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SERVICE_HPP
#define BOOST_REQUESTS_SERVICE_HPP

#include <boost/requests/session.hpp>

namespace boost
{
namespace requests
{

namespace detail
{

struct session_service : asio::detail::execution_context_service_base<session_service>
{
  using executor_type = asio::any_io_executor;
  using session_type = session;

  session_service(asio::execution_context & ctx)
      : asio::detail::execution_context_service_base<session_service>(ctx)
  {
  }

  ~session_service()
  {
  }

  void shutdown() override
  {
    if (session_)
      session_->shutdown();
  }

  void destroy()
  {
    session_ = boost::none;
  }

  boost::optional<session> session_;
};

}


inline auto default_session(asio::any_io_executor exec = asio::system_executor()) -> session &
{
  auto & so = asio::use_service<detail::session_service>(exec.context()).session_;
  if (!so)
    so.emplace(exec);
  return *so;
}

}
}

#endif // BOOST_REQUESTS_SERVICE_HPP
