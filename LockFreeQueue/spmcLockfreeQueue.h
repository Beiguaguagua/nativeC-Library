#pragma once
#include <vector>
#include <array>
#include <atomic>
#include <cstddef>
#include <thread>
#define DEFAULT_LEN		1024*256


namespace spmc {
	template <typename T,size_t queue_len = DEFAULT_LEN>
	class LockFreeQeue
	{
		static constexpr uint64_t CountToIndex(uint64_t idx,size_t capa) {
			return (idx % capa);
		}
	public:
		LockFreeQeue() {
			for (auto& r : ready) {
				r.store(true, std::memory_order_relaxed);
			}
			_capacity = queue_len;
			m_wIdx.store(0, std::memory_order_relaxed);
			m_rIdx.store(0, std::memory_order_relaxed);
		}

		~LockFreeQeue() = default;

		/// <summary>
		/// 获取当前队列元素情况
		/// </summary>
		/// <returns></returns>
		size_t size() const {
			const uint64_t w = m_wIdx.load(std::memory_order_acquire);
			const uint64_t r = m_rIdx.load(std::memory_order_acquire);
			return (w - r) > _capacity ? 0 : (w - r);
		}

		size_t capacity() const {
			return queue_len;
		}

		/// <summary>
		/// 强制入队
		/// </summary>
		/// <param name="elem"></param>
		/// <returns></returns>
		bool enqueue_bulk(T& elem) {
			uint64_t cur_wIdx;
			uint64_t cur_rIdx;
			while (true) {
				cur_wIdx = m_wIdx.load(std::memory_order_relaxed);
				cur_rIdx = m_rIdx.load(std::memory_order_acquire);
				if ((cur_wIdx - cur_rIdx) < _capacity) break;
				cpu_pause();
			}
			uint64_t idx = CountToIndex(cur_wIdx,_capacity);
			// 等待上一个消费者读取完
			while (!ready[idx].load(std::memory_order_relaxed))
				cpu_pause();
			basic[idx] = std::move(elem);
			// 标记该位置已经写入数据，等待取数

			ready[idx].store(false, std::memory_order_relaxed);
			m_wIdx.store(cur_wIdx + 1, std::memory_order_release);
			return true;
		}

		/// <summary>
		/// 强制出队
		/// </summary>
		/// <param name="elem"></param>
		/// <returns></returns>
		bool dequeue_bulk(T& elem) {
			uint64_t cur_ridx, cur_widx,idx;
			for (;;) {
				cur_ridx = m_rIdx.load(std::memory_order_relaxed);
				cur_widx = m_wIdx.load(std::memory_order_acquire);

				if (cur_ridx >= cur_widx) {
					cpu_pause();
					continue;
				}
				if (m_rIdx.compare_exchange_weak(cur_ridx, cur_ridx + 1, std::memory_order_relaxed, std::memory_order_relaxed))break;
			}
			idx = CountToIndex(cur_ridx, _capacity);

			elem = std::move(basic[idx]);
			ready[idx].store(true, std::memory_order_release);
			return true;
		}

	private:
		alignas(64) std::array<T, queue_len> basic;
		alignas(64) std::atomic<uint64_t> m_wIdx{ 0 };							//  写索引
		alignas(64) std::atomic<uint64_t> m_rIdx{ 0 };							//  读索引
		alignas(64) std::array<std::atomic<bool>, queue_len> ready;				//	ready[i]=true 表示槽位空闲,生产者可写;ready[i]=false 表示槽位正在被读取，生产者需要等待
		alignas(64) size_t _capacity = 0;										//	队列容量

		/// <summary>
		/// CPU休息
		/// </summary>
		inline void cpu_pause() noexcept{
			std::this_thread::yield();
		}

	};
}

