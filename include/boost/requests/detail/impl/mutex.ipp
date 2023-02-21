//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_IMPL_MUTEX_IPP
#define BOOST_REQUESTS_DETAIL_IMPL_MUTEX_IPP

#include <boost/requests/detail/mutex.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <condition_variable>

namespace boost
{
namespace requests
{
namespace detail
{

void mutex::async_lock(faux_token_t<void(system::error_code)> tk)
{
  std::lock_guard<std::mutex> lock{mtx_};
  if (try_lock())
    return asio::post(
        exec_,
        asio::append(std::move(tk), system::error_code()));


  auto itr = waiters_.insert(waiters_.end(), std::move(tk));

  auto slot = itr->get_cancellation_slot();
  if (slot.is_connected())
  {
    slot.assign(
        [this, itr](asio::cancellation_type type)
        {
          if (type != asio::cancellation_type::none)
          {
            std::lock_guard<std::mutex> lock{mtx_};
            ignore_unused(lock);
            system::error_code ec;
            BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::operation_aborted);
            asio::defer(exec_, asio::append(std::move(*itr), ec));
            waiters_.erase(itr);
          }
        });
  }
}

void mutex::lock(system::error_code & ec)
{
  if (try_lock())
    return ;

  using token_type = faux_token_t<void(system::error_code)>;
  struct impl final : token_type::base
  {
    using allocator_type = container::pmr::polymorphic_allocator<void>;
    container::pmr::memory_resource * resource;
    asio::any_io_executor executor;
    allocator_type get_allocator() override
    {
      return container::pmr::polymorphic_allocator<void>{resource};
    }

    void resume(faux_token_t<void(system::error_code)> tk, system::error_code ec) override
    {
      done = true;
      this->ec = ec;
      var.notify_all();
    }

    impl(container::pmr::memory_resource * res, asio::any_io_executor executor) : resource(res), executor(executor) {}
    system::error_code ec;
    bool done = false;
    std::condition_variable var;

    void wait(std::unique_lock<std::mutex> & lock)
    {
      var.wait(lock, [this]{ return done;});
    }
  };

  char buf[4096];
  container::pmr::monotonic_buffer_resource res{buf, sizeof(buf)};

  impl ip{&res, get_executor()};

  std::unique_lock<std::mutex> lock(mtx_);
  std::shared_ptr<token_type::base> ptr{&ip, [](impl * ) {}};
  token_type ft{ptr};

  waiters_.push_back(std::move(ft));

  ip.wait(lock);
  ec = ip.ec;
}

void mutex::unlock()
{
  std::lock_guard<std::mutex> lock{mtx_};
  if (waiters_.empty())
    locked_ = false;
  else if (!waiters_.empty())
  {
    auto h = std::move(waiters_.front());
    h.get_cancellation_slot().clear();
    waiters_.pop_front();
    asio::post(exec_, asio::append(std::move(h), system::error_code{}));
  }
}

bool mutex::try_lock()
{
  return !locked_.exchange(true);
}

mutex::~mutex()
{
  if (waiters_.empty())
    return ;
  system::error_code ec;
  BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::operation_aborted);

  auto ww = std::move(waiters_);
  for (auto & h : ww)
  {
    h.get_cancellation_slot().clear();
    asio::dispatch(exec_, asio::append(std::move(h), ec));
  }
}

}
}
}

#endif // BOOST_REQUESTS_DETAIL_IMPL_MUTEX_IPP
