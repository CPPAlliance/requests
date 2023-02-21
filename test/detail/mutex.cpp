//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/lock_guard.hpp>
#include <boost/requests/detail/mutex.hpp>

#include <boost/asio.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <thread>

#include "../string_maker.hpp"

namespace asio      = boost::asio;
namespace requests  = boost::requests;
namespace container = boost::container;

using boost::system::error_code;

using requests::detail::mutex;
using requests::detail::lock_guard;

inline void run_impl(asio::io_context & ctx)
{
  ctx.run();
}

inline void run_impl(asio::thread_pool & ctx)
{
  ctx.join();
}

struct basic_main_impl
{
  mutex mtx;
  std::vector< int > seq;

  basic_main_impl(asio::any_io_executor exec) : mtx{exec} {}

};

struct basic_main
{
  struct step_impl : asio::coroutine
  {
    std::vector< int > &v;
    mutex &mtx;
    int i;

    using executor_type = asio::any_io_executor;
    executor_type get_executor() {return mtx.get_executor();}

    step_impl(std::vector< int > &v,
              mutex &mtx,
              int i) : v(v), mtx(mtx), i(i) {}


    std::unique_ptr<asio::steady_timer> tim;

    using completion_signature_type = void(error_code);
    using step_signature_type       = void(error_code, lock_guard);


    void resume(requests::detail::faux_token_t<step_signature_type> self,
                    error_code & ec, lock_guard l = {})
    {
      BOOST_ASIO_CORO_REENTER(this)
      {
        BOOST_ASIO_CORO_YIELD mtx.async_lock(std::move(self));
        v.push_back(i);
        tim = std::make_unique<asio::steady_timer>(mtx.get_executor(), std::chrono::milliseconds(10));
        BOOST_ASIO_CORO_YIELD tim->async_wait(std::move(self));
        v.push_back(i + 1);
      }
    }
  };

  basic_main(asio::any_io_executor exec) : impl_(std::make_unique<basic_main_impl>(exec)) {}

  std::unique_ptr<basic_main_impl> impl_;

  static auto f(std::vector< int > &v, mutex &mtx, int i)
  {
    return boost::requests::detail::faux_run<step_impl>(asio::deferred, std::ref(v), std::ref(mtx), i);
  }


  void operator()(error_code = {})
  {
    asio::experimental::make_parallel_group(
          f(impl_->seq, impl_->mtx, 0),
          f(impl_->seq, impl_->mtx, 3),
          f(impl_->seq, impl_->mtx, 6),
          f(impl_->seq, impl_->mtx, 9))
          .async_wait(asio::experimental::wait_for_all(), std::move(*this));
  }

  void operator()(std::array<std::size_t, 4> order,
                  error_code ec1,  error_code ec2,
                  error_code ec3,  error_code ec4)
  {
    CHECK(!ec1);
    CHECK(!ec2);
    CHECK(!ec3);
    CHECK(!ec4);
    CHECK(impl_->seq.size() == 8);
    CHECK((impl_->seq[0] + 1) == impl_->seq[1]);
    CHECK((impl_->seq[2] + 1) == impl_->seq[3]);
    CHECK((impl_->seq[4] + 1) == impl_->seq[5]);
    CHECK((impl_->seq[6] + 1) == impl_->seq[7]);
  }

};

TEST_SUITE_BEGIN("mutex");

TEST_CASE_TEMPLATE("random", Context, asio::io_context, asio::thread_pool)
{
  asio::thread_pool ctx;
  asio::post(ctx, basic_main{ctx.get_executor()});
  run_impl(ctx);
}



TEST_CASE("rebind_mutex")
{
  asio::io_context ctx;
  auto res = asio::deferred.as_default_on(mutex{ctx.get_executor()});
}

TEST_CASE("sync_lock_mt")
{
  asio::thread_pool ctx;
  mutex mtx{ctx};

  mtx.lock();
  asio::steady_timer tim{ctx, std::chrono::milliseconds(10)};
  tim.async_wait([&](auto){mtx.unlock();});
  mtx.lock();

  mtx.unlock();
  mtx.lock();

  ctx.join();
}



TEST_CASE("sync_lock_mt-io")
{
  asio::io_context ctx;
  mutex mtx{ctx};

  mtx.lock();
  asio::post(ctx, [&]{mtx.unlock();});
  std::thread thr{
      [&]{
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ctx.run();

      }};

  mtx.lock();
  thr.join();
}

TEST_CASE_TEMPLATE("multi_lock", Context, asio::io_context, asio::thread_pool)
{
  Context ctx;
  mutex mtx{ctx};


  run_impl(ctx);
}

TEST_CASE("cancel_twice")
{
  asio::io_context ctx;

  using token_type = boost::requests::detail::faux_token_t<void(boost::system::error_code)>;
  struct impl final : token_type::base
  {
    std::vector<error_code> ecs;

    using allocator_type = container::pmr::polymorphic_allocator<void>;
    allocator_type get_allocator() override
    {
      return container::pmr::polymorphic_allocator<void>{container::pmr::get_default_resource()};
    }

    void resume(boost::requests::detail::faux_token_t<void(error_code)> tk, error_code ec) override
    {
      ecs.push_back(ec);
    }
  };

  impl ip{};
  std::shared_ptr<token_type::base> ptr{&ip, [](impl * ) {}};

  {
      mutex mtx{ctx};
      mtx.async_lock(token_type{ptr});
      mtx.async_lock(token_type{ptr});
      mtx.async_lock(token_type{ptr});
      mtx.async_lock(token_type{ptr});
      mtx.async_lock(token_type{ptr});
      mtx.async_lock(token_type{ptr});
      mtx.async_lock(token_type{ptr});

      ctx.run_for(std::chrono::milliseconds(10));

      mtx.unlock();
      ctx.restart();
      ctx.run_for(std::chrono::milliseconds(10));

      mtx.unlock();
      ctx.restart();
      ctx.run_for(std::chrono::milliseconds(10));

  }
  ctx.restart();
  ctx.run_for(std::chrono::milliseconds(10));


  CHECK(ip.ecs.size() == 7u);
  CHECK(!ip.ecs.at(0));
  CHECK(!ip.ecs.at(1));
  CHECK(!ip.ecs.at(2));

  CHECK(4u == std::count(ip.ecs.begin(), ip.ecs.end(), asio::error::operation_aborted));
}



TEST_CASE("cancel_lock")
{
  asio::io_context ctx;

  using token_type = boost::requests::detail::faux_token_t<void(boost::system::error_code)>;
  struct impl final : token_type::base
  {
    std::vector<error_code> ecs;

    using allocator_type = container::pmr::polymorphic_allocator<void>;
    allocator_type get_allocator() override
    {
      return container::pmr::polymorphic_allocator<void>{container::pmr::get_default_resource()};
    }

    void resume(boost::requests::detail::faux_token_t<void(error_code)> tk, error_code ec) override
    {
      ecs.push_back(ec);
    }
  };
  impl ip{};
  std::shared_ptr<token_type::base> ptr{&ip, [](impl * ) {}};

  {
    mutex mtx{ctx};
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    ctx.run_for(std::chrono::milliseconds(10));

    mtx.unlock();
    mutex mt2{std::move(mtx)};
    ctx.restart();
    ctx.run_for(std::chrono::milliseconds(10));

    mt2.unlock();
    mtx.unlock(); // should do nothing
  }
  ctx.restart();
  ctx.run_for(std::chrono::milliseconds(10));


  CHECK(ip.ecs.size() == 7u);
  CHECK(!ip.ecs.at(0));
  CHECK(!ip.ecs.at(1));
  CHECK(!ip.ecs.at(2));

  CHECK(4u == std::count(ip.ecs.begin(), ip.ecs.end(), asio::error::operation_aborted));
}


TEST_CASE("cancel_one")
{
  asio::io_context ctx;

  using token_type = boost::requests::detail::faux_token_t<void(boost::system::error_code)>;
  struct impl final : token_type::base
  {
    std::vector<error_code> ecs;

    using allocator_type = container::pmr::polymorphic_allocator<void>;
    allocator_type get_allocator() override
    {
      return container::pmr::polymorphic_allocator<void>{container::pmr::get_default_resource()};
    }

    void resume(boost::requests::detail::faux_token_t<void(error_code)> tk, error_code ec) override
    {
      ecs.push_back(ec);
    }
  };
  impl ip{};
  std::shared_ptr<token_type::base> ptr{&ip, [](impl * ) {}};
  asio::cancellation_signal sig;
  ip.slot = sig.slot();
  {


    mutex mtx{ctx};
    mtx.lock();
    mtx.async_lock(token_type{ptr});
    mtx.async_lock(token_type{ptr});
    ctx.run_for(std::chrono::milliseconds(10));
    CHECK(ip.ecs.empty());

    sig.emit(asio::cancellation_type::all);
    ctx.restart();
    ctx.run_for(std::chrono::milliseconds(10));

    REQUIRE(ip.ecs.size() == 1u);
    CHECK(ip.ecs.front() == asio::error::operation_aborted);
  }

  ctx.restart();
  ctx.run_for(std::chrono::milliseconds(10));
  CHECK(ip.ecs.size() == 2u);
}



TEST_SUITE_END();