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
	//����������ڴ����256KBʱ
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
		//ͨ��TLS ÿ���߳������Ļ�ȡ�Լ���ר����ThreadCache����
		if (pTLSThreadCache == nullptr)
		{
			//ÿ���̴߳Ӷ����ڴ��������ThreadCache����
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
	size_t size = span->_objSize; //ÿ��span���г���С���ڴ� ����һ����

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