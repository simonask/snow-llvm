#include "snow/snow.hpp"
#include "snow/module.hpp"
#include "snow/array.hpp"
#include "snow/str.hpp"
#include "snow/function.hpp"
#include "snow/exception.hpp"
#include "snow/parser.hpp"

#include <google/dense_hash_map>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dlfcn.h>

#include "codemanager.hpp"
#include "function-internal.hpp"

namespace snow {
	namespace {
		struct Module {
			Value module;
			ObjectPtr<Function> entry;
			std::string path;
			std::string source;
			void* snomo;
			// AST?
			
			Module() : snomo(NULL) {}
			~Module() {
				if (snomo != NULL) {
					dlclose(snomo);
				}
			}
		};

		typedef google::dense_hash_map<std::string, Module*> ModuleMap;
		typedef std::vector<Module*> ModuleList;

		enum ModuleType {
			ModuleTypeSource,
			ModuleTypeDynamicLibrary
		};

		ModuleMap* get_module_map() {
			static ModuleMap* l = NULL;
			if (!l) {
				l = new ModuleMap;
				l->set_deleted_key("<DELETED KEY>");
				l->set_empty_key("");
			}
			return l;
		}

		ModuleList* get_module_list() {
			static ModuleList* l = NULL;
			if (!l) l = new ModuleList;
			return l;
		}

		bool module_is_loaded(const std::string& full_path, Module*& module) {
			ModuleMap* map = get_module_map();
			ModuleMap::const_iterator it = map->find(full_path);
			if (it != map->end()) {
				module = it->second;
				return true;
			}
			return false;
		}

		void string_to_std_string(StringConstPtr str, std::string& out_str) {
			size_t sz = string_size(str);
			char buffer[sz];
			string_copy_to(str, buffer, sz);
			out_str.assign(buffer, sz);
		}

		bool file_exists(const std::string& path) {
			struct stat buf;
			return stat(path.c_str(), &buf) == 0;
		}

		bool expand_load_path(const std::string& file, std::string& path) {
			if (file[0] == '/') {
				path = file;
				return file_exists(path);
			} else {
				static const char* file_suffixes[] = {"", ".sn", ".snomo" /* TODO: Dynamic libraries? */};
				static const size_t num_file_suffixes = sizeof(file_suffixes) / sizeof(const char*);

				ObjectPtr<Array> load_paths = get_load_paths();
				for (size_t i = 0; i < array_size(load_paths); ++i) {
					Value vp = array_get(load_paths, i);
					ObjectPtr<String> p = vp;
					if (p == NULL) {
						throw_exception_with_description("Load path is not a string: %@.", value_inspect(vp));
					}

					std::string spath;
					string_to_std_string(p, spath);
					if (spath.size() && spath[spath.size()-1] != '/') {
						spath += '/';
					}

					for (size_t j = 0; j < num_file_suffixes; ++j) {
						// TODO: This can be made much, much faster with a stringref-like class
						std::string candidate = spath + file + file_suffixes[j];
						if (file_exists(candidate)) {
							path = candidate;
							return true;
						}
					}
				}
				return false;
			}
		}

		inline std::string load_source(const std::string& path) {
			struct stat stats;
			if (stat(path.c_str(), &stats) == 0) {
				char* tmp = (char*)malloc(stats.st_size+1);
				FILE* fp = fopen(path.c_str(), "r");
				fread(tmp, stats.st_size, 1, fp);
				fclose(fp);

				tmp[stats.st_size] = '\0';
				std::string str(tmp, stats.st_size);
				free(tmp);
				return str;
			} else {
				fprintf(stderr, "ERROR: WTF? Trying to load non-existing source: %s\n", path.c_str());
				return NULL;
			}
		}

		inline ModuleType get_module_type(const std::string& path) {
			const std::string extension = strrchr(path.c_str(), '.') + 1;
			if (extension == "sn") {
				return ModuleTypeSource;
			} else if (extension == "snomo") {
				return ModuleTypeDynamicLibrary;
			} else {
				fprintf(stderr, "WARNING: Unknown file extension '%s', assuming source code.\n", extension.c_str());
				return ModuleTypeSource;
			}
		}

		inline std::string get_module_name(const std::string& path) {
			std::string module_name = path;
			char* p = strrchr(path.c_str(), '/');
			if (p++) {
				char* q = strrchr(p, '.');
				if (q) {
					module_name = std::string(p, q);
				} else {
					module_name = p;
				}
			}
			return module_name;
		}

		inline Module* compile_module(const std::string& path, const std::string& source, Value mod) {
			Module* m = new Module;
			m->path = path;
			m->source = source;
			m->module = mod;

			struct ASTBase* ast = snow::parse(m->source.c_str());
			if (ast) {
				snow::CodeModule* code = snow::get_code_manager()->compile_ast(ast, m->source.c_str(), get_module_name(path).c_str());
				if (code) {
					get_module_list()->push_back(m);
					m->entry = snow::create_function_for_descriptor(code->entry, NULL);

					// Call module entry
					static SnArguments args = {}; // zero arguments
					CallFrame frame = {
						.function = m->entry,
						.self = NULL,
						.locals = NULL,
						.args = &args,
						.environment = NULL
					};
					Value result = function_call(m->entry, &frame);
					object_set_instance_variable(mod, snow::sym("__module_value__"), result);
					// Import module globals
					for (size_t i = 0; i < code->num_globals; ++i) {
						object_set_instance_variable(mod, code->global_names[i], code->globals[i]);
					}
					return m;
				}
			}
			delete m;
			fprintf(stderr, "ERROR: Could not compile module.\n");
			return NULL;
		}

		inline Module* load_module(const std::string& path) {
			//ASSERT(path.size() && path[0] == '/'); // path must be expanded!
			Module* module = NULL;
			switch (get_module_type(path)) {
				case ModuleTypeSource: {
					Value mod = create_object(get_object_class(), 0, NULL);
					object_give_meta_class(mod);
					module = compile_module(path, load_source(path), mod);
					break;
				}
				case ModuleTypeDynamicLibrary: {
					void* snomo = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
					if (snomo != NULL) {
						void* ptr = dlsym(snomo, "snow_module_init");
						if (ptr != NULL) {
							VALUE(*entry)() = (VALUE(*)())ptr;
							module = new Module;
							module->path = path;
							module->snomo = snomo;
							get_module_list()->push_back(module);
							(*get_module_map())[path] = module;
							module->module = entry();
						} else {
							throw_exception_with_description("dlopen(%@): %@", path, strerror(errno));
						}
					} else {
						throw_exception_with_description("dlopen(%@): %@", path, strerror(errno));
					}
					break;
				}
				default: {
					throw_exception_with_description("Only Snow source code files are supported at this time.");
					return NULL;
				}
			}
			ModuleMap& map = *get_module_map();
			map[path] = module;
			return module;
		}
	}
	
	ObjectPtr<Array> get_load_paths() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Array> load_paths = create_array_with_size(10);
			array_push(load_paths, create_string_constant("./"));
			array_push(load_paths, create_string_constant("./lib"));
			root = gc_create_root(load_paths);
		}
		return *root;
	}
	
	Value get_global_module() {
		static Value* root = NULL;
		if (!root) {
			Value global_module = create_object(get_object_class(), 0, NULL);
			object_give_meta_class(global_module);
			root = gc_create_root(global_module);
		}
		return *root;
	}
	
	Value import(StringConstPtr _file) {
		std::string file;
		string_to_std_string(_file, file);
		std::string path;
		if (expand_load_path(file, path))
		{
			Module* module = NULL;
			if (module_is_loaded(path, module)) {
				return module->module;
			} else {
				module = load_module(path);
				return module->module;
			}
		}
		throw_exception_with_description("File not found in any load path: %@", file.c_str());
		return NULL;
	}
	
	Value load(StringConstPtr _file) {
		std::string file;
		string_to_std_string(_file, file);
		std::string path;
		if (expand_load_path(file, path)) {
			Module* module = load_module(path);
			return module->module;
		}
		return NULL;
	}
	
	Value load_in_global_module(StringConstPtr _file) {
		std::string file;
		string_to_std_string(_file, file);
		std::string path;
		if (expand_load_path(file, path)) {
			ASSERT(get_module_type(path) == ModuleTypeSource); // only source modules are supported in load_in_global_module
			Value mod = get_global_module();
			if (compile_module(path, load_source(file), mod)) {
				return object_get_instance_variable(mod, snow::sym("__module_value__"));
			} else {
				fprintf(stderr, "ERROR: Could not compile module: %s\n", path.c_str());
				return NULL;
			}
		} else {
			throw_exception_with_description("File not found in any load path: %@", file.c_str());
			return NULL;
		}
	}
	
	Value eval_in_global_module(StringConstPtr _source) {
		std::string source;
		string_to_std_string(_source, source);
		if (!source.size()) return NULL;
		Value mod = get_global_module();
		Module* module = compile_module("<eval>", source, mod);
		if (module) {
			return object_get_instance_variable(mod, snow::sym("__module_value__"));
		} else {
			return NULL;
		}
	}
	
	void _module_finished(Value module) {
		ModuleList& list = *get_module_list();
		for (ModuleList::iterator it = list.begin(); it != list.end(); ++it) {
			if ((*it)->module == module) {
				delete *it;
				list.erase(it);
				return;
			}
		}
	}
}