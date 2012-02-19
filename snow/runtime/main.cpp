#include "snow/snow.hpp"
#include "internal.h"

#include "snow/array.hpp"
#include "snow/exception.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "linkbuffer.hpp"
#include "snow/module.hpp"
#include "snow/object.hpp"
#include "snow/parser.hpp"
#include "snow/str.hpp"
#include "snow/numeric.hpp"
#include "snow/str-format.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <getopt.h>

using namespace snow;

#define ARCH_NAME "LLVM"

static void print_version_info()
{
	printf("Snow %s (prealpha super unstable) [" ARCH_NAME "]\n", "0.0.1a");
}


static void interactive_prompt()
{
	const char* global_prompt = "snow> ";
	const char* unfinished_prompt = "snow*> ";
	bool unfinished_expr = false;
	
	snow::LinkBuffer<char> input_buffer;

	char* line;
	while ((line = readline(unfinished_expr ? unfinished_prompt : global_prompt)) != NULL) {
		if (*line) // strlen(line) != 0
			add_history(line);
		input_buffer.push_range(line, line + strlen(line));
		free(line);

		//unfinished_expr = is_expr_unfinished(buffer.str());
		if (!unfinished_expr) {
			try {
				ObjectPtr<String> str = create_string_from_linkbuffer(input_buffer);
				Value result = eval_in_global_module(str);
				ObjectPtr<String> inspected = value_inspect(result);
				if (inspected == NULL) {
					inspected = format_string("[Object@%@]", format::pointer(result));
				}
				
				size_t sz = string_size(inspected);
				char buffer[sz+1];
				string_copy_to(inspected, buffer, sz);
				buffer[sz] = '\0';
				printf("=> %s\n", buffer);
			}
			catch (snow::ExceptionPtr ex) {
				StringPtr backtrace = exception_get_internal_backtrace(ex);
				StringPtr source_excerpt = exception_get_source_excerpt(ex);
				StringPtr error_message = format_string("ERROR: Unhandled exception: %@\n\nFile: %@ (line %@, column %@)\n\nSource excerpt:\n%@\n\nBacktrace:\n%@\n", ex, exception_get_file(ex), exception_get_line(ex), exception_get_column(ex), source_excerpt, backtrace);
				string_puts(error_message);
			}
			catch (...) {
				fprintf(stderr, "ERROR: Unhandled C++ exception, rethrowing!\n");
				throw;
			}
			input_buffer.clear();
		}
	}
}

namespace snow {
int main(int argc, char* const* argv) {
	static int debug_mode = false;
	static int verbose_mode = false;
	static int interactive_mode = false;
	
	ObjectPtr<Array> require_files = create_array();
	
	while (true)
	{
		int c;
		
		static struct option long_options[] = {
			{"debug",       no_argument,       &debug_mode,       'd'},
			{"version",     no_argument,       NULL,              'v'},
			{"require",     required_argument, NULL,              'r'},
			{"interactive", no_argument,       &interactive_mode,  1 },
			{"verbose",     no_argument,       &verbose_mode,      1 },
			{0,0,0,0}
		};
		
		int option_index = -1;
		
		c = getopt_long(argc, argv, "dvir:", long_options, &option_index);
		if (c < 0)
			break;
		
		switch (c)
		{
			case 'v':
			{
				print_version_info();
				return 0;
			}
			case 'r':
			{
				ObjectPtr<String> filename = create_string_constant(optarg);
				array_push(require_files, filename);
				break;
			}
			case 'i':
			{
				interactive_mode = true;
				break;
			}
			case '?':
				TRAP(); // unknown argument
			default:
				break;
		}
	}
	
	// require first loose argument, unless -- was used
	if (optind < argc && strcmp("--", argv[optind-1]) != 0) {
		ObjectPtr<String> filename = create_string_constant(argv[optind++]);
		array_push(require_files, filename);
	}
	
	// stuff the rest in ARGV
	ObjectPtr<Array> ARGV = create_array_with_size(argc);
	while (optind < argc) {
		ObjectPtr<String> argument = create_string_constant(argv[optind++]);
		array_push(ARGV, argument);
	}
	set_global(snow::sym("ARGV"), ARGV);
	
	for (size_t i = 0; i < array_size(require_files); ++i) {
		ObjectPtr<String> str = array_get(require_files, i);
		ASSERT(str != NULL);
		load(str);
	}
	
	if (interactive_mode) {
		interactive_prompt();
	}
	
	return 0;
}
}