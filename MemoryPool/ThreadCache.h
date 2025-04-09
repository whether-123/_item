#ifndef _THREAD
#define _THREAD

#include"Common.h"

class ThreadCache
{
public:
	//�����ڴ����
	void* Allocate(size_t size);
    //�ͷ��ڴ����
	void Deallocate(void* ptr, size_t size);

	// �����Ļ����ȡ����(����ϣͰ�ж�Ӧ���ڴ治��ʱ)
	void* FetchFromCentralCache(size_t index, size_t size);
	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeLists[NFREE_LIST];
};

//��̬TLS�������� _declspec(thread)�ı�������Ϊÿһ���̴߳���һ�������Ŀ���
//�������ܱ�֤��ÿ���̶߳���һ��ThreadCache����������ThreadCahe�Ͳ��ÿ����������������������
#ifdef _WIN32
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
#else
static __thread ThreadCache* pTLSThreadCache = nullptr;
#endif

#endif