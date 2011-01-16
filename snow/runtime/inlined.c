/*
	This file makes sure that functions that are marked as
	"inline" in the runtime will still be accessible from outside modules.
*/

#define inline 

#include "snow/value.h"
#include "snow/str.h"