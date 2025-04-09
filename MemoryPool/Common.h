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

//ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	//lpAddress������ΪNULL��ϵͳ�Լ����������ڴ������λ�ã����Ұ�64KB����ȡ����
	//ֻҪ������64KB�����ܾ�ȷ�����Ӧ�ڴ�飬���ڵ�ҳ��(��ҳ�����ڼ�¼�ڴ�飬�����зֺ͹黹) 
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

//ȫ�ֺ�������Ϊ��� .h�ļ��ڶ�� .cpp�а���
//ÿ��cpp����������һ�������ĺ���������ʱ�ͻ��ж�ݣ�������ͬ�ͻ��ͻ
//����취����static����֤��ֻ�ڵ�ǰ�ļ��ɼ��������Ͳ�������������Ե�����
static void*& NextObj(void* obj)
{
	//��ָ��洢�ĵ�ַ����Ӧ�ռ��ǰ4/8���ֽڣ����洢��һ��ָ��ĵ�ַ	
	return *((void**)obj);
}

//�����зֺ�С���ڴ����������
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

	//ͷ����������С���ڴ�
	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	//ͷɾ���������С���ڴ�
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
	size_t _maxSize = 1; //�������������ڴ�
	size_t _size = 0; //��¼�����ж��ٸ�С���ڴ�
};

//�������ӳ�����
class  SizeClass
{
public:
	// ������ƣ��˷ѵ��ڲ���Ƭ��10%����
	// [1,128]                 8byte����           freelist[0, 16)
	// [128+1,1024]            16byte����          freelist[16, 72)
	// [1024+1,8*1024]         128byte����         freelist[72, 128)
	// [8*1024+1,64*1024]      1024byte����        freelist[128, 184)
	// [64*1024+1,256*1024]    8 * 1024byte����    freelist[184, 208)

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

	//���չ涨�Ķ����������������ڴ���С
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
			//����256KB����ҳΪ��λ����
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	static inline size_t _Index(size_t bytes, size_t alignNum)
	{
		if (bytes % alignNum == 0)
		{
			//�����0��ʼ����1
			return bytes / alignNum - 1;
		}
		else
		{
			return bytes / alignNum;
		}
	}

	//�����ڴ���С�����ӳ�䵽��ϣͰ���±�
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// ÿ�����䷶Χ������
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

	// ThreadCacheÿ�δ����Ļ����ȡ���ٸ�
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;

		// [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
	   // С����һ���������޸ߣ������һ���������޵�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// ����һ����ϵͳ��ȡ����ҳ
	// �������� 8byte ...  �������� 256KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT;
		//������ϵͳ��ȡһҳ
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

//����������ҳ�Ĵ���ڴ��Ƚṹ
struct Span
{
	PAGE_ID  _pageId = 0; //����ڴ���ʼҳ��ҳ��
	size_t _n = 0;  //ҳ������

	Span* _next = nullptr;  //˫������ṹ����������ɾ��
	Span* _prev = nullptr;

	size_t _objSize = 0; //�кõ�С���ڴ��С
	size_t _useCount = 0; //��¼�������ThreadCache���ٸ�С���ڴ�
	void* _freeList = nullptr;  //����к�С���ڴ����������

	bool  _isUse = false;     //�Ƿ��ڱ�ʹ��
};

//��ͷ˫��ѭ������
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

	//ֻ�ǽ���ȡ��������û��delete��
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
	std::mutex _mtx;   //Ͱ��
};
#endif