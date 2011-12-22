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
	
	class Value {
		VALUE value_;
		void assign(VALUE val);
	public:
		Value() : value_(NULL) {}
		Value(VALUE val) : value_(NULL) { assign(val); }
		Value(const Value& other) : value_(NULL) { assign(other.value()); }
		Value(Value&& other) { value_ = other.value_; other.value_ = NULL; }
		~Value() { assign(NULL); }
		Value& operator=(VALUE other) { assign(other); return *this; }
		Value& operator=(const Value& other) { assign(other.value_); return *this; }
		Value& operator=(Value&& other) { value_ = other.value_; other.value_ = NULL; return *this;}
		VALUE value() const { return value_; }
		operator VALUE() const { return value_; }
		
		bool operator==(const Value& other) const { return value_ == other.value_; }
		bool operator==(VALUE other) const { return value_ == other; }
		bool operator!=(const Value& other) const { return value_ != other.value_; }
		bool operator!=(VALUE other) const { return value_ != other; }
		
		bool is_object() const { return snow::is_object(value_); }
		bool is_immediate() const { return !is_object(); }
		bool is_integer() const;
		bool is_float() const;
		bool is_nil() const;
		bool is_boolean() const;
		bool is_symbol() const;
	};
	
	class Immediate {
		VALUE value_;
	public:
		Immediate(); // NULL (nil) by default
		Immediate(VALUE value) { ASSERT(is_immediate(value)); value_ = value; }
		Immediate(const Value& value) { ASSERT(value.is_immediate()); value_ = value.value(); }
		operator Value() const { return value_; }
		VALUE value() const { return value_; }
		
		bool operator==(const Immediate& other) const { return value_ == other.value_; }
		bool operator!=(const Immediate& other) const { return value_ != other.value_; }
		bool operator>(const Immediate& other) const { return value_ > other.value_; }
		bool operator>=(const Immediate& other) const { return value_ >= other.value_; }
		bool operator<(const Immediate& other) const { return value_ < other.value_; }
		bool operator<=(const Immediate& other) const { return value_ <= other.value_; }
		
		bool is_integer() const;
		bool is_float() const;
		bool is_nil() const;
		bool is_boolean() const;
		bool is_symbol() const;
	};
}

#endif /* end of include guard: VALUE_H_ATXVBTXI */
