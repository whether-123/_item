#include "PageCache.h"

PageCache PageCache::_sInst;

//��ȡһ��Kҳ��span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	//����128ҳʱ��ֱ���Ҷ�����
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();

		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;

		//������ʼҳ����span��ӳ���ϵ
		_idSpanMap.set(span->_pageId, span);

		return span;
	}

	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();
		//����id(ҳ��)��span��ӳ�䣬����CentralCache�����ڴ�ʱ�����Ҷ�Ӧ��ҳ��
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			//��kҳ��С��span �е�ÿ��ҳ����span����ӳ���ϵ
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}

	//���һ�º����Ͱ������û��span������п��԰��������з�
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = _spanPool.New();

			//��nSpan��ͷ����kҳ������kҳspan����
			//ʣ��� n-k �ҵ�pageCacheӳ���λ����
			kSpan->_pageId = nSpan->_pageId; //��¼��ʼҳ��
			kSpan->_n = k; //��¼ҳ������

			nSpan->_pageId += k;
			nSpan->_n -= k;

			_spanLists[nSpan->_n].PushFront(nSpan);
			//û�з����CentralCache��span�����洢nSpan����βҳ�ź�nSpan��ӳ��
			//����PageCache�����ڴ�ʱ�����кϲ�����ǰ���ҳ
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

			//ֻ�з����CentralCache��span����Ҫÿһҳ��������span��ӳ���ϵ
			//����id(ҳ��)��span��ӳ�䣬����CentralCache�����ڴ�ʱ ���Ҷ�Ӧ��ҳ��
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				//��kҳ��С��span �е�ÿ��ҳ����span����ӳ���ϵ
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}
			return kSpan;
		}
	}
	//�ߵ����λ�þ�˵������û�д�ҳ��span��
	//��ʱ��ȥ�Ҷ�Ҫһ��128ҳ��span
	Span* bigSpan = _spanPool.New();
	
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);
}

// �ҵ������ַ����Ӧ��ҳ�������ĸ�span�������ض�Ӧ��span
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;

	auto ret=(Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//����128ҳʱ��ֱ�ӻ�����
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		_idSpanMap.unset(span->_pageId);
		SystemFree(ptr,span->_n);
		_spanPool.Delete(span);

		return;
	}

	//��spanǰ���ҳ�����Խ��кϲ��������ⲿ��Ƭ����
	//�ϲ�ʱ��ͨ����ϣ��ȥ��ǰ���ҳ
	//��ǰ�ϲ�
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		
		//ǰ���ҳ��û�У����ϲ�
		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)
		{
			break;
		}
		
		//ǰ������ҳ��span��ʹ�ã����ϲ�
		Span* prevSpan = ret; 
		if (prevSpan->_isUse == true)
		{
			break;
		}

		//�ϲ�������128ҳ��spanû�취�������ϲ�
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);

		_spanPool.Delete(prevSpan);
	}

	//���ϲ�
	while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		
		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}

		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		_spanPool.Delete(nextSpan);
	}

	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;

	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}
