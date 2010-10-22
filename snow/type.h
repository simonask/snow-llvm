#pragma once
#ifndef TYPE_H_ZR7ZWP35
#define TYPE_H_ZR7ZWP35

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"

struct SnProcess;
struct SnObject;
struct SnArguments;

typedef void(*SnInitializeFunc)(SN_P, struct SnObject* obj);
typedef void(*SnFinalizeFunc)(SN_P, struct SnObject* obj);
typedef void(*SnCopyFunc)(SN_P, struct SnObject* copy, const struct SnObject* original);
typedef void(*SnForEachRootCallbackFunc)(SN_P, VALUE, VALUE* root, void* userdata);
typedef void(*SnForEachRootFunc)(SN_P, VALUE self, SnForEachRootCallbackFunc func, void* userdata);
typedef VALUE(*SnGetMemberFunc)(SN_P, VALUE self, SnSymbol member);
typedef VALUE(*SnSetMemberFunc)(SN_P, VALUE self, SnSymbol member, VALUE value);
typedef VALUE(*SnCallFunc)(SN_P, VALUE functor, VALUE self, struct SnArguments*);

typedef struct SnType {
	size_t size;
	SnInitializeFunc initialize;
	SnFinalizeFunc finalize;
	SnCopyFunc copy;
	SnForEachRootFunc for_each_root;
	SnGetMemberFunc get_member;
	SnSetMemberFunc set_member;
	SnCallFunc call;
} SnType;

static const size_t SN_TYPE_INVALID_SIZE = (size_t)-1LL;


CAPI void snow_init_immediate_type(SnType* type);

#endif /* end of include guard: TYPE_H_ZR7ZWP35 */
