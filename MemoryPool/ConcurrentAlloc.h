#ifndef _CURRENT
#define _CURRENT

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static ObjectPool<ThreadCache> tcPool;
static std::mutex mux;

static void* ConcurrentAlloc(size_t size)
{
	//进程申请的内存大于256KB时
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = alignSize;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		//通过TLS 每个线程无锁的获取自己的专属的ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			//每个线程从定长内存池中申请ThreadCache对象
			mux.lock();
			pTLSThreadCache = tcPool.New();
			mux.unlock();
		}
		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize; //每个span中切出的小块内存 都是一样大

	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
#endif