#include "snow/symbol.h"
#include "snow/type.h"
#include "internal.h"
#include "snow/class.h"
#include "snow/function.h"
#include "snow/exception.h"
#include "snow/str.h"

#include <map>
#include <string>

// TODO: Figure out a way to use google::dense_hash_map with -fno-rtti
typedef std::map<std::string, SnSymbol> SymbolTable;

static SymbolTable& symbol_table() {
	static SymbolTable* t = NULL;
	if (!t) {
		t = new SymbolTable;
		/*t->set_deleted_key(std::string("<DELETED SYMBOL>"));
		t->set_empty_key(std::string());*/
	}
	return *t;
}

static VALUE symbol_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_is_symbol(self));
	const char* symstr = snow_sym_to_cstr(snow_value_to_symbol(self));

	if (!symstr) {
		snow_throw_exception_with_description("'%p' is not in the symbol table.", self);
		return NULL;
	}

	size_t len = strlen(symstr);
	char str[len+2];
	str[0] = '#';
	memcpy(str+1, symstr, len);
	str[len+1] = '\0';
	return snow_create_string(str);
}

static VALUE symbol_to_string(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_is_symbol(self));
	return snow_create_string_constant(snow_sym_to_cstr(snow_value_to_symbol(self)));
}

CAPI {
	SnSymbol snow_sym(const char* str) {
		SymbolTable& t = symbol_table();
		std::string s(str);
		SymbolTable::iterator it = t.find(s);
		if (it != t.end()) {
			return it->second;
		} else {
			SnSymbol sym = t.size() + 1;
			t[s] = sym;
			return sym;
		}
	}
	
	const char* snow_sym_to_cstr(SnSymbol sym) {
		SymbolTable& t = symbol_table();
		for (SymbolTable::const_iterator it = t.begin(); it != t.end(); ++it) {
			if (it->second == sym) {
				return it->first.c_str();
			}
		}
		return NULL;
	}

	SnClass* snow_get_symbol_class() {
		static VALUE* root = NULL;
		if (!root) {
			SnClass* cls = snow_create_class(snow_sym("Symbol"), NULL);
			snow_class_define_method(cls, "inspect", symbol_inspect, 0);
			snow_class_define_method(cls, "to_string", symbol_to_string, 0);
			root = snow_gc_create_root(cls);
		}
		return (SnClass*)*root;
	}
}
