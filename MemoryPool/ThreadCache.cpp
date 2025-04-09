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
	// 1.第一次申请向CentralCache要1个size大小的内存块
	// 2.后续每次申请size大小内存块，batchNum都会比上次多1个(还有另一个约束条件，两者取最小值)
	// 3.申请时size越大，一次向centralCache要的batchNum就越小
	// 4.申请时size越小，一次向centralCache要的batchNum就越大
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
		//将第一个返回给线程，后面的挂到链表中
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
		//当自由链表里面没有时，去找CentralCache获取
		return  FetchFromCentralCache(index, alignSize);
	}
}

void  ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size <= MAX_BYTES);
	assert(ptr);

	//找出映射的自由链表桶，将归还的内存插入进去
	size_t index = SizeClass::Index(size);
	size_t alignSize = SizeClass::RoundUp(size);
	_freeLists[index].Push(ptr);

	//当某个映射的位置下面，挂的内存块太多了，要还给CentralCache
	//当还回来后，链表下挂的内存数量超过了上次批量申请的，还一段list给CentralCache
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