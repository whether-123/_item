#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个K页的span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	//大于128页时，直接找堆申请
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();

		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;

		//建立起始页号与span的映射关系
		_idSpanMap.set(span->_pageId, span);

		return span;
	}

	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();
		//建立id(页号)和span的映射，方便CentralCache回收内存时，查找对应的页号
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			//将k页大小的span 中的每个页都与span建立映射关系
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}

	//检查一下后面的桶里面有没有span，如果有可以把它进行切分
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = _spanPool.New();

			//在nSpan的头部切k页下来，k页span返回
			//剩余的 n-k 挂到pageCache映射的位置中
			kSpan->_pageId = nSpan->_pageId; //记录起始页号
			kSpan->_n = k; //记录页的数量

			nSpan->_pageId += k;
			nSpan->_n -= k;

			_spanLists[nSpan->_n].PushFront(nSpan);
			//没有分配给CentralCache的span，仅存储nSpan的首尾页号和nSpan的映射
			//方便PageCache回收内存时，进行合并查找前后的页
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

			//只有分配给CentralCache的span才需要每一页都建立与span的映射关系
			//建立id(页号)和span的映射，方便CentralCache回收内存时 查找对应的页号
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				//将k页大小的span 中的每个页都与span建立映射关系
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}
			return kSpan;
		}
	}
	//走到这个位置就说明后面没有大页的span了
	//这时就去找堆要一个128页的span
	Span* bigSpan = _spanPool.New();
	
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);
}

// 找到这个地址，对应的页号属于哪个span，并返回对应的span
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;

	auto ret=(Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

// 释放空闲span回到Pagecache，并合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//大于128页时，直接还给堆
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		_idSpanMap.unset(span->_pageId);
		SystemFree(ptr,span->_n);
		_spanPool.Delete(span);

		return;
	}

	//对span前后的页，尝试进行合并，缓解外部碎片问题
	//合并时，通过哈希表去找前后的页
	//向前合并
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		
		//前面的页号没有，不合并
		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)
		{
			break;
		}
		
		//前面相邻页的span在使用，不合并
		Span* prevSpan = ret; 
		if (prevSpan->_isUse == true)
		{
			break;
		}

		//合并出超过128页的span没办法管理，不合并
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);

		_spanPool.Delete(prevSpan);
	}

	//向后合并
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
