#include "snow/type.h"
#include "snow/object.h"
#include "snow/exception.h"

#include "snow/object.h"
#include "snow/array.h"
#include "snow/map.h"
#include "snow/function.h"
#include "snow/class.h"
#include "snow/str.h"
#include "snow/pointer.h"
#include "snow/fiber.h"

size_t snow_size_of_type(SnType type) {
	switch (type) {
		case SnObjectType:    return sizeof(struct SnObject);
		case SnClassType:     return sizeof(struct SnClass);
		case SnArrayType:     return sizeof(struct SnArray);
		case SnMapType:       return sizeof(struct SnMap);
		case SnStringType:    return sizeof(struct SnString);
		case SnFunctionType:  return sizeof(struct SnFunction);
		case SnCallFrameType: return sizeof(struct SnCallFrame);
		case SnArgumentsType: return sizeof(struct SnArguments);
		case SnPointerType:   return sizeof(struct SnPointer);
		case SnFiberType:     return sizeof(struct SnFiber);
		default:              return 0;
	}
}

