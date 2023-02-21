//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_POOL_IPP
#define BOOST_REQUESTS_IMPL_CONNECTION_POOL_IPP

#include <boost/requests/connection_pool.hpp>

namespace boost {
namespace requests {


void connection_pool::lookup(urls::url_view sv, system::error_code & ec)
{
  urls::string_view scheme = "https";
  if (sv.has_scheme())
    scheme = sv.scheme();

  detail::lock_guard lock;

  if (scheme == "unix")
    // all good, no lookup needed
  {

    lock =  detail::lock(mutex_, ec);
    if (ec)
      return;

    use_ssl_ = false;
    host_ = "localhost";
    endpoints_ = {asio::local::stream_protocol::endpoint(
                        std::string(
                          sv.encoded_target().data(),
                          sv.encoded_target().size()
                        ))};
  }
  else if (scheme == "http" || scheme == "https")
  {
    asio::ip::tcp::resolver resolver{get_executor()};
    const auto service = sv.has_port() ? sv.port() : scheme;
    auto eps = resolver.resolve(
        std::string(sv.encoded_host_name().data(), sv.encoded_host_name().size()),
        std::string(service.data(), service.size()), ec);

    if (!ec && eps.empty())
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found)
    if (ec)
      return;

    lock =  detail::lock(mutex_, ec);
    if (ec)
      return;
    use_ssl_ = scheme == "https";
    host_ = eps->host_name();
    const auto r =
        boost::adaptors::transform(
            eps,
            [](const typename asio::ip::tcp::resolver::results_type::value_type  res)
            {
              return res.endpoint();
            });

    endpoints_.assign(r.begin(), r.end());
  }
  else
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::invalid_argument);
    return ;
  }

  // clean up!
  for (auto itr = conns_.begin(); itr != conns_.end(); )
  {
    auto it = std::find(endpoints_.begin(), endpoints_.end(), itr->first);
    if (it != endpoints_.end())
      itr ++;
    else
      itr = conns_.erase(itr);
  }
}



void connection_pool::async_lookup_op::resume(requests::detail::faux_token_t<step_signature_type> self,
                                              system::error_code & ec, typename asio::ip::tcp::resolver::results_type eps)
{
  BOOST_ASIO_CORO_REENTER(this)
  {
    if (sv.has_scheme())
      scheme = sv.scheme();

    if (scheme == "unix")
      // all good, no lookup needed
    {

      if (!this_->mutex_.try_lock())
      {
        BOOST_ASIO_CORO_YIELD this_->mutex_.async_lock(std::move(self));
      }
      if (ec)
        return;
      lock = lock_type{this_->mutex_, std::adopt_lock};

      this_->use_ssl_ = false;
      this_->host_ = "localhost";
      this_->endpoints_ = {asio::local::stream_protocol::endpoint(
          std::string(sv.encoded_target().data(), sv.encoded_target().size()))};
    }
    else if (scheme == "http" || scheme == "https")
    {
      resolver.emplace(get_executor());
      service = sv.has_port() ? sv.port() : scheme;
      BOOST_ASIO_CORO_YIELD resolver->async_resolve(
          std::string(sv.encoded_host_name().data(), sv.encoded_host_name().size()),
          std::string(service.data(), service.size()), std::move(self));

      if (!ec && eps.empty())
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found)
      if (ec)
        return;

      if (!this_->mutex_.try_lock())
      {
        BOOST_ASIO_CORO_YIELD this_->mutex_.async_lock(std::move(self));
      }
      if (ec)
        return;
      lock = lock_type{this_->mutex_, std::adopt_lock};
      this_->use_ssl_ = scheme == "https";
      this_->host_ = eps->host_name();
      const auto r =
          boost::adaptors::transform(
              eps,
              [](const typename asio::ip::tcp::resolver::results_type::value_type  res)
              {
                return res.endpoint();
              });

      this_->endpoints_.assign(r.begin(), r.end());
    }
    else
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::invalid_argument);
      return ;
    }

    // clean up!
    for (auto itr = this_->conns_.begin(); itr != this_->conns_.end(); )
    {
      auto it = std::find(this_->endpoints_.begin(), this_->endpoints_.end(), itr->first);
      if (it != this_->endpoints_.end())
        itr ++;
      else
        itr = this_->conns_.erase(itr);
    }
  }
}


auto connection_pool::get_connection(error_code & ec) -> connection
{

  auto lock = detail::lock(mutex_, ec);
  if (ec)
    return connection();

  // find an idle connection
  auto itr = std::find_if(conns_.begin(), conns_.end(),
                          [](const std::pair<const endpoint_type, std::shared_ptr<detail::connection_impl>> & conn)
                          {
                            return (conn.second->working_requests() == 0u) && conn.second->is_open();
                          });

  if (itr != conns_.end())
    return connection(itr->second);

  // check if we can make more connections. -> open another connection.
  // the race here is that we might open one too many
  if (conns_.size() <= limit_) // open another connection then -> we block the entire
  {
    if (endpoints_.empty())
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
      return connection();
    }

    //sort the endpoints by connections that use it
    std::sort(endpoints_.begin(), endpoints_.end(),
              [this](const endpoint_type & a, const endpoint_type & b)
              {
                return conns_.count(a) < conns_.count(b);
              });
    const auto ep = endpoints_.front();

    std::shared_ptr<detail::connection_impl> nconn = std::make_shared<detail::connection_impl>(get_executor(), context_);
    nconn->use_ssl(use_ssl_);
    nconn->set_host(host_);
    nconn->connect(ep, ec);
    if (ec)
      return connection();

    if (ec)
      return connection();

    conns_.emplace(ep, nconn);
    return connection(nconn);

  }

  // find the one with the lowest usage
  itr = std::min_element(conns_.begin(), conns_.end(),
                         [](const std::pair<const endpoint_type, std::shared_ptr<detail::connection_impl>> & lhs,
                            const std::pair<const endpoint_type, std::shared_ptr<detail::connection_impl>> & rhs)
                         {
                           return (lhs.second->working_requests() + (lhs.second->is_open() ? 0 : 1))
                                < (rhs.second->working_requests() + (rhs.second->is_open() ? 0 : 1));
                         });
  if (itr == conns_.end())
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
    return connection();
  }
  else
    return connection(itr->second);
}


auto connection_pool::async_get_connection_op::resume(
            requests::detail::faux_token_t<step_signature_type> self,
            system::error_code & ec) -> connection
{
  BOOST_ASIO_CORO_REENTER (this)
  {
    if (!this_->mutex_.try_lock())
    {
      BOOST_ASIO_CORO_YIELD this_->mutex_.async_lock(std::move(self));
    }
    if (ec)
      return connection();

    lock = {this_->mutex_, std::adopt_lock};

    // find an idle connection
    itr = std::find_if(this_->conns_.begin(), this_->conns_.end(),
                       [](const std::pair<const endpoint_type, std::shared_ptr<detail::connection_impl>> & conn)
                       {
                         return (conn.second->working_requests() == 0u) && conn.second->is_open();;
                       });

    if (itr != this_->conns_.end())
      return connection(itr->second);

    // check if we can make more connections. -> open another connection.
    // the race here is that we might open one too many
    if (this_->conns_.size() < this_->limit_) // open another connection then -> we block the entire
    {
      if (this_->endpoints_.empty())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
        return connection();
      }

      //sort the endpoints by connections that use it
      std::sort(this_->endpoints_.begin(), this_->endpoints_.end(),
                [this](const endpoint_type & a, const endpoint_type & b)
                {
                  return this_->conns_.count(a) < this_->conns_.count(b);
                });
      ep = this_->endpoints_.front();
      nconn = std::make_shared<detail::connection_impl>(get_executor(), this_->context_);
      nconn->use_ssl(this_->use_ssl_);
      nconn->set_host(this_->host_);
      BOOST_ASIO_CORO_YIELD nconn->async_connect(ep, std::move(self)); // don't unlock here.
      if (ec)
        return connection();

      this_->conns_.emplace(ep, nconn);
      return connection(std::move(nconn));
    }
    // find the one with the lowest usage
    itr = std::min_element(this_->conns_.begin(), this_->conns_.end(),
                           [](const std::pair<const endpoint_type, std::shared_ptr<detail::connection_impl>> & lhs,
                              const std::pair<const endpoint_type, std::shared_ptr<detail::connection_impl>> & rhs)
                           {
                             return (lhs.second->working_requests() + (lhs.second->is_open() ? 0 : 1))
                                  < (rhs.second->working_requests() + (rhs.second->is_open() ? 0 : 1));
                           });
    if (itr == this_->conns_.end())
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
      return connection();
    }
    else
      return connection(itr->second);
  }
  return connection();
}

stream connection_pool::async_ropen_op::resume(
    requests::detail::faux_token_t<step_signature_type> self,
    boost::system::error_code & ec,
    variant2::variant<variant2::monostate, connection, stream> res)
{
  BOOST_ASIO_CORO_REENTER(this)
  {
    BOOST_ASIO_CORO_YIELD this_->async_get_connection(std::move(self));
    conn = std::move(variant2::get<1>(res));
    if (!ec && !conn)
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
    if (ec)
      return stream{this_->get_executor(), nullptr};

    BOOST_ASIO_CORO_YIELD conn.async_ropen(method, path, headers, src, std::move(opt), jar, std::move(self));
    return variant2::get<2>(std::move(res));
  }
  return stream{get_executor(), nullptr};
}

}
}


#endif // BOOST_REQUESTS_IMPL_CONNECTION_POOL_IPP
