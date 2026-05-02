#pragma once
#include <thread>
#include <array>
#include <vector>
#include <atomic>
#include <functional>
#include "spmcLockfreeQueue.h"

#define THREAD_POOL_DEFAULT_SIZE  1024				/* 线程池默认空间 */

namespace pool {
	template <size_t pool_init_len = THREAD_POOL_DEFAULT_SIZE>
	class FixedSizeThreadPool {
	public:
		/// <summary>
		/// 通过模长计算循环队列索引
		/// </summary>
		/// <param name="idx"></param>
		/// <param name="capa"></param>
		/// <returns></returns>
		static constexpr uint64_t CountToIndex(uint64_t idx, size_t capa) {
			return (idx % capa);
		}

		FixedSizeThreadPool() {
			pool_len = pool_init_len;
			basic.resize(pool_len);
			for (int i = 0; i < pool_len; i++) {
				ready[i].store(false, std::memory_order_relaxed);
				basic[i] = std::thread(&FixedSizeThreadPool::worker_loop, this,i);
			}
		}

		~FixedSizeThreadPool() {
			for (auto& t : basic) {
				if (t.joinable()) {
					t.join();
				}
			}
			basic.clear();
		}
		/// <summary>
		/// 任务入队线程池
		/// </summary>
		/// <param name="f"></param>
		/// <returns></returns>
		bool add_task(std::function<void()> f) {
			size_t cur_run_idx = run_idx.load(std::memory_order_acquire);
			size_t idx;
			// 检查是否有空余的线程
			while (true) {
				idx = CountToIndex(cur_run_idx, pool_len);
				bool valid = ready[idx].load(std::memory_order_acquire);
				if (valid) {
					cur_run_idx++;
					cpu_pause();
					
					continue;
				}
				bool expected = false;
				if (ready[idx].compare_exchange_weak(expected, false,std::memory_order_acq_rel))break;
				cpu_pause();
			}
			bool en_queue_status = task_queue.enqueue_bulk(f);
			if (en_queue_status) {
				ready[idx].store(true, std::memory_order_release);
				// 更新索引
				run_idx.store(idx + 1, std::memory_order_release);
				return true;
			}
			return false;
		}
	private:

		/// <summary>
		/// CPU片间休息
		/// </summary>
		inline void cpu_pause()  noexcept {
			std::this_thread::yield();
		}

		/// <summary>
		/// 线程池线程无函数运行休息
		/// </summary>
		inline void thread_in_run_pause()  noexcept {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		/// <summary>
		/// 线程池线程有函数运行，但是需要出队。
		/// </summary>
		inline void thread_in_function_pause() noexcept {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		/// <summary>
		/// 线程池的子线程内部循环执行
		/// </summary>
		/// <param name="idx"></param>
		void worker_loop(size_t idx) {
			std::function<void()> f;
			bool valid = false;
			while (true) {
				if (ready[idx].load(std::memory_order_acquire)) {
					while (true) {
						valid = task_queue.dequeue_bulk(f);
						if (!valid) {
							thread_in_function_pause();
							continue;
						}
						f();
						f = nullptr;
						break;
					}
					ready[idx].store(false, std::memory_order_release);
				}
				thread_in_run_pause();
			}
		}

		/// <summary>
		/// 线程管理基础容器
		/// </summary>
		alignas(64) std::vector<std::thread> basic;

		/// <summary>
		/// 线程运行标记状态,false标记为未运行，true标记为正在运行
		/// </summary>
		alignas(64) std::array<std::atomic_bool, pool_init_len> ready;

		/// <summary>
		/// 任务队列
		/// </summary>
		alignas(64) spmc::LockFreeQeue<std::function<void()>, pool_init_len > task_queue;

		/// <summary>
		/// 执行索引
		/// </summary>
		alignas(64) std::atomic<size_t> run_idx{ 0 };

		/// <summary>
		/// 线程池基础长度
		/// </summary>
		alignas(64) size_t pool_len = 0;
	};
}
