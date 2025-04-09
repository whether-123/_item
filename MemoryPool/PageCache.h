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
	//��ȡ���ڴ�鵽span��ӳ��
	Span* MapObjectToSpan(void* obj);

	//�����Ļ����ͷſ���span��PageCache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

	//��ȡһ��Kҳ��span
	Span* NewSpan(size_t k);

    //���ﲻ�����ó�Ͱ�� �����һ��λ��û�� 
	//����������ң����Ƶ���ļ�������
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
