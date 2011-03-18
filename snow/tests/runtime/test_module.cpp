#include "test.hpp"
#include "color.hpp"
#include <stdio.h>

#include <stdexcept>
#include <iostream>

extern "C" int snow_test_suite_entry()
{
	using namespace snow::test;
	
	try {
		run_tests();
	}
	catch (std::runtime_error error) {
		std::cerr << color::red << "UNCAUGHT EXCEPTION:" << color::reset << '\n';
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}

namespace snow { namespace test {
	void begin_group(const char* description) {
		std::cout << color::cyan << description << color::reset << '\n';
	}
	
	Story begin_story(const char* c_description) {
		std::string description(c_description);
		static const int total_width = 80;
		std::cout << description << ' ';
		for (int i = description.size(); i < total_width-9; ++i) {
			std::cout << ' ';
		}
		return Story();
	}
	
	void fail_story(Story& story, const TestFailure& failure) {
		story.failed = true;
		std::cout << color::red << "[FAILED]" << color::reset << '\n';
		std::cerr << "Test failed at " << failure.file << ":" << failure.line << ": " << failure.expression << '\n';
	}
	
	void finish_story(Story& story) {
		if (!story.failed) {
			std::cout << color::green << "[  OK  ]" << color::reset << '\n';
		}
	}
}}