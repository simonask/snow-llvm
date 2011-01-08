#include "snow/snow.h"
#include "snow/module.h"
#include "snow/array.h"
#include "snow/type.h"
#include "snow/str.h"
#include "snow/function.h"
#include <google/dense_hash_map>
#include <string>
#include <vector>
#include <sys/stat.h>

struct Module {
	SnObject* module;
	SnFunction* entry;
	std::string path;
	std::string source;
	// AST?
};

typedef google::dense_hash_map<std::string, Module*> ModuleMap;
typedef std::vector<Module*> ModuleList;

namespace {
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
	
	bool file_exists(const std::string& path) {
		struct stat buf;
		return stat(path.c_str(), &buf) == 0;
	}
	
	bool expand_load_path(const std::string& file, std::string& path) {
		if (file[0] == '/') {
			path = file;
			return file_exists(path);
		} else {
			SnArray* load_paths = snow_get_load_paths();
			for (size_t i = 0; i < snow_array_size(load_paths); ++i) {
				VALUE vpath = snow_array_get(load_paths, i);
				if (snow_type_of(vpath) != SnStringType) {
					//vpath = snow_value_to_string(vpath);
					continue;
				}
				SnString* spath = (SnString*)vpath;
				size_t spath_len = snow_string_size(spath);
				const char* spath_cstr = snow_string_cstr(spath);
				if (spath_len && spath_cstr[spath_len-1] != '/') {
					snow_string_append_cstr(spath, "/");
					spath_cstr = snow_string_cstr(spath);
					++spath_len;
				}
				
				size_t candidate_len = spath_len + file.size();
				char* candidate = (char*)alloca(candidate_len + 1);
				memcpy(candidate, spath_cstr, spath_len);
				memcpy(candidate + spath_len, file.c_str(), file.size());
				candidate[candidate_len] = '\0';
				
				if (file_exists(candidate)) {
					path = candidate;
					return true;
				}
			}
			return false;
		}
	}
	
	inline std::string load_source(const std::string& path) {
		fprintf(stderr, "Loading source: %s\n", path.c_str());
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
	
	inline Module* compile_module(const std::string& path, const std::string& source, SnObject* mod) {
		Module* m = new Module;
		m->path = path;
		m->source = source;
		m->module = mod;
		
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
		
		m->entry = snow_compile(module_name.c_str(), m->source.c_str());
		
		if (m->entry) {
			get_module_list()->push_back(m);
			
			// call module entry
			SnFunctionCallContext* context = snow_create_function_call_context(m->entry, NULL, 0, NULL, 0, NULL);
			context->module = mod;
			VALUE result = snow_function_call(m->entry, context, mod, NULL);
			snow_object_set_member(mod, mod, snow_sym("__module_value__"), result);
			return m;
		} else {
			fprintf(stderr, "ERROR: Could not compile module.\n");
			delete m;
			return NULL;
		}
	}
}

CAPI {
	SnArray* snow_get_load_paths() {
		static SnArray* load_paths = NULL; // TODO: GC
		if (!load_paths) {
			load_paths = snow_create_array_with_size(10);
			snow_array_push(load_paths, snow_create_string_constant("./"));
		}
		return load_paths;
	}
	
	SnObject* snow_get_global_module() {
		static SnObject* global_module = NULL; // TODO: GC
		if (!global_module) {
			global_module = snow_create_object(NULL);
		}
		return global_module;
	}
	
	SnObject* snow_import(const char* file) {
		std::string path;
		if (expand_load_path(file, path))
		{
			Module* module = NULL;
			if (module_is_loaded(path, module)) {
				return module->module;
			}
			return snow_load(path.c_str());
		}
		fprintf(stderr, "ERROR: file not found in any load path: %s\n", file);
		return NULL; // TODO: Exception?
	}
	
	SnObject* snow_load(const char* file) {
		std::string path;
		if (expand_load_path(file, path)) {
			SnObject* mod = snow_create_object(NULL);
			Module* module = compile_module(path, load_source(path), mod);
			
			ModuleMap& map = *get_module_map();
			map[path] = module;
			return module->module;
		}
		return NULL;
	}
	
	VALUE snow_load_in_global_module(const char* file) {
		std::string path;
		if (expand_load_path(file, path)) {
			SnObject* mod = snow_get_global_module();
			if (compile_module(path, load_source(file), mod)) {
				return snow_object_get_member(mod, mod, snow_sym("__module_value__"));
			} else {
				fprintf(stderr, "ERROR: Could not compile module: %s\n", path.c_str());
				return NULL;
			}
		} else {
			fprintf(stderr, "ERROR: File not found in any load path: %s\n", file);
			return NULL;
		}
	}
	
	VALUE snow_eval_in_global_module(const char* source) {
		SnObject* mod = snow_get_global_module();
		Module* module = compile_module("<eval>", source, mod);
		if (module) {
			return snow_object_get_member(mod, mod, snow_sym("__module_value__"));
		} else {
			return NULL;
		}
	}
	
	void _snow_module_finished(SnObject* module) {
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