#ifndef _COMMON
#define _COMMON

#include<iostream>
#include<vector>
#include<algorithm>
#include<mutex>
#include<thread>
#include<ctime>
#include<cassert>
#include<cstring>

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREE_LIST = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 12;

#ifdef _WIN64
#include<windows.h>
typedef  unsigned long long  PAGE_ID;
static const size_t OBJECTLEN = 16 * 1024 * 1024;
#elif _WIN32
#include<windows.h>
typedef  size_t  PAGE_ID;
static const size_t OBJECTLEN = 512 * 1024;
#else
#include <sys/mman.h>
#include <unistd.h>
typedef  unsigned long long  PAGE_ID;
static const size_t OBJECTLEN = 16 * 1024 * 1024;
#endif  

//直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	//lpAddress参数设为NULL，系统自己决定分配内存区域的位置，并且按64KB向上取整。
	//只要不大于64KB，都能精确算出对应内存块，所在的页号(此页号用于记录内存块，方便切分和归还) 
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}
#else
	void* ptr = nullptr;
	if (kpage <= 128)
	{
		ptr = sbrk(kpage * (1 << PAGE_SHIFT));
	}
	else
	{
		ptr = mmap(NULL, kpage * (1 << PAGE_SHIFT), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	}

	if (ptr == (void*)(-1))
	{
		throw std::bad_alloc();
	}
#endif
	return ptr;
	}

inline static void SystemFree(void* ptr, size_t kpage)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	munmap(ptr, kpage * (1 << PAGE_SHIFT));
#endif
}

//全局函数，因为这个 .h文件在多个 .cpp中包含
//每个cpp，都会生成一个这样的函数，链接时就会有多份，名字相同就会冲突
//解决办法，加static，保证它只在当前文件可见，这样就不会存在链接属性的问题
static void*& NextObj(void* obj)
{
	//用指针存储的地址，对应空间的前4/8个字节，来存储下一个指向的地址	
	return *((void**)obj);
}

//管理切分好小块内存的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);

		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	//头插多个连续的小块内存
	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	//头删多个连续的小块内存
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = start;

		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}

		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	void* Pop()
	{
		assert(_freeList);

		void* obj = _freeList;
		_freeList = NextObj(obj);
		NextObj(obj) = nullptr;
		--_size;

		return obj;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1; //线性增长分配内存
	size_t _size = 0; //记录表中有多少个小块内存
};

//计算对齐映射规则
class  SizeClass
{
public:
	// 整体控制，浪费的内部碎片在10%左右
	// [1,128]                 8byte对齐           freelist[0, 16)
	// [128+1,1024]            16byte对齐          freelist[16, 72)
	// [1024+1,8*1024]         128byte对齐         freelist[72, 128)
	// [8*1024+1,64*1024]      1024byte对齐        freelist[128, 184)
	// [64*1024+1,256*1024]    8 * 1024byte对齐    freelist[184, 208)

	static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		size_t alignSize;
		if (size % alignNum != 0)
		{
			alignSize = (size / alignNum + 1) * alignNum;
		}
		else
		{
			alignSize = size;
		}

		return alignSize;
	}

	//按照规定的对齐数，算出分配的内存块大小
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);

		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			//大于256KB，以页为单位对齐
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	static inline size_t _Index(size_t bytes, size_t alignNum)
	{
		if (bytes % alignNum == 0)
		{
			//数组从0开始，减1
			return bytes / alignNum - 1;
		}
		else
		{
			return bytes / alignNum;
		}
	}

	//按照内存块大小，算出映射到哈希桶的下标
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// 每个区间范围的数量
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128)
		{
			return _Index(bytes, 8);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 16) + group_array[0];
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 128) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 1024) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (bytes <= 256 * 1024)
		{
			return _Index(bytes - 64 * 1024, 8 * 1024) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	// ThreadCache每次从中心缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;

		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
	   // 小对象一次批量上限高，大对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// 计算一次向系统获取几个页
	// 单个对象 8byte ...  单个对象 256KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT;
		//最少向系统获取一页
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

//管理多个连续页的大块内存跨度结构
struct Span
{
	PAGE_ID  _pageId = 0; //大块内存起始页的页号
	size_t _n = 0;  //页的数量

	Span* _next = nullptr;  //双向链表结构，方便插入和删除
	Span* _prev = nullptr;

	size_t _objSize = 0; //切好的小块内存大小
	size_t _useCount = 0; //记录分配给了ThreadCache多少个小块内存
	void* _freeList = nullptr;  //存放切好小块内存的自由链表

	bool  _isUse = false;     //是否在被使用
};

//带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		//prev  newspan  pos
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		pos->_prev = newSpan;
		newSpan->_next = pos;
	}

	//只是将其取下来，并没有delete掉
	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
		pos->_prev = nullptr;
		pos->_next = nullptr;
	}

private:
	Span* _head = nullptr;
public:
	std::mutex _mtx;   //桶锁
};
#endif