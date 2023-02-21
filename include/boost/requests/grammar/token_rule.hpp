//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_GRAMMAR_TOKEN_RULE_HPP
#define BOOST_REQUESTS_GRAMMAR_TOKEN_RULE_HPP

#include <boost/requests/detail/config.hpp>
#include <boost/url/grammar/token_rule.hpp>

namespace boost {
namespace requests {
namespace grammar {

using boost::urls::grammar::token_rule_t;
using boost::urls::grammar::token_rule;


/** Match a non-empty string of characters from a set,
    where the first character is of a differnt charset then the rest.

    If there is no more input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    result< string_view > rv = parse( "abc123", token_rule( alpha_chars, alnum_chars ) );
    @endcode

    @par BNF
    @code
    token     =  ch1 *( ch2 )
    @endcode

    @param cs The character set to use

    @see
        @ref alpha_chars,
        @ref parse.
*/
#ifdef BOOST_REQUESTS_DOCS
template<class CharSetFirst, class CharSetRest>
constexpr __implementation_defined__ token_rule(CharSet csf, CharSetRest csr) noexcept;
#else
template<class CharSetFirst, class CharSetRest>
struct token_rule_2_t
{
  using value_type = core::string_view;

  static_assert(
      urls::grammar::is_charset<CharSetFirst>::value &&
      urls::grammar::is_charset<CharSetRest>::value,
      "CharSet requirements not met");

  auto
  parse(
      char const*& it,
      char const* end
  ) const noexcept ->
      system::result<value_type>
  {
    auto const it0 = it;
    if(it == end)
      BOOST_REQUESTS_RETURN_EC(
          urls::grammar::error::need_more);

    if (!csf_(*it))
      BOOST_REQUESTS_RETURN_EC(
          urls::grammar::error::mismatch);

    it = (urls::grammar::find_if_not)(++it, end, csr_);
    return urls::string_view(it0, it - it0);
  }

private:
  template<class CharSetFirst_, class CharSetRest_>
  friend
    constexpr
    auto
    token_rule(
        CharSetFirst_ const&,
        CharSetRest_ const &) noexcept ->
      token_rule_2_t<CharSetFirst_, CharSetRest_>;

  constexpr
      token_rule_2_t(
          CharSetFirst const & csf,
          CharSetRest  const & csr) noexcept
      : csf_(csf), csr_(csr)
  {
  }

  CharSetFirst const csf_;
  CharSetRest const csr_;
};

template<class CharSetFirst, class CharSetNext>
constexpr
    auto
    token_rule(
        CharSetFirst const& csf,
        CharSetNext  const& csr) noexcept ->
    token_rule_2_t<CharSetFirst, CharSetNext>
{
  return {csf, csr};
}
#endif

}
}
}

#endif // BOOST_REQUESTS_GRAMMAR_TOKEN_RULE_HPP
