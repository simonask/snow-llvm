#include "snow/snow.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/intern.h"
#include "snow/function.h"
#include "snow/parser.h"
#include "snow/linkbuffer.h"
#include "snow/str.h"
#include "snow/array.h"
#include "snow/exception.h"

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


static void interactive_prompt(SN_P p)
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
			SnStringRef str = snow_create_string_from_linkbuffer(p, input_buffer);
			VALUE result = snow_eval(p, snow_string_cstr(str));
			printf("Got a result: %p\n", result);
			//snow_printf(p, "=> {0}\n", 1, snow_call_method(p, result, SN_SYM("inspect"), 0));
			snow_linkbuffer_clear(input_buffer);
		}
	}
}

int snow_main(int argc, char* const* argv)
{
	printf("main()\n");
	SN_P p = snow_get_main_process();
	
	static int debug_mode = false;
	static int verbose_mode = false;
	static int interactive_mode = false;
	
	SnArrayRef require_files = snow_create_array(p);
	
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
				SnStringRef filename = snow_create_string(p, optarg);
				snow_array_push(p, require_files, snow_string_as_object(filename));
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
		SnStringRef filename = snow_create_string(p, argv[optind++]);
		snow_array_push(p, require_files, snow_string_as_object(filename));
	}
	
	// stuff the rest in ARGV
	SnArrayRef ARGV = snow_create_array(p);
	while (optind < argc) {
		SnStringRef argument = snow_create_string(p, argv[optind++]);
		snow_array_push(p, ARGV, snow_string_as_object(argument));
	}
	snow_set_global(p, SN_SYM("ARGV"), snow_array_as_object(ARGV));
	
	for (size_t i = 0; i < snow_array_size(require_files); ++i) {
		snow_require(p, snow_string_cstr(snow_object_as_string(snow_array_get(require_files, i))));
	}
	
	if (interactive_mode) {
		interactive_prompt(p);
	}
	
	return 0;
}

int main(int argc, char** argv) {
	return snow_main(argc, argv);
}
