/*
Vertical Stats for OBS
Copyright (C) 2026 Nathan V

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

/*
 * A very small test harness: TEST_CASE registers a function, CHECK* record
 * failures without aborting, and RunAll runs every case and reports. No
 * dependencies beyond the standard library, so the tests build anywhere a
 * C++17 compiler does.
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct Case {
	const char *name;
	void (*fn)();
};

inline std::vector<Case> &Cases()
{
	static std::vector<Case> cases;
	return cases;
}

inline int &Failures()
{
	static int failures = 0;
	return failures;
}

struct Register {
	Register(const char *name, void (*fn)()) { Cases().push_back({name, fn}); }
};

template<typename T> std::string Show(const T &value)
{
	std::ostringstream out;
	out.precision(12);
	out << value;
	return out.str();
}

inline std::string Show(bool value)
{
	return value ? "true" : "false";
}

inline std::string Show(const char *value)
{
	return value ? std::string("\"") + value + "\"" : "null";
}

inline std::string Show(const std::string &value)
{
	return "\"" + value + "\"";
}

inline void Fail(const char *file, int line, const std::string &message)
{
	Failures()++;
	fprintf(stderr, "    FAIL %s:%d: %s\n", file, line, message.c_str());
}

template<typename A, typename B> void Equal(const char *file, int line, const char *text, const A &a, const B &b)
{
	if (!(a == b))
		Fail(file, line, std::string(text) + "  got " + Show(a) + ", expected " + Show(b));
}

inline void Str(const char *file, int line, const char *text, const std::string &a, const std::string &b)
{
	if (a != b)
		Fail(file, line, std::string(text) + "  got " + Show(a) + ", expected " + Show(b));
}

inline void Near(const char *file, int line, const char *text, long double a, long double b, long double eps)
{
	if (std::fabs(a - b) > eps)
		Fail(file, line,
		     std::string(text) + "  got " + Show(a) + ", expected " + Show(b) + " within " + Show(eps));
}

/* Runs every registered case, or only those whose name contains filter.
 * Returns a process exit code. */
inline int RunAll(const char *filter = nullptr)
{
	int ran = 0;
	int failedCases = 0;
	for (const Case &c : Cases()) {
		if (filter && !strstr(c.name, filter))
			continue;
		int before = Failures();
		c.fn();
		ran++;
		if (Failures() != before) {
			failedCases++;
			fprintf(stderr, "FAILED %s\n", c.name);
		} else {
			printf("ok     %s\n", c.name);
		}
	}
	printf("%d case(s), %d failed, %d check(s) failed\n", ran, failedCases, Failures());
	return (ran == 0 || Failures()) ? 1 : 0;
}

} // namespace check

#define TEST_CASE(name)                                       \
	static void name();                                   \
	static const check::Register name##_registration(#name, name); \
	static void name()

/* An expression rather than do/while so MSVC /W4 does not raise C4127
 * ("conditional expression is constant") on the while (0). */
#define CHECK(expr) ((expr) ? (void)0 : check::Fail(__FILE__, __LINE__, #expr))

#define CHECK_EQ(a, b) check::Equal(__FILE__, __LINE__, #a " == " #b, (a), (b))
#define CHECK_STR(a, b) check::Str(__FILE__, __LINE__, #a " == " #b, (a), (b))
#define CHECK_NEAR(a, b, eps) check::Near(__FILE__, __LINE__, #a " ~= " #b, (a), (b), (eps))
