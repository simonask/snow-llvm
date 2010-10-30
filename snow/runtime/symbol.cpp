#include "snow/symbol.h"
#include "snow/type.h"

#include <google/dense_hash_map>
#include <string>

typedef google::dense_hash_map<std::string, SnSymbol> SymbolTable;

static SymbolTable& symbol_table() {
	static SymbolTable* t = NULL;
	if (!t) {
		t = new SymbolTable;
		t->set_deleted_key(std::string("<DELETED SYMBOL>"));
		t->set_empty_key(std::string());
	}
	return *t;
}

VALUE snow_vsym(const char* str) {
	return snow_symbol_to_value(snow_sym(str));
}

SnSymbol snow_sym(const char* cstr) {
	std::string str(cstr);
	SymbolTable& t = symbol_table();
	
	SymbolTable::iterator it = t.find(str);
	if (it == t.end()) {
		SnSymbol sym = t.size();
		t[str] = sym;
		return sym;
	}
	return it->second;
}

const char* snow_sym_to_cstr(SnSymbol sym) {
	SymbolTable& t = symbol_table();
	SymbolTable::const_iterator it = t.begin();
	for (SymbolTable::const_iterator it = t.begin(); it != t.end(); ++it) {
		if (it->second == sym) {
			return it->first.c_str();
		}
	}
	return NULL;
}
