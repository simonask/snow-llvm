#include "codebuffer.hpp"

#include <algorithm>

namespace snow {
	void CodeBuffer::render_at(byte* location, size_t max_size) const {
		ASSERT(buffer.size() <= max_size);
		if (max_alignment > 1) {
			ASSERT(((intptr_t)location & (max_alignment-1)) == 0); // target location is not aligned!
		}
		std::copy(buffer.begin(), buffer.end(), location);
		perform_fixups(location);
	}
	
	void CodeBuffer::perform_fixups(byte* location) const {
		for (auto it = fixups.begin(); it != fixups.end(); ++it) {
			const Fixup& fixup = *it;
			int64_t value = it->value + it->displacement;
			if (it->type == FixupRelative) {
				value -= it->position;
			} else if (it->type == FixupPointerToOffset) {
				value = (int64_t)(location + value);
			} else if (it->type == FixupOffsetToPointer) {
				value = value - ((intptr_t)location + it->position);
			}
			
			#if defined(DEBUG)
			switch (it->size) {
				case 1: ASSERT(value >= INT8_MIN && value <= INT8_MAX);   // truncation!
				case 2: ASSERT(value >= INT16_MIN && value <= INT64_MAX); // truncation!
				case 4: ASSERT(value >= INT32_MIN && value <= INT32_MAX); // truncation!
				default: break;
			}
			#endif
			
			byte* src = reinterpret_cast<byte*>(&value);
			std::copy(src, src + it->size, location + it->position);
		}
	}
}