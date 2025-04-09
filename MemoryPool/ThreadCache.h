#ifndef _THREAD
#define _THREAD

#include"Common.h"

class ThreadCache
{
public:
	//申请内存对象
	void* Allocate(size_t size);
    //释放内存对象
	void Deallocate(void* ptr, size_t size);

	// 从中心缓存获取对象(当哈希桶中对应的内存不够时)
	void* FetchFromCentralCache(size_t index, size_t size);
	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeLists[NFREE_LIST];
};

//静态TLS，声明了 _declspec(thread)的变量，会为每一个线程创建一个单独的拷贝
//这样就能保证，每个线程独享一个ThreadCache，这样访问ThreadCahe就不用考虑锁竞争带来的性能损耗
#ifdef _WIN32
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
#else
static __thread ThreadCache* pTLSThreadCache = nullptr;
#endif

#endif