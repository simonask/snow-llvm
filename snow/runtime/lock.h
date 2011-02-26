#pragma once
#ifndef LOCK_H_P8N0PHEM
#define LOCK_H_P8N0PHEM

#include <pthread.h>

#define SN_RWLOCK_T pthread_rwlock_t
#define SN_RWLOCK_INIT(X) pthread_rwlock_init(X, NULL)
#define SN_RWLOCK_DESTROY(X) pthread_rwlock_destroy(X)
#define SN_RWLOCK_RDLOCK(X) pthread_rwlock_rdlock(X)
#define SN_RWLOCK_TRYRDLOCK(X) pthread_rwlock_tryrdlock(X)
#define SN_RWLOCK_WRLOCK(X) pthread_rwlock_wrlock(X)
#define SN_RWLOCK_TRYWRLOCK(X) pthread_rwlock_trywrlock(X)
#define SN_RWLOCK_UNLOCK(X) pthread_rwlock_unlock(X)

#endif /* end of include guard: LOCK_H_P8N0PHEM */
