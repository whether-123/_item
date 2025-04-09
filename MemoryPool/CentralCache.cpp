#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// ��ȡһ���ǿյ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//�鿴��ǰ��spanlist����û�л�δ�����span����
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}
	//�Ȱ�CentralCache��Ͱ�������������������߳��ͷ��ڴ������������ᱻ����
	list._mtx.unlock();

	//�ߵ�����˵��û�п���span��ֻ����pageCacheҪ
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;  //˵����sapn�Ѿ��������centralCache
	span->_objSize = size; //��¼��span Ҫ�зֵ�С���ڴ��С
	PageCache::GetInstance()->_pageMtx.unlock();
	
	//�Ի�ȡ��span�����з֣�����Ҫ��������Ϊ�����̷߳��ʲ������span
	//����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С(�ֽ���)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	//�Ѵ���ڴ��г�С���ڴ�������������������
	//����һ������������β��
	span->_freeList = start;
	//�����size �Ѿ��Ƕ������ֽ���
	start += size;
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		NextObj(tail) = nullptr;
		start += size;
	}
	//�ڴ������֮������ Ȼ���ڱ�������̵߳���
	//�п��ܻ����֮ǰ ���ù� Ȼ����� �ڱ�����
	NextObj(tail) = nullptr;

	//�кú󣬰�span�ҵ�Ͱ����ʱ���ڼ���
	list._mtx.lock();
	list.PushFront(span);

	return span;
}

// �����Ļ����ȡһ�������Ķ����ThreadCache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	//ÿ��ӳ�����������һ����
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	//��span�л�ȡbatchNum���ڴ��
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	//������� ��span�йҵ��ڴ��ȫ������
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}

	span->_freeList = NextObj(end);
	//�������������һ���ڵ��next��Ϊ��
	NextObj(end) = nullptr;
	//ÿ�δ�span�������˶��ٸ� ��¼����
	span->_useCount += actualNum;

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

// ��һ�������Ķ����ͷŵ�span��
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	//start ������������С���ڴ�
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		//���ݵ�ַ �ҵ������ĸ�span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		//��ʱ˵��span�зֳ�ȥ������С���ڴ涼������
		//���span�Ϳ����ٻ��ո�PageCache��PageCache���Գ���ȥ��ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0)
		{
			//��span��CentralCache�У��������
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//���Ͱ������ʱҪ����PageCache ������ʱ�ڼ��� 
			_spanLists[index]._mtx.unlock();

			//����pageCache
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		start = next;
	}
	_spanLists[index]._mtx.unlock();
}