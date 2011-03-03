#pragma once
#ifndef LOCK_HPP_TC4L5SG6
#define LOCK_HPP_TC4L5SG6

#include "snow/gc.h"

namespace snow {
	struct ScopedGCReadLock {
		const SnObjectBase* object;
		
		ScopedGCReadLock(const SnObjectBase* object) : object(object) {
			snow_gc_rdlock(object, &object);
		}
		~ScopedGCReadLock() {
			snow_gc_unlock(object);
		}
	};
	
	struct ScopedGCWriteLock {
		const SnObjectBase* object;
		
		ScopedGCWriteLock(const SnObjectBase* object) : object(object) {
			snow_gc_wrlock(object, &object);
		}
		~ScopedGCWriteLock() {
			snow_gc_unlock(object);
		}
	};
}

#define SN_GC_SCOPED_RDLOCK(OBJECT) snow::ScopedGCReadLock _scoped_gc_rdlock ## __LINE__(&(OBJECT)->base)
#define SN_GC_SCOPED_WRLOCK(OBJECT) snow::ScopedGCWriteLock _scoped_gc_wrlock ## __LINE__(&(OBJECT)->base)

#endif /* end of include guard: LOCK_HPP_TC4L5SG6 */
