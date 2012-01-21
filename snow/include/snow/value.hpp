#pragma once
#ifndef VALUE_H_ATXVBTXI
#define VALUE_H_ATXVBTXI

#include "snow/basic.h"

struct SnObject;

namespace snow {
	typedef void* VALUE;
	
	enum ValueType {
		ObjectType = 0x0,
		IntegerType = 0x1,
		NilType = 0x2,   // == SN_NIL
		BooleanType = 0x4, // == SN_FALSE
		// 0x6 unused
		SymbolType = 0x8,
		FloatType = 0xa,

		ValueTypeMask = 0xf,
		AnyType
	};
	
	static const VALUE SN_UNDEFINED = NULL;
	static const VALUE SN_NIL = (VALUE)NilType;
	static const VALUE SN_FALSE = (VALUE)BooleanType;
	static const VALUE SN_TRUE = (VALUE)(0x10 | BooleanType);
	
	INLINE ValueType type_of(const void* val) {
		if (!val) return NilType;
		const uintptr_t t = (uintptr_t)val & ValueTypeMask;
		if (t & 0x1) return IntegerType;
		return (ValueType)t;
	}
	
	INLINE bool is_immediate_type(ValueType type) {
		return type < ValueTypeMask && type != ObjectType;
	}

	INLINE bool is_object(VALUE val) {
		return val && (((uintptr_t)val & ValueTypeMask) == ObjectType);
	}

	INLINE bool is_immediate(VALUE val) {
		return !is_object(val);
	}

	INLINE bool is_truthy(VALUE val) {
		return val != NULL && val != SN_NIL && val != SN_FALSE;
	}
	
	class Immediate;
	
	class Value {
		VALUE value_;
		void assign(VALUE val) { value_ = val; }
	public:
		Value() : value_(NULL) {}
		Value(VALUE val) : value_(NULL) { assign(val); }
		explicit Value(const Value& other) : value_(other.value_) {}
		Value& operator=(const Value& other) { value_ = other.value_; return *this; }
		VALUE value() const { return value_; }
		operator VALUE() const { return value_; }
		ValueType type() const { return type_of(value_); }
		
		bool operator==(Value other) const { return value_ == other.value_; }
		bool operator==(VALUE other) const { return value_ == other; }
		bool operator!=(Value other) const { return value_ != other.value_; }
		bool operator!=(VALUE other) const { return value_ != other; }
		
		bool is_object() const    { return snow::is_object(value_); }
		bool is_immediate() const { return !is_object(); }
		bool is_integer() const   { return type_of(value_) == IntegerType; }
		bool is_float() const     { return type_of(value_) == FloatType; }
		bool is_nil() const       { return type_of(value_) == NilType; }
		bool is_boolean() const   { return type_of(value_) == BooleanType; }
		bool is_symbol() const    { return type_of(value_) == SymbolType; }
	};
	
	class Immediate {
		Value value_;
	public:
		Immediate() {} // NULL (nil) by default
		Immediate(Value value) { ASSERT(value.is_immediate()); value_ = value; }
		Immediate(VALUE value) : value_(value) { ASSERT(value_.is_immediate()); }
		operator VALUE() const { return value_; }
		operator Value() const { return value_; }
		VALUE value() const { return value_.value(); }
		ValueType type() const { return value_.type(); }
		
		bool operator==(Immediate other) const { return value_ == other.value_; }
		bool operator!=(Immediate other) const { return value_ != other.value_; }
		bool operator>(Immediate other) const { return value_ > other.value_; }
		bool operator>=(Immediate other) const { return value_ >= other.value_; }
		bool operator<(Immediate other) const { return value_ < other.value_; }
		bool operator<=(Immediate other) const { return value_ <= other.value_; }
		
		bool is_integer() const { return value_.is_integer(); }
		bool is_float() const   { return value_.is_float(); }
		bool is_nil() const     { return value_.is_nil(); }
		bool is_boolean() const { return value_.is_boolean(); }
		bool is_symbol() const  { return value_.is_symbol(); }
	};
}

#endif /* end of include guard: VALUE_H_ATXVBTXI */
