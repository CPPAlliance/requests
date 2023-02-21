//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_RFC_IMPL_DATES_IPP
#define BOOST_REQUESTS_RFC_IMPL_DATES_IPP

#include <boost/requests/rfc/dates.hpp>
#include <boost/requests/grammar/fixed_token_rule.hpp>

namespace boost
{
namespace requests
{
namespace rfc
{

namespace detail
{

constexpr auto wkday = boost::urls::grammar::variant_rule(
    // epoch was a thursday
    boost::urls::grammar::literal_rule("Thu"),
    boost::urls::grammar::literal_rule("Fri"),
    boost::urls::grammar::literal_rule("Sat"),
    boost::urls::grammar::literal_rule("Sun"),
    boost::urls::grammar::literal_rule("Mon"),
    boost::urls::grammar::literal_rule("Tue"),
    boost::urls::grammar::literal_rule("Wed")
);


constexpr auto weekday = boost::urls::grammar::variant_rule(
    // epoch was a thursday
    boost::urls::grammar::literal_rule("Thursday"),
    boost::urls::grammar::literal_rule("Friday"),
    boost::urls::grammar::literal_rule("Saturday"),
    boost::urls::grammar::literal_rule("Sunday"),
    boost::urls::grammar::literal_rule("Monday"),
    boost::urls::grammar::literal_rule("Tuesday"),
    boost::urls::grammar::literal_rule("Wednesday")
);

constexpr auto month =  boost::urls::grammar::variant_rule(
    boost::urls::grammar::literal_rule("Jan"),
    boost::urls::grammar::literal_rule("Feb"),
    boost::urls::grammar::literal_rule("Mar"),
    boost::urls::grammar::literal_rule("Apr"),
    boost::urls::grammar::literal_rule("May"),
    boost::urls::grammar::literal_rule("Jun"),
    boost::urls::grammar::literal_rule("Jul"),
    boost::urls::grammar::literal_rule("Aug"),
    boost::urls::grammar::literal_rule("Sep"),
    boost::urls::grammar::literal_rule("Oct"),
    boost::urls::grammar::literal_rule("Nov"),
    boost::urls::grammar::literal_rule("Dec")
);



using grammar::fixed_token_rule;
constexpr auto time =
    urls::grammar::tuple_rule(
        fixed_token_rule<2u>(urls::grammar::digit_chars),
        urls::grammar::squelch(urls::grammar::literal_rule(":")),
        fixed_token_rule<2u>(urls::grammar::digit_chars),
        urls::grammar::squelch(urls::grammar::literal_rule(":")),
        fixed_token_rule<2u>(urls::grammar::digit_chars)
    );

template<typename T>
inline auto interpret_result(const T raw)
    -> system::result<std::chrono::system_clock::time_point>
{
  const std::size_t wd = std::get<0>(raw).index();
  const auto d1 = std::get<1>(raw);
  const auto t1 = std::get<2>(raw);

  std::chrono::seconds time_{};
  time_ += std::chrono::seconds(std::stoi(std::get<2>(t1)));
  time_ += std::chrono::minutes(std::stoi(std::get<1>(t1)));
  time_ += std::chrono::hours(std::stoi(std::get<0>(t1)));

  const auto year = std::stoi(std::get<2>(d1));
  if (year < 1970)
  {
    BOOST_URL_RETURN_EC(urls::grammar::error::out_of_range);
  }

  switch (std::get<1>(d1).index())
  {
  case 11: // december
    time_ += std::chrono::hours(30 * 24);
    BOOST_FALLTHROUGH;
  case 10: // november
    time_ += std::chrono::hours(31 * 24);
    BOOST_FALLTHROUGH;
  case 9: // october
    time_ += std::chrono::hours(30 * 24);
    BOOST_FALLTHROUGH;
  case 8: // september
    time_ += std::chrono::hours(31 * 24);
    BOOST_FALLTHROUGH;
  case 7: // august
    time_ += std::chrono::hours(31 * 24);
    BOOST_FALLTHROUGH;
  case 6: // july
    time_ += std::chrono::hours(30 * 24);
    BOOST_FALLTHROUGH;
  case 5: // june
    time_ += std::chrono::hours(31 * 24);
    BOOST_FALLTHROUGH;
  case 4: // may
    time_ += std::chrono::hours(30 * 24);
    BOOST_FALLTHROUGH;
  case 3: // april
    time_ += std::chrono::hours(31 * 24);
    BOOST_FALLTHROUGH;
  case 2: // march
    time_ += std::chrono::hours(28 * 24);
    if (year % 4 == 0)
      time_ += std::chrono::hours(24);
    BOOST_FALLTHROUGH;
  case 1: // february
    time_ += std::chrono::hours(31 * 24);
    BOOST_FALLTHROUGH;
  case 0: // january
    break;
  }

  const auto yd = year - 1970;
  time_ += std::chrono::hours((yd * 365 * 24)  + ((yd + 2) / 4) * 24 );
  time_ += std::chrono::hours((std::stoi(std::get<0>(d1)) - 1) * 24);

  const auto days = (std::chrono::duration_cast<std::chrono::hours>(time_).count() / 24u) % 7;

  if (wd != static_cast<std::size_t>(days))
  {
    BOOST_URL_RETURN_EC(urls::grammar::error::mismatch);
  }
  return std::chrono::system_clock::time_point{time_};
}


}


auto
date_1123_t::parse(
    char const*& it,
    char const* end
) const noexcept ->
    urls::error_types::result<value_type>
{
  namespace ug = boost::urls::grammar;
  using namespace detail;
  constexpr auto sp = ug::squelch(ug::literal_rule(" "));
  // RFC 2616 only allows space, but `-` is sometimes in use, that's minor enough to tolerate
  constexpr auto dsp = ug::squelch(
        ug::variant_rule(
          ug::literal_rule(" "),
          ug::literal_rule("-")))
        ;

  constexpr auto date1 =
      ug::tuple_rule(
          fixed_token_rule<2u>(ug::digit_chars),
          dsp,
          month,
          dsp,
          fixed_token_rule<4u>(ug::digit_chars)
      );

  auto res =
      ug::parse(it, end,
                ug::tuple_rule(
                    wkday,
                    ug::squelch(ug::literal_rule(", ")),
                    date1,
                    sp,
                    time,
                    ug::squelch(ug::literal_rule(" GMT"))
                        ));

  if (res.has_error())
    return res.error();
  return detail::interpret_result(*std::move(res));
}


auto
date_850_t::parse(
    char const*& it,
    char const* end
) const noexcept ->
    urls::error_types::result<value_type>
{
  namespace ug = boost::urls::grammar;
  using namespace detail;

  constexpr auto date2 =
      ug::tuple_rule(
          fixed_token_rule<2u>(ug::digit_chars),
          ug::squelch(ug::literal_rule("-")),
          month,
          ug::squelch(ug::literal_rule("-")),
          fixed_token_rule<4u>(ug::digit_chars)
      );

  auto res =
      ug::parse(it, end,
                ug::tuple_rule(
                    weekday,
                    ug::squelch(ug::literal_rule(", ")),
                    date2,
                    urls::grammar::squelch(urls::grammar::literal_rule(" ")),
                    time,
                    ug::squelch(ug::literal_rule(" GMT"))
                        ));

  if (res.has_error())
    return res.error();

  return detail::interpret_result(*std::move(res));
}


auto
date_asctime_t::parse(
    char const*& it,
    char const* end
) const noexcept ->
    urls::error_types::result<value_type>
{
  namespace ug = boost::urls::grammar;
  using namespace detail;
  constexpr auto sp = urls::grammar::squelch(urls::grammar::literal_rule(" "));

  constexpr auto date3 =
      ug::tuple_rule(
          month,
          sp,
          ug::variant_rule(
              fixed_token_rule<2u>(ug::digit_chars),
              ug::tuple_rule(sp, fixed_token_rule<1u>(ug::digit_chars))
              )
      );

  auto res =
      ug::parse(it, end,
                ug::tuple_rule(
                    wkday,
                    sp,
                    date3,
                    sp,
                    time,
                    sp,
                    fixed_token_rule<4u>(ug::digit_chars)));

  if (res.has_error())
    return res.error();

  const auto year = std::get<3>(*res);
  const auto wd = std::get<0>(*res);
  const auto dd = std::get<1>(*res);
  const auto tt = std::get<2>(*res);

  return detail::interpret_result(
      std::make_tuple(
          wd,
          std::make_tuple(
              visit([](auto val){return val;}, std::get<1>(dd)),
              std::get<0>(dd),
              year),
          tt
          ));


}

auto
http_date_t::parse(
    char const*& it,
    char const* end
) const noexcept ->
    urls::error_types::result<value_type>
{
  namespace ug = boost::urls::grammar;
  auto res = ug::parse(it, end, ug::variant_rule(date_1123, date_850, date_asctime ));

  if (res.has_error())
    return res.error();

  return visit([](value_type && v){return v;}, *std::move(res));

}


}
}
}
#endif //BOOST_REQUESTS_RFC_IMPL_RFC_1123_IPP
