#ifndef _CENTRAL
#define _CENTRAL

#include "Common.h"

//单例模式(饿汉模式)
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	// 从中心缓存获取一定数量的对象给ThreadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);
	// 获取一个非空的span 
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	SpanList _spanLists[NFREE_LIST];

	CentralCache(){}
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInst;
};
#endif
