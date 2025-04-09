#ifndef _PAGE
#define _PAGE

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	//获取从内存块到span的映射
	Span* MapObjectToSpan(void* obj);

	//从中心缓存释放空闲span到PageCache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

	//获取一个K页的span
	Span* NewSpan(size_t k);

    //这里不能设置成桶锁 如果第一个位置没有 
	//会继续往后找，造成频繁的加锁解锁
    std::mutex _pageMtx;
private:
	ObjectPool<Span> _spanPool;
	SpanList  _spanLists[NPAGES];

#ifdef _WIN64
#include<windows.h>
	TCMalloc_PageMap3<64 - PAGE_SHIFT> _idSpanMap;
#elif _WIN32
#include<windows.h>
	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap;
#else
	TCMalloc_PageMap3<64 - PAGE_SHIFT> _idSpanMap;
#endif  

	PageCache(){}
	PageCache(const PageCache&) = delete;
	static PageCache _sInst;
};
#endif
