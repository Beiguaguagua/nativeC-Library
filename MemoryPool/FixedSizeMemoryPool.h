#pragma once
#include <vector>

/// <summary>
/// 向上对齐
/// </summary>
/// <param name="n"></param>
/// <param name="align"></param>
/// <returns></returns>
static inline size_t align_up(size_t n, size_t align) {
	return (n + (align - 1)) & ~(align - 1);
}

class FixedSizeMemoryPool {
public:
	/// <summary>
	/// 初始化函数
	/// </summary>
	FixedSizeMemoryPool(size_t _block_size,
						size_t _blocks_per_page = 1024) {
		m_blocks_per_page = _blocks_per_page;
		m_block_size = adjust_block_size(_block_size);
		free_list = nullptr;
	}

	/// <summary>
	/// 析构函数
	/// </summary>
	~FixedSizeMemoryPool() {
		for (void* p : pages) {
			operator delete[](p);
		}
	}

	/// <summary>
	/// 分配内存
	/// </summary>
	/// <returns></returns>
	void* allocate(){
		if (!free_list) {
			expand();
		}

		Node* head = free_list;
		free_list = head->next;

		return head;
	}
	
	/// <summary>
	/// 内存回归
	/// </summary>
	/// <param name=""></param>
	void deallocate(void* p){
		if (!p)return;
		Node* node = static_cast<Node*>(p);
		node->next = free_list;
		free_list = node;
		return;
	}

	size_t block_size()const { return m_block_size; };
	size_t block_per_page()const { return m_blocks_per_page; };
private:
	size_t adjust_block_size(size_t s) {
		size_t min = sizeof(void*);
		size_t a = align_up(s < min ? min : s,alignof(void*));
		return a;
	}

	/// <summary>
	/// 向系统申请一页内存
	/// </summary>
	void expand() {
		// 申请一整页内存
		size_t page_bytes = m_block_size * m_blocks_per_page;
		char* page = static_cast<char*>(operator new[](page_bytes));
		pages.push_back(page);

		//把整页内存进行分块
		for (size_t i = 0; i < m_blocks_per_page; i++) {
			char* addr = page + i * m_block_size;
			Node* node = reinterpret_cast<Node*>(addr);
			node->next = free_list;
			free_list = node;
		}
	};
	struct  Node { Node* next; };
	size_t m_block_size = 0;
	size_t m_blocks_per_page = 0;

	Node* free_list;

	std::vector<void*> pages;

};
