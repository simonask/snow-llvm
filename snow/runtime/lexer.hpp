#pragma once
#ifndef LEXER_HPP_S9CRBPS5
#define LEXER_HPP_S9CRBPS5

#include "snow/basic.h"
#include "linkbuffer.hpp"

#include <string.h>

namespace snow {
	struct SourceLocation {
		
	};
	
	struct Token {
		enum Type {
			#define TOKEN(X) X,
			#include "tokens.decl"
			#undef TOKEN
			NUMBER_OF_TOKENS
		};
		
		Type type;
		const char* begin;
		size_t length;
		int line_number;
		const char* line_begin;
		
		Token(Type t, const char* begin, size_t len, int lineno, const char* line_begin) : type(t), begin(begin), length(len), line_number(lineno), line_begin(line_begin) {}
		Token(const Token& other) : type(other.type), begin(other.begin), length(other.length), line_number(other.line_number), line_begin(other.line_begin) {}
	};
	
	inline const char* get_token_name(Token::Type t) {
		switch (t) {
			#define TOKEN(X) case snow::Token::X: return #X;
			#include "tokens.decl"
			#undef TOKEN
			default: return "<UNKNOWN TOKEN>";
		}
	}
	
	class Lexer {
	public:
		Lexer(const char* input_stream, size_t length);
		Lexer(const char* input_stream);
		
		void tokenize();
		
		typedef LinkBuffer<Token>::iterator iterator;
		iterator begin() { return _buffer.begin(); }
		iterator end() { return _buffer.end(); }
	private:
		const char* _input;
		size_t _input_length;
		LinkBuffer<Token> _buffer;
	};
	
	inline Lexer::Lexer(const char* input_stream, size_t l) : _input(input_stream), _input_length(l) {}
	inline Lexer::Lexer(const char* input_stream) : _input(input_stream), _input_length(strlen(_input)) {}
}

#endif /* end of include guard: LEXER_HPP_S9CRBPS5 */
