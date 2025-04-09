#ifndef _CENTRAL
#define _CENTRAL

#include "Common.h"

//����ģʽ(����ģʽ)
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	// �����Ļ����ȡһ�������Ķ����ThreadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);
	// ��ȡһ���ǿյ�span 
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	SpanList _spanLists[NFREE_LIST];

	CentralCache(){}
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInst;
};
#endif
