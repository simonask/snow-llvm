#pragma once
#ifndef TEST_HPP_1IKARV06
#define TEST_HPP_1IKARV06

#include "../libsnow/basic.hpp"
#include <list>
#include <sstream>
#include <string>

namespace snow { namespace test {
	struct TestFailure {
		const char* expression;
		const char* file;
		int line;
		TestFailure(const char* expression, const char* file, int line) : expression(expression), file(file), line(line) {}

		template <typename A, typename B>
		inline static TestFailure Op(const char* op, const char* expr_a, const A& a, const char* expr_b, const B& b, const char* file, int line) {
			std::stringstream ss;
			ss << expr_a << " " << op << " " << expr_b << " ";
			ss << "(" << a << " " << op << " " << b << ")";
			std::string* str = new std::string(ss.str()); // leak on purpose
			return TestFailure(str->c_str(), file, line);
		}
	};
	
	struct Story {
		bool failed;
		Story() : failed(false) {}
	};
	
	void run_tests();
	void begin_group(const char* name);
	Story begin_story(const char* description);
	void fail_story(Story& story, const TestFailure& failure);
	void finish_story(Story& story);
	
	#define BEGIN_TESTS() namespace snow { namespace test { void run_tests() {
	#define END_TESTS() } } }
	#define BEGIN_GROUP(NAME) begin_group(NAME); {
	#define END_GROUP() }
	#define STORY_VAR _story_ ##__LINE__
	#define STORY(DESCRIPTION, BLOCK) Story STORY_VAR = begin_story(DESCRIPTION); try BLOCK catch(TestFailure failure) { fail_story(STORY_VAR, failure); } finish_story(STORY_VAR)
	#define TEST(EXPR) if (!(EXPR)) { throw TestFailure(#EXPR, __file__, __LINE); }
	#define TEST_EQ(A, B) if (!((A) == (B))) { throw TestFailure::Op("==", #A, A, #B, B, __FILE__, __LINE__); }
	#define TEST_NEQ(A, B) if ((A) == (B)) { throw TestFailure::Op("!=", #A, A, #B, B, __FILE__, __LINE__); }
}}

#endif /* end of include guard: TEST_HPP_1IKARV06 */
