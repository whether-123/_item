#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// 获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//查看当前的spanlist中有没有还未分配的span对象
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
	//先把CentralCache的桶锁解掉，这样如果其他线程释放内存对象回来，不会被阻塞
	list._mtx.unlock();

	//走到这里说明没有空闲span，只能找pageCache要
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;  //说明该sapn已经被分配给centralCache
	span->_objSize = size; //记录该span 要切分的小块内存大小
	PageCache::GetInstance()->_pageMtx.unlock();
	
	//对获取的span进行切分，不需要加锁，因为其他线程访问不到这个span
	//计算span的大块内存的起始地址和大块内存的大小(字节数)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	//把大块内存切成小块内存用自由链表链接起来
	//先切一块下来，方便尾插
	span->_freeList = start;
	//这里的size 已经是对齐后的字节数
	start += size;
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		NextObj(tail) = nullptr;
		start += size;
	}
	//内存块用了之后会回收 然后在被分配给线程调用
	//有可能会出现之前 被用过 然后回收 在被分配
	NextObj(tail) = nullptr;

	//切好后，把span挂到桶里面时，在加锁
	list._mtx.lock();
	list.PushFront(span);

	return span;
}

// 从中心缓存获取一定数量的对象给ThreadCache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	//每个映射的链表单独有一个锁
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	//从span中获取batchNum个内存块
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	//如果不够 将span中挂的内存块全部给出
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}

	span->_freeList = NextObj(end);
	//将拆下来的最后一个节点的next置为空
	NextObj(end) = nullptr;
	//每次从span中拿走了多少个 记录下来
	span->_useCount += actualNum;

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

// 将一定数量的对象释放到span中
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	//start 连接着批量的小块内存
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		//根据地址 找到属于哪个span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		//此时说明span切分出去的所有小块内存都回来了
		//这个span就可以再回收给PageCache，PageCache可以尝试去做前后页的合并
		if (span->_useCount == 0)
		{
			//将span从CentralCache中，解除下来
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//解掉桶锁，此时要进入PageCache 当回来时在加锁 
			_spanLists[index]._mtx.unlock();

			//还给pageCache
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		start = next;
	}
	_spanLists[index]._mtx.unlock();
}