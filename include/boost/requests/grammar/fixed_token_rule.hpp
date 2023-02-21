// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_COOKIES_GRAMMAR_FIXED_TOKEN_RULE_HPP
#define BOOST_REQUESTS_COOKIES_GRAMMAR_FIXED_TOKEN_RULE_HPP

#include <boost/url/grammar.hpp>
#include <boost/requests/detail/config.hpp>

namespace boost {
namespace requests {
namespace grammar {

/** Match a fixed-sized string of characters from a set

    If there is not enough input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    result< string_view > rv = parse( "abcdef", fixed_token_rule<6>( alpha_chars ) );
    @endcode

    @par BNF
    @code
    token     = 1*( ch )
    @endcode

    @param cs The character set to use

    @see
        @ref alpha_chars,
        @ref parse.
*/
#ifdef BOOST_REQUESTS_DOCS
template<std::size_t Size, class CharSet>
constexpr __implementation_defined__ fixe_rule( CharSet cs) noexcept;

template<std::size_t MinSize, std::size_t MaxSize,, class CharSet>
constexpr __implementation_defined__ fixe_rule( CharSet cs) noexcept;

#else
template<std::size_t Size, class CharSet>
struct fixed_token_rule_t
{
    using value_type = core::string_view;

    static_assert(
            urls::grammar::is_charset<CharSet>::value,
            "CharSet requirements not met");

    auto
    parse(
            char const*& it,
            char const* end
    ) const noexcept ->
    system::result<value_type>
    {
        const auto it0 = it;
        if(static_cast<std::size_t>(std::distance(it, end)) < Size)
            BOOST_REQUESTS_RETURN_EC(
                    urls::grammar::error::need_more);

        const auto e = std::next(it, Size);
        it = (urls::grammar::find_if_not)(it, e, cs_);
        if(it == e)
            return core::string_view(it0, it - it0);
        BOOST_REQUESTS_RETURN_EC(
                urls::grammar::error::mismatch);
    }

  private:
    template<std::size_t Size_, class CharSet_>
    friend
    constexpr
    auto
    fixed_token_rule(
            CharSet_ const&) noexcept ->
    fixed_token_rule_t<Size_, CharSet_>;

    constexpr
    fixed_token_rule_t(
            CharSet const& cs) noexcept
            : cs_(cs)
    {
    }

    CharSet const cs_;
};

template<std::size_t MinSize, std::size_t MaxSize, class CharSet>
struct fixed_token_rule_2_t
{

  using value_type = core::string_view;

  static_assert(
      urls::grammar::is_charset<CharSet>::value,
      "CharSet requirements not met");

  auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      system::result<value_type>
  {
    const auto it0 = it;
    if (std::distance(it, end) < static_cast<int>(MinSize))
      BOOST_REQUESTS_RETURN_EC(
          urls::grammar::error::need_more);

    const auto emin = std::next(it, MinSize);
    const auto e = (std::min)(std::next(it, MaxSize), end);

    it = (urls::grammar::find_if_not)(it, e, cs_);
    if(it >= emin)
      return core::string_view(it0, it - it0);
    BOOST_REQUESTS_RETURN_EC(
        urls::grammar::error::mismatch);
  }

private:
  template<std::size_t MinSize_, std::size_t MaxSize_, class CharSet_>
  friend
      constexpr
      auto
      fixed_token_rule(
          CharSet_ const&) noexcept ->
      fixed_token_rule_2_t<MinSize_, MaxSize_, CharSet_>;

  constexpr fixed_token_rule_2_t(CharSet const& cs) noexcept
      : cs_(cs)
  {
  }

  CharSet const cs_;
};

template<std::size_t Size, class CharSet>
constexpr
auto
fixed_token_rule(
        CharSet const& cs) noexcept ->
fixed_token_rule_t<Size, CharSet>
{
    return {cs};
}


template<std::size_t MinSize, std::size_t MaxSize, class CharSet>
constexpr
    auto
    fixed_token_rule(
        CharSet const& cs) noexcept ->
    fixed_token_rule_2_t<MinSize, MaxSize, CharSet>
{
  return {cs};
}
#endif

} // grammar
} // requests
} // boost


#endif //BOOST_REQUESTS_COOKIES_GRAMMAR_FIXED_TOKEN_RULE_HPP
