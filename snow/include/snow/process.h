#pragma once
#ifndef PROCESS_H_CGDBE46G
#define PROCESS_H_CGDBE46G

#include "snow/basic.h"

struct SnVM;
struct SnObject;

typedef struct SnProcess {
	struct SnVM* vm;
	struct SnObject** immediate_prototypes;
} SnProcess;

#endif /* end of include guard: PROCESS_H_CGDBE46G */
