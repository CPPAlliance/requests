// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SRC_IPP
#define BOOST_REQUESTS_SRC_IPP

#include <boost/requests/detail/config.hpp>

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#error "You can't include this in header only mode"
#endif

#ifndef BOOST_REQUESTS_SOURCE
#define BOOST_REQUESTS_SOURCE
#endif

#include <boost/requests/detail/impl/connection_impl.ipp>
#include <boost/requests/detail/impl/mutex.ipp>
#include <boost/requests/fields/impl/keep_alive.ipp>
#include <boost/requests/fields/impl/link.ipp>
#include <boost/requests/fields/impl/location.ipp>
#include <boost/requests/fields/impl/set_cookie.ipp>
#include <boost/requests/impl/connection_pool.ipp>
#include <boost/requests/impl/cookie_jar.ipp>
#include <boost/requests/impl/error.ipp>
#include <boost/requests/impl/mime_types.ipp>
#include <boost/requests/impl/public_suffix.ipp>
#include <boost/requests/impl/redirect.ipp>
#include <boost/requests/impl/request_options.ipp>
#include <boost/requests/impl/request_settings.ipp>
#include <boost/requests/impl/response.ipp>
#include <boost/requests/impl/session.ipp>
#include <boost/requests/impl/stream.ipp>
#include <boost/requests/rfc/impl/dates.ipp>
#include <boost/requests/rfc/impl/link.ipp>
#include <boost/requests/rfc/impl/quoted_string.ipp>
#include <boost/requests/sources/impl/buffer.ipp>
#include <boost/requests/sources/impl/empty.ipp>
#include <boost/requests/sources/impl/file.ipp>
#include <boost/requests/sources/impl/form.ipp>
#include <boost/requests/sources/impl/json.ipp>

#endif //BOOST_REQUESTS_SRC_IPP
