//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_COOKIE_JAR_IPP
#define BOOST_REQUESTS_IMPL_COOKIE_JAR_IPP

#if defined(BOOST_REQUESTS_SOURCE)

#include <boost/requests/cookie_jar.hpp>

namespace boost {
namespace requests {

template struct basic_cookie_jar<std::allocator<char>>;
template struct basic_cookie_jar<container::pmr::polymorphic_allocator<char>>;

}
}

#endif


#endif // BOOST_REQUESTS_COOKIE_JAR_IPP