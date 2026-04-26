#ifndef SPLINLOCK_H
#define SPLINLOCK_H

#include <atomic>
#include <thread>
#include <chrono>
/*
* author - Beiguagua
* create DateTime - 2026/04/26 15:55
* read_me -	采用atomic_flag编写的自旋锁，默认睡眠时间是10ms
* think -	之前采用的是atomic_bool去实现，对于大部分场景来说，我认为atomic_bool和atomic_flag的性能是差不多的。\n
*			但是atomic_bool内部自带了一个bool的地址变量,从无锁性能上而言，无地址变量是优于带地址变量的，理解是来自于DEV59的关于无锁和有锁的讨论->
*			"https://dev59.com/RYrda4cB1Zd3GeqPSOPX"
*			其实还是很有必要去查阅《C++ Concurrency in Action》里面的内容，我打算去买一本《C++ Concurrency in Action》。走你。学无止境。
*/

namespace Lock {
	/// <summary>
	/// 自旋锁
	/// </summary>
	class SpinLock
	{
	public:
		/// <summary>
		/// 初始化函数
		/// </summary>
		SpinLock() = default;

		/// <summary>
		/// 析构函数
		/// </summary>
		~SpinLock() = default;

		// 禁止拷贝
		SpinLock(const SpinLock&) = delete;

		// 禁止移动
		SpinLock& operator=(const SpinLock&) = delete;

		// 默认自旋时间-毫秒类型-10ms
		static constexpr std::chrono::milliseconds default_wait_time = std::chrono::milliseconds(10);

		/// <summary>
		/// 自旋锁入锁
		/// </summary>
		void lock(std::chrono::milliseconds wait_tiem = default_wait_time) &noexcept
		{
			while (true) {
				const std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };	//起始时间点
				while (flag.test_and_set(std::memory_order_acquire)) {
					const std::chrono::steady_clock::time_point now{ std::chrono::steady_clock::now() };//内部自旋的时间点
					std::chrono::milliseconds elapse{ std::chrono::duration_cast <std::chrono::milliseconds> (now - start) };
					if (elapse >= wait_tiem)goto yield;
				}
				return;
			yield:
				std::this_thread::yield();
			}
		}

		bool try_lock() & noexcept{
			return !flag.test_and_set(std::memory_order_acquire);
		}

		/// <summary>
		/// 自旋锁出锁
		/// </summary>
		void unlock() &noexcept
		{
			/* 释放锁 */
			flag.clear(std::memory_order_release);
		}
	private:
		/// <summary>
		/// 初始化标志
		/// </summary>
		std::atomic_flag flag = ATOMIC_FLAG_INIT;
	};
}
#endif //SPINLOCK_H

