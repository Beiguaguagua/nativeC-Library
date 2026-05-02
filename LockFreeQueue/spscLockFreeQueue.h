#pragma once
#include <vector>
#include <thread>
#include <atomic>
#define SPSC_DEFAULT_LEN  1024*256
/*
* 服务于SPSC场景下的无锁循环队列
* create_time:2026/04/29
*/

inline void cpu_pause() {
	std::this_thread::yield();
}

namespace spsc {
	template <typename T,size_t queue_len = SPSC_DEFAULT_LEN>
	class LockFreeQueue 
	{
		static constexpr uint64_t CountToIndex(uint64_t idx, size_t capa) {
			return (idx % capa);
		}
	public:
		LockFreeQueue() {
			_capacity = queue_len;
		}
		~LockFreeQueue() = default;

		bool enqueue(T& ele) {
			uint64_t cur_widx, cur_ridx,idx;
			cur_widx = widx.load(std::memory_order_relaxed);
			cur_ridx = ridx.load(std::memory_order_acquire);
			if ((cur_widx - cur_ridx) >= _capacity)return false;
			idx = CountToIndex(cur_widx, _capacity);
			basic[idx] = std::move(ele);
			widx.store(cur_widx + 1, std::memory_order_release);
			return true;
		}

		bool dequeue(T& ele) {
			uint64_t cur_widx, cur_ridx, idx;
			cur_ridx = ridx.load(std::memory_order_relaxed);
			cur_widx = widx.load(std::memory_order_acquire);
			if (cur_ridx >= cur_widx)break;
			idx = CountToIndex(cur_ridx, _capacity);
			ele = std::move(basic[idx]);
			basic[idx] = T{};
			ridx.store(cur_ridx + 1, std::memory_order_release);
			return true;
		}

	private:
		std::array<T, queue_len> basic;
		std::atomic<size_t> widx;
		std::atomic<size_t> ridx;
		size_t _capacity = 0;
	};
}
