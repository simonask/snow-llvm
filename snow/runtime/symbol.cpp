#include "snow/symbol.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/exception.hpp"
#include "snow/str.hpp"

#include <map>
#include <string>

namespace snow {

// TODO: Figure out a way to use google::dense_hash_map with -fno-rtti
	typedef std::map<std::string, Symbol> SymbolTable;

	static SymbolTable& symbol_table() {
		static SymbolTable* t = NULL;
		if (!t) {
			t = new SymbolTable;
		/*t->set_deleted_key(std::string("<DELETED SYMBOL>"));
		t->set_empty_key(std::string());*/
		}
		return *t;
	}

	namespace bindings {
		static VALUE symbol_inspect(const CallFrame* here, VALUE self, VALUE it) {
			ASSERT(is_symbol(self));
			const char* symstr = snow::sym_to_cstr(value_to_symbol(self));

			if (!symstr) {
				throw_exception_with_description("'%p' is not in the symbol table.", self);
				return NULL;
			}

			size_t len = strlen(symstr);
			char str[len+2];
			str[0] = '#';
			memcpy(str+1, symstr, len);
			str[len+1] = '\0';
			return snow::create_string(str);
		}

		static VALUE symbol_to_string(const CallFrame* here, VALUE self, VALUE it) {
			ASSERT(is_symbol(self));
			return snow::create_string_constant(snow::sym_to_cstr(value_to_symbol(self)));
		}
	}

	Symbol sym(const char* str) {
		SymbolTable& t = symbol_table();
		std::string s(str);
		SymbolTable::iterator it = t.find(s);
		if (it != t.end()) {
			return it->second;
		} else {
			Symbol sym = t.size() + 1;
			t[s] = sym;
			return sym;
		}
	}

	const char* sym_to_cstr(Symbol sym) {
		SymbolTable& t = symbol_table();
		for (SymbolTable::const_iterator it = t.begin(); it != t.end(); ++it) {
			if (it->second == sym) {
				return it->first.c_str();
			}
		}
		return NULL;
	}

	ObjectPtr<Class> get_symbol_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class(snow::sym("Symbol"), NULL);
			SN_DEFINE_METHOD(cls, "inspect", bindings::symbol_inspect);
			SN_DEFINE_METHOD(cls, "to_string", bindings::symbol_to_string);
			root = gc_create_root(cls);
		}
		return *root;
	}
}