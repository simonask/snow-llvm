#include "snow/symbol.h"
#include "snow/type.h"

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

namespace snow {
	SnSymbol symbol(const char* str) {
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
	
	const char* symbol_to_cstr(SnSymbol sym) {
		SymbolTable& t = symbol_table();
		for (SymbolTable::const_iterator it = t.begin(); it != t.end(); ++it) {
			if (it->second == sym) {
				return it->first.c_str();
			}
		}
		return NULL;
	}
}
