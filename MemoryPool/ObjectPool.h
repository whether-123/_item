#ifndef _OBJECT
#define _OBJECT

#include "Common.h"

//���������
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//���Ȱѻ�����С���ڴ棬�����ȥ
		if (_freeList)
		{
			void* next = NextObj(_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			//ʣ���ڴ治��һ�������Сʱ��ȥ�������ڴ棬���з�
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
			
			//�����з�һ��ָ���С���ڴ�� ���㻹����ʱ����
			size_t  objSize= sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		//��λnew�����ڴ��Ѿ����������£���ʾ����T�Ĺ��캯����ʼ��
		new(obj) T;
		return obj;
	}

	void Delete(T* obj)
	{
		//�ͷŶ�������������ڴ�
		obj->~T();
		//��������С���ڴ棬ͷ����������
		NextObj(obj) = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr; //ָ���������ڴ��ָ��
	size_t  _remainBytes = 0; //��������ڴ�飬���зֹ�����ʣ����ֽ���

	void* _freeList = nullptr; //С���ڴ滹����ʱ�������������
};
#endif