#pragma once
#ifndef LEXER_HPP_S9CRBPS5
#define LEXER_HPP_S9CRBPS5

#include "snow/basic.h"
#include "snow/ast.hpp"
#include "linkbuffer.hpp"

#include <string>

namespace snow {
	struct Token {
		enum Type {
			#define TOKEN(X) X,
			#include "tokens.decl"
			#undef TOKEN
			NUMBER_OF_TOKENS
		};
		
		Type type;
		size_t length;
		LexerLocation location;
		const char* line_begin;
		
		Token(Type t, const char* begin, size_t len, int lineno, const char* line_begin) : type(t), length(len), line_begin(line_begin) {
			location.line = lineno;
			location.column = begin - line_begin + 1;
		}
		Token(const Token& other) : type(other.type), length(other.length), location(other.location), line_begin(other.line_begin) {}
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
		Lexer(const std::string& path, const char* input_stream, size_t length);
		Lexer(const std::string& path, const char* input_stream);
		
		void tokenize();
		
		typedef LinkBuffer<Token>::iterator iterator;
		iterator begin() { return _buffer.begin(); }
		iterator end() { return _buffer.end(); }
	private:
		const std::string& _path;
		const char* _input;
		size_t _input_length;
		LinkBuffer<Token> _buffer;
	};
	
	inline Lexer::Lexer(const std::string& path, const char* input_stream, size_t l) : _path(path), _input(input_stream), _input_length(l) {}
	inline Lexer::Lexer(const std::string& path, const char* input_stream) : _path(path), _input(input_stream), _input_length(_input ? strlen(_input) : 0) {}
}

#endif /* end of include guard: LEXER_HPP_S9CRBPS5 */
