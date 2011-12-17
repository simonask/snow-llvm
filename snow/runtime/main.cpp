#include "snow/snow.hpp"
#include "internal.h"

#include "snow/array.hpp"
#include "snow/exception.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "snow/linkbuffer.h"
#include "snow/module.hpp"
#include "snow/object.hpp"
#include "snow/parser.h"
#include "snow/str.hpp"

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
				SnObject* str = snow_create_string_from_linkbuffer(input_buffer);
				VALUE result = snow_eval_in_global_module(str);
				VALUE inspected = SNOW_CALL_METHOD(result, "inspect", 0, NULL);
				if (!snow_is_string(inspected)) {
					inspected = snow_string_format("[Object@%p]", result);
				}
				
				size_t sz = snow_string_size((SnObject*)inspected);
				char buffer[sz+1];
				snow_string_copy_to((SnObject*)inspected, buffer, sz);
				buffer[sz] = '\0';
				printf("=> %s\n", buffer);
			}
			catch (const snow::Exception& ex) {
				fprintf(stderr, "ERROR: Unhandled exception: %p\n", ex.value);
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
	
	SnObject* require_files = snow_create_array();
	
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
				SnObject* filename = snow_create_string_constant(optarg);
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
		SnObject* filename = snow_create_string_constant(argv[optind++]);
		snow_array_push(require_files, filename);
	}
	
	// stuff the rest in ARGV
	SnObject* ARGV = snow_create_array_with_size(argc);
	while (optind < argc) {
		SnObject* argument = snow_create_string_constant(argv[optind++]);
		snow_array_push(ARGV, argument);
	}
	snow_set_global(snow_sym("ARGV"), ARGV);
	
	for (size_t i = 0; i < snow_array_size(require_files); ++i) {
		VALUE vstr = snow_array_get(require_files, i);
		ASSERT(snow_is_string(vstr));
		snow_load((SnObject*)vstr);
	}
	
	if (interactive_mode) {
		interactive_prompt();
	}
	
	return 0;
}
