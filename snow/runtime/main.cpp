#include "snow/snow.h"
#include "internal.h"

#include "snow/array.h"
#include "snow/exception.hpp"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/module.h"
#include "snow/object.h"
#include "snow/parser.h"
#include "snow/str.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <getopt.h>

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
	
	SnLinkBuffer* input_buffer = snow_create_linkbuffer(1024);

	char* line;
	while ((line = readline(unfinished_expr ? unfinished_prompt : global_prompt)) != NULL) {
		if (*line) // strlen(line) != 0
			add_history(line);
		snow_linkbuffer_push_string(input_buffer, line);
		free(line);

		//unfinished_expr = is_expr_unfinished(buffer.str());
		if (!unfinished_expr) {
			try {
				SnString* str = snow_create_string_from_linkbuffer(input_buffer);
				VALUE result = snow_eval_in_global_module(snow_string_cstr(str));
				VALUE inspected = SNOW_CALL_METHOD(result, "inspect", 0, NULL);
				if (snow_type_of(inspected) != SnStringType) {
					inspected = snow_string_format("[Object@%p]", result);
				}
				printf("=> %s\n", snow_string_cstr((SnString*)inspected));
			}
			catch (const snow::Exception& ex) {
				fprintf(stderr, "ERROR: Unhandled exception: %s\n", snow_value_to_cstr(ex.value));
			}
			catch (...) {
				fprintf(stderr, "ERROR: Unhandled C++ exception, rethrowing!\n");
				snow_linkbuffer_clear(input_buffer);
				throw;
			}
			snow_linkbuffer_clear(input_buffer);
		}
	}
}

CAPI int snow_main(int argc, char* const* argv) {
	static int debug_mode = false;
	static int verbose_mode = false;
	static int interactive_mode = false;
	
	SnArray* require_files = snow_create_array();
	
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
				SnString* filename = snow_create_string_constant(optarg);
				snow_array_push(require_files, filename);
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
		SnString* filename = snow_create_string_constant(argv[optind++]);
		snow_array_push(require_files, filename);
	}
	
	// stuff the rest in ARGV
	SnArray* ARGV = snow_create_array_with_size(argc);
	while (optind < argc) {
		SnString* argument = snow_create_string_constant(argv[optind++]);
		snow_array_push(ARGV, argument);
	}
	snow_set_global(snow_sym("ARGV"), ARGV);
	
	for (size_t i = 0; i < snow_array_size(require_files); ++i) {
		VALUE vstr = snow_array_get(require_files, i);
		ASSERT(snow_type_of(vstr) == SnStringType);
		snow_load(snow_string_cstr((SnString*)vstr));
	}
	
	if (interactive_mode) {
		interactive_prompt();
	}
	
	return 0;
}
