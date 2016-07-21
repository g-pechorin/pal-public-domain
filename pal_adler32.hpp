///	Peter LaValle / gmail
///	pal_adler32 and util .hpp
///
/// This file provides adler32 functionality compatible with Visual Studio 2013 CE.
/// It makes a drop-in replacement for string-keys based on Jason Gregory's notes in "Game Engine Architecture" with the added benefit of a TMP solver.
///	... which I'm itchin to try with a code generator.
///
/// I place it in "public domain" ... if your lawyers say no to that; get better lawyers
///	... though if you can handle it; feedback and attribution would make my tail wag (but I by no means require such silliness)
/// 
/// 
///	Contents;
///		> a function `fun(...)` prototype to calculate an adler32 sum from a null-terminated string
///		> an object `obj` that wraps this into an imutable compare-to-anything class
///		> a TMP construct that can caulculate the value at compile-time and is suitable for switch statements
///		> the definition of `fun(...)` with some template acrobatics to perform sanity checks
///
/// 2016-04-02
///		- "cleanup" of docs and code
//		- added a debug-mode sanity check to ensure strings are unique
///		- elborated license / documentation / blurb-at-top
///
///	======================================================================================================================
#pragma once

#include <string>

#include <stdint.h>

namespace pal_adler32
{
	// https://en.wikipedia.org/wiki/Adler-32#Example
	uint32_t fun(const char* text);

	///
	/// Object for sum/string comparisons
	struct obj
	{
		/// this is the hash-sum we've got from the string
		/// it's const to prevent clever-snots from mucking with it (... and of course the copy operator casts off the const modifier)
		const uint32_t _sum;

#define pal_adler32_obj_new(ARG, SUM)	obj(ARG) : _sum(SUM) { }; obj& operator=(ARG) { *(const_cast<uint32_t*>(&_sum)) = SUM; return *this; };

		pal_adler32_obj_new(const char* text, fun(text));
		pal_adler32_obj_new(const std::string& text, fun(text.c_str()));
		pal_adler32_obj_new(const uint32_t sum, sum);
		pal_adler32_obj_new(const obj& other, other._sum);


#define pal_adler32_obj_cmp(ARG, OP, VAL)	bool operator OP (ARG)const { return _sum OP VAL; }
#define pal_adler32_obj_cmpall(ARG, VAL)	\
			pal_adler32_obj_cmp(ARG, !=, VAL) \
			pal_adler32_obj_cmp(ARG, ==, VAL) \
			pal_adler32_obj_cmp(ARG, <=, VAL) \
			pal_adler32_obj_cmp(ARG, >=, VAL) \
			pal_adler32_obj_cmp(ARG, <, VAL) \
			pal_adler32_obj_cmp(ARG, >, VAL)

		pal_adler32_obj_cmpall(const char* text, fun(text));
		pal_adler32_obj_cmpall(const std::string& text, fun(text.c_str()));
		pal_adler32_obj_cmpall(const uint32_t hash, hash);
		pal_adler32_obj_cmpall(const pal_adler32::obj& other, other._sum);

		//operator const uint32_t(void) const { return _sum; }
	};


	/// I'm using the funky `template<...> struct ...` form below because these aren't type-teplates

	// with help from ; http://eli.thegreenplace.net/2014/variadic-templates-in-c/
	// pal_adler32::fun("Wikipedia")) == pal_adler32::charlist<'W', 'i', 'k', 'i', 'p', 'e', 'd', 'i', 'a'>::sum<>::val
	// should let code-generators use almost-strings in switch statements
	// ... not sure what'll happen on big-endian CPUs (best you put an assertion in place)
	template <char...> struct charlist
	{
		template <uint32_t a, uint32_t b> struct sum
		{
			enum _tmp : uint32_t
			{
				val = (b << 16) | a,
			};
		};

		enum _tmp : size_t { len = 0, };

		static void cpy(char* out) { *out = '\0'; }
		static const char* str(void) { return ""; }
	};

	template <char head, char... tail> struct charlist < head, tail... >
	{
		template <uint32_t a = 1, uint32_t b = 0> struct sum
		{
			enum _tmp : uint32_t
			{
				val = charlist<tail...>::sum<(a + head) % 65521, (b + ((a + head) % 65521)) % 65521>::val,
			};
		};

		enum _tmp : size_t { len = 1 + charlist<tail...>::len, };

		// copies the string to a normal C-string
		static void cpy(char* out) { *out = head; charlist<tail...>::cpy(out + 1); }

		// returns a static-local version of the C-string
		static const char* str(void)
		{
			static char mem[len];
			cpy(mem);
			return mem;
		}
	};
};

#include <ostream>

inline std::ostream& operator << (std::ostream& stream, const pal_adler32::obj& val)
{
	return stream << std::hex << val._sum << std::dec;
}

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

TEST(pal_adler32, Wikipedia_fun)
{
	EXPECT_EQ(0x11E60398, pal_adler32::fun("Wikipedia"));
}

TEST(pal_adler32, Wikipedia_charlist)
{
	auto crc = pal_adler32::charlist<'W', 'i', 'k', 'i', 'p', 'e', 'd', 'i', 'a'>::sum<>::val;
	EXPECT_EQ(0x11E60398, crc);
}

TEST(pal_adler32, Wikipedia_string)
{
	std::string crc = pal_adler32::charlist<'W', 'i', 'k', 'i', 'p', 'e', 'd', 'i', 'a'>::str();
	EXPECT_EQ(std::string("Wikipedia"), crc);
}

#endif

#include <assert.h>

#include <array>

template<typename E, typename I, I min, I max>
class subarray
{
	std::array<E, 1 + max - min> _items;
public:
	struct {

		struct _it
		{
			I _i;

			bool operator !=(const _it& other) const { return _i != other._i; }

			_it& operator ++(void) { ++_i; return *this; }

			I operator*(void) const { return _i; }
		};

		_it begin(void) { return _it{ min }; }

		_it end(void) { return _it{ max + 1 }; }

	} indicies;

	subarray(E value)
	{
		for (I i = 0; i < (1 + max - min); ++i)
		{
			_items[i] = value;
		}
	}

	static bool valid(const I index)
	{
		return min <= index && index <= max;
	}

	E& operator[](const I index)
	{
		if (!valid(index))
			assert(valid(index));

		return _items[index - min];
	}
};

#include <stdint.h>
#include <vector>
#include <stdio.h>

inline std::vector<uint8_t>& operator >>(FILE* file, std::vector<uint8_t>& buffer)
{
	const auto pos = ftell(file);
	fseek(file, 0, SEEK_END);
	const auto end = ftell(file);
	fseek(file, pos, SEEK_SET);

	const auto base = buffer.size();
	buffer.resize(base + (end - pos));

	for (size_t i = base; i < buffer.size(); assert(!feof(file)))
	{
		i += fread(&(buffer[i]), 1, buffer.size() - i, file);
	}

	fclose(file);

	return buffer;
}

namespace pal
{
	template<size_t L>
	struct fixed_string
	{
		std::array<char, L> _data;
		operator const char*(void) const { return &(_data[0]); }
		operator char*(void) { return &(_data[0]); }
	};

	fixed_string<sizeof(void*) * 2 + 1> ptr2hex(void*);

	static_assert('0' < '9', "I'll need to rethink some conversions");

	const uint64_t hex_to_u64(const char*);

	fixed_string<sizeof(uint64_t) * 2 + 1> u64_to_hex(const uint64_t);
}




///
/// This pile of preprocessor acrobatics implements a sanity check at runtime
/// it ensures that all strings which're used are unique

#if defined(pal_adler32_cpp)
#if defined(_DEBUG)
static uint32_t pal_adler32_fun_(const char* text);

#include <assert.h>

#include <map>
#include <sstream>

uint32_t pal_adler32::fun(const char* text)
{
	// calculate the sum
	auto value = pal_adler32_fun_(text);

#if !defined(pal_adler32_cpp_nosanity_check)
	static std::map<uint32_t, std::string> known;

	if (known.end() == known.find(value))
	{
		// it's a `new` value - insert it!
		known[value] = std::string(text);
	}
	else
	{
		// it's an old value - check it!
		assert(std::string(text) == known[value]);
	}
#endif

	return value;
}

static uint32_t pal_adler32_fun_(const char* text)
#else
uint32_t pal_adler32::fun(const char* text)
#endif
{
	uint32_t a = 1, b = 0;

	for (size_t index = 0; text[index]; ++index)
	{
		a = (a + text[index]) % 65521;
		b = (b + a) % 65521;
	}

	return (b << 16) | a;
}

pal::fixed_string<sizeof(void*) * 2 + 1> ptr2hex(void* ptr)
{
	pal::fixed_string<1 + sizeof(void*) * 2> text;
	static_assert(sizeof(void*) == sizeof(size_t), "Expectations");
	text[sizeof(void*) * 2] = '\0';

	for (int i = 0; i < sizeof(void*) * 2; i)
	{
		auto v_ = (reinterpret_cast<size_t>(ptr) >> (i * 4)) & 0x0000000F;

		assert(0 <= v_ && v_ <= 0xF);

		auto v = (char)v_;

		char c = (v <= 9) ? ('0' + v) : ('a' + (v - 10));

		text[(int)((sizeof(void*) * 2) - (++i))] = c;
	}

#if _DEBUG
	std::ostringstream slow;

	slow << std::hex << reinterpret_cast<size_t>(ptr);
	char* out = &(text[0]);
	while ('0' == *out && out[1])
	{
		++out;
	}

	auto str = slow.str();

	PAL_STUB_ASSERT_EQ(str, std::string(out), "Uh ohhh");
#endif

	return text;
}

const uint64_t pal::hex_to_u64(const char* val)
{
	uint64_t value = 0;

	for (int i = 0; val[i]; ++i)
	{
		value <<= 4;
		auto c = ('0' <= val[i] && val[i] <= '9') ? val[i] - '0' : ((val[i] - 'a') + 10);

		value += c;
	}

	return value;
}

pal::fixed_string<sizeof(uint64_t) * 2 + 1> pal::u64_to_hex(const uint64_t val)
{
	pal::fixed_string<1 + sizeof(uint64_t) * 2> text;
	text[sizeof(uint64_t) * 2] = '\0';

	for (int i = 0; i < sizeof(uint64_t) * 2;)
	{
		auto v = static_cast<char>((val >> (i * 4)) & 0x0000000F);

		char c = (v <= 9) ? ('0' + v) : ('a' + (v - 10));

		text[(int)((sizeof(uint64_t) * 2) - (++i))] = c;
	}

#if _DEBUG
	std::ostringstream slow;

	slow << std::hex << val;
	char* out = &(text[0]);
	while ('0' == *out && out[1])
	{
		++out;
	}

	auto str = slow.str();

	PAL_STUB_ASSERT_EQ(str, std::string(out), "Uh ohhh");

	PAL_STUB_ASSERT_EQ(val, hex_to_u64(out), "Failed to decode");
#endif

	return text;
}

#endif
