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

VALUE snow_vsym(SN_P p, const char* str) {
	return snow_symbol_to_value(snow_sym(p, str));
}

SnSymbol snow_sym(SN_P p, const char* cstr) {
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

const char* snow_sym_to_cstr(SN_P p, SnSymbol sym) {
	SymbolTable& t = symbol_table();
	SymbolTable::const_iterator it = t.begin();
	for (SymbolTable::const_iterator it = t.begin(); it != t.end(); ++it) {
		if (it->second == sym) {
			return it->first.c_str();
		}
	}
	return NULL;
}

static VALUE symbol_get_member(SN_P p, VALUE self, SnSymbol member) {
	return NULL;
}

static VALUE symbol_set_member(SN_P p, VALUE self, SnSymbol member, VALUE val) {
	return NULL;
}

static VALUE symbol_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	// TODO: Convert functor to symbol and call it as a member function on first argument
	return functor;
}


static SnType SymbolType;

const SnType* snow_get_symbol_type() {
	static const SnType* type = NULL;
	if (!type) {
		snow_init_immediate_type(&SymbolType);
		SymbolType.get_member = symbol_get_member;
		SymbolType.set_member = symbol_set_member;
		SymbolType.call = symbol_call;
		type = &SymbolType;
	}
	return type;
}
