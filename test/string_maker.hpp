//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef STRING_MAKER_HPP
#define STRING_MAKER_HPP


#include "doctest.h"
#include <boost/core/demangle.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/requests/fields/set_cookie.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/multiple_exceptions.hpp>
#include <boost/system/result.hpp>
#include <boost/json.hpp>


namespace doctest
{


template<>
struct StringMaker<std::type_info >
{
  static String convert(const std::type_info  & t)
  {
    return String(boost::core::demangle(t.name()).c_str());
  }
};

template<>
struct StringMaker<boost::core::string_view>
{
  static String convert(boost::core::string_view sv)
  {
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::system::error_code>
{
  static String convert(boost::system::error_code ec)
  {
    if (ec.has_location())
        return toString(
          ec.location().to_string() + " [" + std::to_string(ec.value()) + "/" + ec.category().name() + "]: " + ec.message());
    else
        return toString("[" + std::to_string(ec.value()) + "/" + ec.category().name() + "]: " +  ec.message());
  }
};

template<typename T>
struct StringMaker<boost::system::result<T>>
{
  static String convert(const boost::system::result<T> & res)
  {
    if(res.has_value())
      return toString(*res);
    else
      return toString(res.error());
  }
};


template<>
struct StringMaker<boost::requests::set_cookie::extensions_type>
{
  static String convert(const boost::requests::set_cookie::extensions_type & res)
  {
    std::string val;
    for (auto && attr : res)
    {
      val += attr;
      val += "; ";
    }
    return String(val.data(), val.size());
  }
};

template<>
struct StringMaker<boost::json::value>
{
  static String convert(const boost::json::value & v)
  {
    auto sv = boost::json::serialize(v);
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::json::array>
{
  static String convert(const boost::json::array & v)
  {
    auto sv = boost::json::serialize(v);
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::json::object>
{
  static String convert(const boost::json::object & v)
  {
    auto sv = boost::json::serialize(v);
    return String(sv.data(), sv.size());
  }
};

template<>
struct StringMaker<boost::json::string>
{
  static String convert(const boost::json::string & sv)
  {
    return String(sv.data(), sv.size());
  }
};


template<>
struct StringMaker<std::exception_ptr>
{
  static String convert(std::exception_ptr e)
  {
    if (e)
        try
        {
          std::rethrow_exception(e);
        }
        catch(boost::asio::multiple_exceptions & me)
        {
           return convert(me.first_exception());

        }
        catch(std::exception & ex)
        {
          return ex.what();
        }
    else
        return "<null>";
  }
};

}

inline void check_ec(boost::system::error_code ec,
                     boost::source_location loc = BOOST_CURRENT_LOCATION)
{
  doctest::detail::ResultBuilder rb(doctest::assertType::DT_REQUIRE, loc.file_name(), loc.line(), loc.function_name());
  rb.setResult(doctest::detail::Result{!ec, doctest::StringMaker< boost::system::error_code>::convert(ec)});
  DOCTEST_ASSERT_LOG_AND_REACT(rb);
}

template<typename Handler>
struct tracker_t
{
  Handler handler;
  boost::source_location loc;
  bool called = false;
  bool moved_from = false;
  tracker_t(
      Handler && handler,
      const boost::source_location & loc = BOOST_CURRENT_LOCATION)
      : handler(std::forward<Handler>(handler)), loc(loc) {}

  template<typename ... Args>
  void operator()(Args && ... args)
  {
    called = true;
    std::move(handler)(std::forward<Args>(args)...);
  }

  tracker_t(tracker_t && lhs) : handler(std::move(lhs.handler)), loc(lhs.loc), called(lhs.called)
  {
    lhs.moved_from = true;
  }

  ~tracker_t()
  {
    if (!moved_from)
    {
      doctest::detail::ResultBuilder rb(doctest::assertType::DT_CHECK, loc.file_name(), loc.line(), loc.function_name());
      rb.setResult(doctest::detail::Result{called, "called"});
      DOCTEST_ASSERT_LOG_AND_REACT(rb);
    }
  }
};

template<typename Handler>
auto tracker(Handler && handler, const boost::source_location & loc = BOOST_CURRENT_LOCATION) -> tracker_t<std::decay_t<Handler>>
{
  return tracker_t<std::decay_t<Handler>>(std::forward<Handler>(handler), loc);
}


template<typename Handler>
auto tracker(boost::asio::any_io_executor exec, Handler && handler,
             const boost::source_location & loc = BOOST_CURRENT_LOCATION) ->
    boost::asio::executor_binder<tracker_t<std::decay_t<Handler>>, boost::asio::any_io_executor>
{
  return boost::asio::bind_executor(exec, tracker_t<std::decay_t<Handler>>(std::forward<Handler>(handler), loc));
}

#define CHECK_HTTP_RESULT(Response) \
  CHECK_MESSAGE(boost::requests::http::to_status_class(Response.result()) == requests::http::status_class::successful, Response)


#endif