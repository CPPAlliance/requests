//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_IPP
#define BOOST_REQUESTS_IMPL_SESSION_IPP


#include <boost/requests/session.hpp>
#include <boost/requests/detail/define.hpp>

namespace boost {
namespace requests {


auto session::ropen(
    beast::http::verb method,
    urls::url_view url,
    http::fields & headers,
    source & src,
    system::error_code & ec) -> stream
{
  auto opts = options_;

  response_base::history_type history{headers.get_allocator()};


  auto do_ropen =
      [&](http::fields & hd, urls::pct_string_view target, request_options opts) -> stream
  {
    auto p = get_pool(url, ec);
    if (ec)
      return stream{get_executor(), nullptr};

    return p->ropen(method, target, hd, src, opts, &jar_, ec);
  };

  const auto is_secure = (url.scheme_id() == urls::scheme::https)
                         || (url.scheme_id() == urls::scheme::wss);

  auto host = url.encoded_host();

  if (!is_secure && opts.enforce_tls)
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
    return stream{get_executor(), nullptr};
  }

  {
    detail::monotonic_token mv;
    auto cc = jar_.get(host, is_secure, url.encoded_path(), mv);
    if (!cc.empty())
      headers.set(http::field::cookie, cc);
  }


  auto str = do_ropen(headers, url.encoded_target(), opts);

  if (!ec) // all good
    return str;

  if (opts.max_redirects == str.history().size())
    return str;

  urls::url url_cache;
  while (ec == error::forbidden_redirect)
  {
    ec.clear();
    if (str.history().empty())
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
      break;
    }
    opts.max_redirects -= str.history().size();
    const auto & last = str.history().back();
    auto rc = last.base().result();

    auto loc_itr = last.base().find(http::field::location);

    if (opts.max_redirects == 0)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
      break ;
    }

    if (((rc != http::status::moved_permanently)
         && (rc != http::status::found)
         && (rc != http::status::temporary_redirect)
         && (rc != http::status::permanent_redirect))
        || (loc_itr == last.base().end()))
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
      break;
    }

    const auto nurl = interpret_location(url.encoded_resource(), loc_itr->value());
    if (nurl.has_error())
    {
      ec = nurl.error();
      break;
    }

    if (!should_redirect(options_.redirect, url, *nurl))
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
      break ;
    }

    if (nurl->has_authority())
      url = *nurl;
    else
    {
      url_cache = url;
      url_cache.set_encoded_path(nurl->encoded_path());
      url = url_cache;
    }


    {
      detail::monotonic_token mv;
      auto cc = jar_.get(host, is_secure, url.encoded_path(), mv);
      if (!cc.empty())
        headers.set(http::field::cookie, cc);
    }
    history.insert(history.end(),
                   std::make_move_iterator(std::move(str).history().begin()),
                   std::make_move_iterator(std::move(str).history().end()));

    str = do_ropen(headers, url.encoded_target(), opts);
  }
  str.prepend_history(std::move(history));
  return str;
}

auto session::make_request_(http::fields fields) -> requests::request_parameters {
  return requests::request_parameters{
      std::move(fields),
      options_,
      &jar_
  };
}

urls::url session::normalize_(urls::url_view in)
{
  urls::url res;
  if (!in.has_scheme() || in.scheme() == "https")
  {
    res.set_scheme("https");
    res.set_encoded_host(in.encoded_host());
    if (in.has_port())
      res.set_port(in.port());
    return res;
  }
  else if (in.scheme() == "http")
  {
    res.set_scheme("http");
    res.set_encoded_host(in.encoded_host());
    if (in.has_port())
      res.set_port(in.port());
    return res;
  }
  else
    return in; // just do some invalid shit
}


auto
session::get_pool(urls::url_view url, error_code & ec) -> std::shared_ptr<connection_pool>
{
  // can be optimized to ellide the string allocation, blabla (pmr?)
  char buf[1024];
  container::pmr::monotonic_buffer_resource res{buf, sizeof(buf)};
  auto host_key = normalize_(url);

  auto lock = detail::lock(mutex_, ec);
  if (ec)
    return std::shared_ptr<connection_pool>();

  auto itr = pools_.find(host_key);
  if (itr != pools_.end())
    return itr->second;
  else
  {
    auto p = std::make_shared<connection_pool>(get_executor(), sslctx_);
    p->lookup(host_key, ec);
    if (!ec)
    {
      pools_.emplace(host_key, p);
      return p;
    }
  }
  return nullptr;
}

std::shared_ptr<connection_pool> session::async_get_pool_op::resume(
    requests::detail::faux_token_t<step_signature_type> self,
    error_code ec)
{
  BOOST_ASIO_CORO_REENTER(this)
  {
    BOOST_REQUESTS_AWAIT_LOCK(this_->mutex_, lock);

    {
      auto itr = this_->pools_.find(url);
      if (itr != this_->pools_.end())
        return itr->second;
    }
    p = std::make_shared<connection_pool>(this_->get_executor(), this_->sslctx_);
    BOOST_ASIO_CORO_YIELD p->async_lookup(url, std::move(self));
    if (!ec)
    {
      this_->pools_.emplace(url, p);
      return p;
    }
  }
  return nullptr;
}

auto session::async_ropen_op::resume(requests::detail::faux_token_t<step_signature_type> self,
                                     system::error_code & ec,
                                     variant2::variant<variant2::monostate, std::shared_ptr<connection_pool>, stream> s) -> stream
{
  BOOST_ASIO_CORO_REENTER(this)
  {
    headers.set(beast::http::field::host, url.encoded_host_and_port());
    headers.set(beast::http::field::user_agent, "Requests-" BOOST_BEAST_VERSION_STRING);

    if (!is_secure && this_->options_.enforce_tls)
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, error::insecure);
      return stream{get_executor(), nullptr};
    }

    {
      detail::monotonic_token mv;
      auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), mv);
      if (!cc.empty())
        headers.set(http::field::cookie, cc);
    }

    BOOST_ASIO_CORO_YIELD this_->async_get_pool(url, std::move(self));
    if (ec)
      return stream{get_executor(), nullptr};
    BOOST_ASIO_CORO_YIELD variant2::get<1>(s)->async_ropen(method, url.encoded_resource(),
                                           headers, src, opts, &this_->jar_, std::move(self));

    if (!ec || opts.max_redirects == variant2::get<2>(s).history().size())
      return std::move(variant2::get<2>(s));

    while (ec == error::forbidden_redirect)
    {
      ec.clear();
      if (variant2::get<2>(s).history().empty())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
        break;
      }
      opts.max_redirects -= variant2::get<2>(s).history().size();
      if (opts.max_redirects == 0)
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, error::too_many_redirects);
        break ;
      }

      {
        const auto & last = variant2::get<2>(s).history().back();
        auto loc_itr = last.base().find(http::field::location);
        auto rc = last.base().result();
        if (((rc != http::status::moved_permanently)
             && (rc != http::status::found)
             && (rc != http::status::temporary_redirect)
             && (rc != http::status::permanent_redirect))
            || (loc_itr == last.base().end()))
        {
          BOOST_REQUESTS_ASSIGN_EC(ec, error::invalid_redirect);
          break;
        }
        const auto nurl = interpret_location(url.encoded_resource(), loc_itr->value());
        if (nurl.has_error())
        {
          ec = nurl.error();
          break;
        }

        if (!should_redirect(this_->options_.redirect, url, *nurl))
        {
          BOOST_REQUESTS_ASSIGN_EC(ec, error::forbidden_redirect);
          break ;
        }

        if (nurl->has_authority())
          url = *nurl;
        else
        {
          url_cache = url;
          url_cache.set_encoded_path(nurl->encoded_path());
          url = url_cache;
        }

      }

      {
        detail::monotonic_token mv;
        auto cc = this_->jar_.get(url.encoded_host(), is_secure, url.encoded_path(), mv);
        if (!cc.empty())
          headers.set(http::field::cookie, cc);
        else
          headers.erase(http::field::cookie);
      }
      history.insert(history.end(),
                     std::make_move_iterator(std::move(variant2::get<2>(s)).history().begin()),
                     std::make_move_iterator(std::move(variant2::get<2>(s)).history().end()));
      BOOST_ASIO_CORO_YIELD this_->async_get_pool(url, std::move(self));
      if (ec)
        return stream{get_executor(), nullptr};
      BOOST_ASIO_CORO_YIELD variant2::get<1>(s)->async_ropen(method, url.encoded_resource(), headers, src, opts, &this_->jar_, std::move(self));
    }
    variant2::get<2>(s).prepend_history(std::move(history));
    return std::move(variant2::get<2>(s));

  }
  return stream{this_->get_executor(), nullptr};
}

}
}

#include <boost/requests/detail/undefine.hpp>

#endif // BOOST_REQUESTS_IMPL_SESSION_IPP
