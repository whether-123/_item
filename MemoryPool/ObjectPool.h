#ifndef _OBJECT
#define _OBJECT

#include "Common.h"

//定长对象池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//优先把还回来小块内存，分配出去
		if (_freeList)
		{
			void* next = NextObj(_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			//剩余内存不够一个对象大小时，去堆申请内存，再切分
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = OBJECTLEN;

				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);
				if (_memory == nullptr)
				{
					throw  std::bad_alloc();
				} 
			}

			obj = (T*)_memory;
			
			//最少切分一个指针大小的内存块 方便还回来时链接
			size_t  objSize= sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		//定位new，在内存已经分配的情况下，显示调用T的构造函数初始化
		new(obj) T;
		return obj;
	}

	void Delete(T* obj)
	{
		//释放对象里面申请的内存
		obj->~T();
		//还回来的小块内存，头插入链表中
		NextObj(obj) = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr; //指向堆申请的内存块指针
	size_t  _remainBytes = 0; //堆申请的内存块，在切分过程中剩余的字节数

	void* _freeList = nullptr; //小块内存还回来时，挂在链表后面
};
#endif