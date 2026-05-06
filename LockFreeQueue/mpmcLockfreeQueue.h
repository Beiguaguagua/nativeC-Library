#ifndef _MPMCLOCKFREEQUEUE_H_
#define _MPMCLOCKFREEQUEUE_H_

#include <atomic>
#include <vector>
#include <chrono>
#include <thread>
#include <array>

namespace mpmc {
	template <typename T,size_t q_len = 1024*24>
	class LockFreeQueue
	{
	public:
		LockFreeQueue(size_t queue_len = 1024*24) {
			basic.resize(queue_len);
			_capacity = queue_len;
			for (int i = 0; i < queue_len; i++) {
				seq[i].store(i, std::memory_order_relaxed);
			}
		}
		~LockFreeQueue() {
			
		}

		static constexpr size_t count2Index(size_t cur_count, size_t _capa) {
			return (cur_count % _capa);
		}

		bool enqueue(T& ele) {
			size_t cur_widx, idx;
			cur_widx = w_idx.fetch_add(1, std::memory_order_acq_rel);
			idx = count2Index(cur_widx, _capacity);
			size_t spins = 0;
			while (true) {
				size_t seq_item = seq[idx].load(std::memory_order_acquire);
				intptr_t dif = (intptr_t)seq_item - (intptr_t)idx;
				if (dif == 0)break;
				if ((spins & 0x3f) == 0) std::this_thread::sleep_for(std::chrono::microseconds(50));
				else std::this_thread::yield();
			}
			basic[idx] = ele;
			std::atomic_thread_fence(std::memory_order_release);
			seq[idx].store(idx + 1, std::memory_order_release);
			return true;
		}

		bool enqueue(T&& ele) {
			size_t cur_widx, idx;
			cur_widx = w_idx.fetch_add(1, std::memory_order_acq_rel);
			idx = count2Index(cur_widx, _capacity);
			size_t spins = 0;
			while (true) {
				size_t seq_item = seq[idx].load(std::memory_order_acquire);
				intptr_t dif = (intptr_t)seq_item - (intptr_t)idx;
				if (dif == 0)break;
				if ((spins & 0x3f) == 0) std::this_thread::sleep_for(std::chrono::microseconds(50));
				else std::this_thread::yield();
			}
			basic[idx] = std::move(ele);
			std::atomic_thread_fence(std::memory_order_release);
			seq[idx].store(idx + 1, std::memory_order_release);
			return true;
		}

		bool dequeue(T& ele) {
			size_t cur_ridx = r_idx.load(std::memory_order_relaxed);
			size_t idx,seq_item;
			intptr_t dif;
			while (true) {
				idx = count2Index(cur_ridx, _capacity);
				seq_item = seq[idx].load(std::memory_order_acquire);
				dif = (intptr_t)seq_item - (intptr_t)(cur_ridx + 1);
				if (dif == 0) {
					if (r_idx.compare_exchange_weak(cur_ridx, cur_ridx + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
						ele = std::move(basic[idx]);
						std::atomic_thread_fence(std::memory_order_release);
						seq[idx].store(cur_ridx + _capacity, std::memory_order_release);
						break;
					}
				}
				else if (dif < 0)return false;
				else {
					cur_ridx = r_idx.load(std::memory_order_relaxed);
				}
			}
			return true;
		}

	private:
		alignas(64) std::vector<T> basic;
		alignas(64) size_t _capacity = 0;    

		alignas(64) std::atomic<size_t> w_idx{ 0 };
		alignas(64) std::atomic<size_t> r_idx{ 0 };

		alignas(64) std::array<std::atomic<size_t>,q_len> seq;

	};
}

#endif // !_MPMCLOCKFREEQUEUE_H_
