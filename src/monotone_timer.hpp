//
// monotone_timer.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2009
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_MONOTONE_TIMER_HPP
#define BOOST_ASIO_MONOTONE_TIMER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)
#include <boost/asio/detail/push_options.hpp>

#include <boost/asio/detail/socket_types.hpp> // Must come before posix_time.
#include <boost/asio/detail/push_options.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/asio/detail/pop_options.hpp>

#include <boost/asio/basic_deadline_timer.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>


namespace boost
{
namespace asio
{

namespace monotone_time
{

class mtime
{
public:
	mtime()	{}
	explicit mtime(const boost::posix_time::time_duration &d) :	m(d){}
	boost::posix_time::time_duration m;
	mtime operator+(const boost::posix_time::time_duration &d) const { return mtime(m + d);	}
	mtime operator-(const boost::posix_time::time_duration &d) const { return mtime(m - d);	}
	boost::posix_time::time_duration operator-(const mtime &t) const { return m - t.m; }
};

} // namespace monotone_time


/// Time traits specialized for monotone_time
template <>
struct time_traits<monotone_time::mtime>
{
	/// The time type.
	typedef boost::asio::monotone_time::mtime time_type;

	/// The duration type.
	typedef boost::posix_time::time_duration duration_type;

	/// Get the current time.
	static time_type now()
	{
#if defined(_POSIX_TIMERS) && ( _POSIX_TIMERS > 0 ) && defined(_POSIX_MONOTONIC_CLOCK)
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
#if defined(BOOST_DATE_TIME_HAS_NANOSECONDS)
		return time_type(boost::posix_time::seconds(ts.tv_sec) +boost::posix_time::nanosec(ts.tv_nsec));
#else
		return time_type(boost::posix_time::seconds(ts.tv_sec) +boost::posix_time::microsec(ts.tv_nsec / 1000));
#endif
#elif defined(BOOST_WINDOWS)
		LARGE_INTEGER count;
		LARGE_INTEGER frequency;

		QueryPerformanceCounter(&count);
		QueryPerformanceFrequency(&frequency);
	
		ULONGLONG nanoseconds = count.QuadPart*(1000000000/frequency.QuadPart);
#if defined(BOOST_DATE_TIME_HAS_NANOSECONDS)
		return time_type(boost::posix_time::nanosec(nanoseconds));
#else
		return time_type(boost::posix_time::microsec(nanoseconds/1000));
#endif
#endif
	}

	/// Add a duration to a time.
	static time_type add(const time_type& t, const duration_type& d)
	{
		return t + d;
	}

	/// Subtract one time from another.
	static duration_type subtract(const time_type& t1, const time_type& t2)
	{
		return t1.m - t2.m;
	}

	/// Test whether one time is less than another.
	static bool less_than(const time_type& t1, const time_type& t2)
	{
		return t1.m < t2.m;
	}

	/// Convert to POSIX duration type.
	static boost::posix_time::time_duration to_posix_duration(
		const duration_type& d)
	{
		return d;
	}
};

/// Typedef for the typical usage of timer.
typedef boost::asio::basic_deadline_timer<boost::asio::monotone_time::mtime> monotone_timer;

} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>



#endif // BOOST_ASIO_MONOTONE_TIMER_HPP
