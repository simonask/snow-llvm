#pragma once
#ifndef LOCK_HPP_TC4L5SG6
#define LOCK_HPP_TC4L5SG6

#include "snow/gc.h"

namespace snow {
	struct ScopedGCReadLock {
		const SnObject* object;
		
		ScopedGCReadLock(const SnObject* object) : object(object) {
			snow_gc_rdlock(object, &object);
		}
		~ScopedGCReadLock() {
			snow_gc_unlock(object);
		}
	};
	
	struct ScopedGCWriteLock {
		const SnObject* object;
		
		ScopedGCWriteLock(const SnObject* object) : object(object) {
			snow_gc_wrlock(object, &object);
		}
		~ScopedGCWriteLock() {
			snow_gc_unlock(object);
		}
	};
}

#define SN_GC_SCOPED_RDLOCK(OBJECT) snow::ScopedGCReadLock _scoped_gc_rdlock ## __LINE__((SnObject*)(OBJECT))
#define SN_GC_SCOPED_WRLOCK(OBJECT) snow::ScopedGCWriteLock _scoped_gc_wrlock ## __LINE__((SnObject*)(OBJECT))

#endif /* end of include guard: LOCK_HPP_TC4L5SG6 */
