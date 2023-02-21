//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_ASYNC_COROUTINE_HPP
#define BOOST_REQUESTS_DETAIL_ASYNC_COROUTINE_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/cancellation_state.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/resource_adaptor.hpp>
#include <boost/system/error_code.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

template<typename Implementation, typename = typename Implementation::step_signature_type>
struct faux_runner;

template<typename ... Signatures>
struct faux_token_t;

template<>
struct faux_token_t<void()>
{
  using cancellation_slot_type = asio::cancellation_slot;
  cancellation_slot_type get_cancellation_slot() const {BOOST_ASSERT(impl_ != nullptr); return impl_->slot;}

  using allocator_type = container::pmr::polymorphic_allocator<void>;
  allocator_type get_allocator() const {BOOST_ASSERT(impl_ != nullptr); return impl_->get_allocator();}

  void operator()()
  {
    auto & base = *impl_;
    base.resume(std::move(*this));
  }

  struct base
  {
    virtual void resume(faux_token_t<void()> impl) = 0;
    asio::cancellation_slot slot;
    virtual container::pmr::polymorphic_allocator<void> get_allocator() = 0;
  };

  faux_token_t(const faux_token_t & ) = delete;
  faux_token_t(      faux_token_t && lhs) : impl_(lhs.impl_) { }

  explicit faux_token_t(std::shared_ptr<base> impl) : impl_(std::move(impl)) {}
 private:
  template<typename...>
  friend struct faux_token_t;


  std::shared_ptr<base> impl_;

  template<typename Implementation, typename> friend struct faux_runner;
};


template<typename T1>
struct faux_token_t<void(T1)>
{
  using cancellation_slot_type = asio::cancellation_slot;
  cancellation_slot_type get_cancellation_slot() const {BOOST_ASSERT(impl_ != nullptr); return impl_->slot;}

  using allocator_type = container::pmr::polymorphic_allocator<void>;
  allocator_type get_allocator() const {BOOST_ASSERT(impl_ != nullptr); return impl_->get_allocator();}

  void operator()(T1 t1 = {})
  {
    auto & base = *impl_;
    base.resume(std::move(*this), std::move(t1));
  }

  operator faux_token_t<void()> () &&
  {
    return faux_token_t<void()>{impl_};
  }

  struct base : faux_token_t<void()>::base
  {
    virtual void resume(faux_token_t<void(T1)> tk, T1 t1) = 0;
    void resume(faux_token_t<void()> tk)
    {
      resume(faux_token_t<void(T1)>{std::static_pointer_cast<base>(std::move(tk.impl_))}, T1{});
    }
  };

  faux_token_t(const faux_token_t & ) = delete;
  faux_token_t(      faux_token_t && lhs) : impl_(lhs.impl_) { }

  explicit faux_token_t(std::shared_ptr<base> impl) : impl_(std::move(impl)) {}
private:
  template<typename...>
  friend struct faux_token_t;


  std::shared_ptr<base> impl_;

  template<typename Implementation, typename> friend struct faux_runner;
};

template<typename T1, typename T2>
struct faux_token_t<void(T1, T2)>
{
  using cancellation_slot_type = asio::cancellation_slot;
  cancellation_slot_type get_cancellation_slot() const
  {
    BOOST_ASSERT(impl_ != nullptr);
    return impl_->slot;
  }

  using allocator_type = container::pmr::polymorphic_allocator<void>;
  allocator_type get_allocator() const
  {
    BOOST_ASSERT(impl_ != nullptr);
    return impl_->get_allocator();
  }

  void operator()(T1 t1 = {}, T2 t2 = {})
  {
    auto & base = *impl_;
    base.resume(std::move(*this), std::move(t1), std::move(t2));
  }

  struct base : faux_token_t<void(T1)>::base
  {
    virtual void resume(faux_token_t<void(T1, T2)> tk, T1 t1, T2 t2) = 0;
    void resume(faux_token_t<void()> tk) final
    {
      resume(faux_token_t<void(T1, T2)>{std::static_pointer_cast<base>(std::move(tk.impl_))}, T1{}, T2{});
    }
    void resume(faux_token_t<void(T1)> tk, T1 t1)
    {
      resume(faux_token_t<void(T1, T2)>{std::static_pointer_cast<base>(std::move(tk.impl_)) }, std::move(t1), T2{});
    }
  };

  faux_token_t(      faux_token_t && lhs) : impl_(lhs.impl_) { }

  operator faux_token_t<void(T1)> () &&
  {
    return faux_token_t<void(T1)>{std::move(impl_)};
  }

  operator faux_token_t<void()> () &&
  {
    return faux_token_t<void()>{std::move(impl_)};
  }
  explicit faux_token_t(std::shared_ptr<base> impl) : impl_(std::move(impl)) {}

 private:
  template<typename...>
  friend struct faux_token_t;

  std::shared_ptr<base> impl_;
  template<typename Implementation, typename> friend struct faux_runner;
};

struct with_allocator_t {};
constexpr static with_allocator_t with_allocator{};

template<typename Implementation, typename ... Args>
struct faux_runner<Implementation, void(system::error_code, Args...)>
{

  using token_type = faux_token_t<void(system::error_code, Args...)>;
  template<typename Handler>
  struct impl_ final : token_type::base
  {
    void resume(token_type tk, system::error_code ec, Args ... args) override
    {
      using result_type = decltype(impl.resume(std::move(tk), ec, std::move(args)...));
      resume_impl(std::is_void<result_type>{}, std::move(tk), std::move(ec), std::move(args)...);
    }

    void resume_impl(std::true_type, token_type tk, system::error_code ec, Args ... args)
    {
      auto buf = tk.impl_;
      impl.resume(std::move(tk), ec, std::move(args)...);
      if (impl.is_complete())
      {
        if (buf.use_count() == 0u)
          return ;
        auto h = std::move(handler);
        BOOST_ASSERT(buf.use_count()  >= 1);
        auto exec = asio::get_associated_executor(h, impl.get_executor());
        buf = nullptr;
        asio::dispatch(exec, asio::append(std::move(h), ec));
      }
    }

    void resume_impl(std::false_type, token_type tk, system::error_code ec, Args ... args)
    {
      auto buf = tk.impl_;
      decltype(auto) res = impl.resume(std::move(tk), ec, std::move(args)...);
      if (impl.is_complete())
      {
        if (buf.use_count() == 0u)
          return ;
        auto h = std::move(handler);
        BOOST_ASSERT(buf.use_count() >= 1);
        auto tmp = std::move(res);
        auto exec = asio::get_associated_executor(h, impl.get_executor());
        buf = nullptr;
        asio::dispatch(exec, asio::append(std::move(h), ec, std::move(tmp)));
      }
    }

    void initiate(token_type tk)
    {
      using result_type = decltype(impl.resume(std::move(tk), std::declval<system::error_code&>(), std::declval<Args>()...));
      BOOST_ASSERT(tk.impl_);
      initiate_impl(std::is_void<result_type>{}, std::move(tk), {}, Args{}...);
    }

    void initiate_impl(std::true_type, token_type tk, system::error_code ec, Args ... args)
    {
      BOOST_ASSERT(tk.impl_.get() == this);
      auto buf = tk.impl_;
      impl.resume(std::move(tk), ec, std::move(args)...);
      if (impl.is_complete())
      {
        if (buf.use_count() == 0u)
          return ;

        auto h = std::move(handler);
        auto exec = asio::get_associated_executor(h, impl.get_executor());
        buf.reset();
        asio::post(exec, asio::append(std::move(h), ec));
      }
    }

    void initiate_impl(std::false_type, token_type tk, system::error_code ec, Args ... args)
    {
      BOOST_ASSERT(tk.impl_.get() == this);
      auto buf = tk.impl_;
      decltype(auto) res = impl.resume(std::move(tk), ec, std::move(args)...);
      if (impl.is_complete())
      {
        if (buf.use_count() == 0u)
          return ;

        auto h = std::move(handler);
        auto tmp = std::move(res);
        auto exec = asio::get_associated_executor(h, impl.get_executor());
        asio::post(exec,  asio::append(std::move(h), ec, std::move(tmp)));
      }
    }

    Handler handler;
    container::pmr::resource_adaptor_imp<
        typename std::allocator_traits<asio::associated_allocator_t<Handler>>::template rebind_alloc<char>
          > alloc_re{asio::get_associated_allocator(handler)};

    Implementation impl;

    template<typename T>
    typename token_type::allocator_type get_allocator_impl(T )
    {

    }

    template<typename T>
    typename token_type::allocator_type get_allocator_impl(std::allocator<T>)
    {
      return {};
    }

    template<typename T>
    typename token_type::allocator_type get_allocator_impl(container::pmr::polymorphic_allocator<T> alloc)
    {
      return alloc;
    }

    typename token_type::allocator_type get_allocator() override
    {
      return get_allocator_impl(asio::get_associated_allocator(handler));
    }


    template<typename Handler_, typename ... Args_>
    impl_(Handler_ && h, Args_ && ... args)
        : handler(std::forward<Handler_>(h))
        , impl(std::forward<Args_>(args)...)
    {
      this->token_type::base::slot = asio::get_associated_cancellation_slot(handler);
    }


    template<typename Handler_, typename ... Args_>
    impl_(Handler_ && h, const with_allocator_t &, Args_ && ... args)
        : handler(std::forward<Handler_>(h))
        , impl(get_allocator(), std::forward<Args_>(args)...)
    {
      this->token_type::base::slot = asio::get_associated_cancellation_slot(handler);
    }

    template<typename Handler_, typename ... Args_>
    impl_(Handler_ && h, with_allocator_t &&, Args_ && ... args)
        : handler(std::forward<Handler_>(h))
        , impl(get_allocator(), std::forward<Args_>(args)...)
    {
      this->token_type::base::slot = asio::get_associated_cancellation_slot(handler);
    }
  };

  template<typename Handler, typename ... Args_>
  void operator()(Handler && h, Args_ &&... args)
  {
    auto alloc = asio::get_associated_allocator(h, asio::recycling_allocator<void>());
    using impl_t = impl_<std::decay_t<Handler>>;
    auto ptr = std::allocate_shared<impl_t>(alloc, std::forward<Handler>(h), std::forward<Args_>(args)...);
    auto * impl = ptr.get();
    impl->initiate(token_type{std::move(ptr)});
  }


};

template<typename Implementation,
         typename Token,
         typename ... Args>
auto faux_run(Token && token, Args && ... args)
{
  static_assert(std::is_constructible<Implementation, Args&&...>::value,
                "Can't construct implementation from those args");
  return asio::async_initiate<Token, typename Implementation::completion_signature_type>(
      faux_runner<Implementation>{}, token, std::forward<Args>(args)...);
}

template<typename Implementation,
          typename Token,
          typename ... Args>
auto faux_run_with_allocator(Token && token, Args && ... args)
{
  static_assert(std::is_constructible<Implementation,
                container::pmr::polymorphic_allocator<void>,
                Args&&...>::value,
                "Can't construct implementation from those args");
  return asio::async_initiate<Token, typename Implementation::completion_signature_type>(
      faux_runner<Implementation>{}, token, with_allocator, std::forward<Args>(args)...);
}


}
}
}

#endif // BOOST_REQUESTS_DETAIL_ASYNC_COROUTINE_HPP
