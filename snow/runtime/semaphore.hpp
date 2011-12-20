#pragma once
#ifndef SEMAPHORE_HPP_5ZCU3TMP
#define SEMAPHORE_HPP_5ZCU3TMP

#include <mutex>
#include <condition_variable>

namespace snow {
	class Semaphore {
		int64_t count;
		int64_t num_waiters;
		std::condition_variable count_nonzero;
		std::mutex lock;
	public:
		Semaphore(int64_t initial_count = 0) : count(initial_count), num_waiters(0) {}
		inline void wait() {
			std::unique_lock<std::mutex> ul(lock);
			++num_waiters;
			while (count == 0) {
				count_nonzero.wait(ul);
			}
			--num_waiters;
			--count;
		}
		inline void signal() {
			std::unique_lock<std::mutex> ul(lock);
			if (num_waiters > 0) {
				count_nonzero.notify_one();
			}
			++count;
		}
	};
}

#endif /* end of include guard: SEMAPHORE_HPP_5ZCU3TMP */
