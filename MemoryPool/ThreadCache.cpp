#include"ThreadCache.h"
#include"CentralCache.h"

size_t Min(size_t x, size_t y)
{
	if (x > y)
	{
		return y;
	}
	return x;
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 1.��һ��������CentralCacheҪ1��size��С���ڴ��
	// 2.����ÿ������size��С�ڴ�飬batchNum������ϴζ�1��(������һ��Լ������������ȡ��Сֵ)
	// 3.����ʱsizeԽ��һ����centralCacheҪ��batchNum��ԽС
	// 4.����ʱsizeԽС��һ����centralCacheҪ��batchNum��Խ��
	size_t batchNum = Min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchNum)
	{
		++_freeLists[index].MaxSize();
	}

	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);

	if (actualNum == 1)
	{
		assert(start == end);
		NextObj(start) = nullptr;
		return start;
	}
	else
	{
		//����һ�����ظ��̣߳�����Ĺҵ�������
		_freeLists[index].PushRange(NextObj(start), end,actualNum-1);
		NextObj(start) = nullptr;
		return start;
	}

	return nullptr;
}

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
	else
	{
		//��������������û��ʱ��ȥ��CentralCache��ȡ
		return  FetchFromCentralCache(index, alignSize);
	}
}

void  ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size <= MAX_BYTES);
	assert(ptr);

	//�ҳ�ӳ�����������Ͱ�����黹���ڴ�����ȥ
	size_t index = SizeClass::Index(size);
	size_t alignSize = SizeClass::RoundUp(size);
	_freeLists[index].Push(ptr);

	//��ĳ��ӳ���λ�����棬�ҵ��ڴ��̫���ˣ�Ҫ����CentralCache
	//���������������¹ҵ��ڴ������������ϴ���������ģ���һ��list��CentralCache
	if (_freeLists[index].Size() > _freeLists[index].MaxSize())
	{
	   ListTooLong(_freeLists[index], alignSize);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}