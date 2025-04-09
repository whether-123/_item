#include "ConcurrentAlloc.h"

static std::mutex mux1;
static std::mutex mux2;
//测试性能函数
//ntimes 申请和释放内存的次数  nworks 线程数  rounds 轮数
void TestConcurrentAlloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t ConcurrentAlloc_totaltime = 0;
	size_t ConcurrentFree_totaltime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc((7 + i) % (1024*128) + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				mux2.lock();
				ConcurrentAlloc_totaltime += (end1 - begin1);
				ConcurrentFree_totaltime += (end2 - begin2);
				mux2.unlock();
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%u个线程并发执行%u轮次，每轮次concurrent_alloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, ConcurrentAlloc_totaltime);

	printf("%u个线程并发执行%u轮次，每轮次concurrent_free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, ConcurrentFree_totaltime);

	printf("%u个线程并发concurrent_alloc&free %u次，总计花费：%u ms\n",
		nworks, nworks * rounds * ntimes, ConcurrentAlloc_totaltime + ConcurrentFree_totaltime);
}

void TestMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_totaltime = 0;
	size_t free_totaltime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc((7 + i) % (1024*128) + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				mux1.lock();
				malloc_totaltime += (end1 - begin1);
				free_totaltime += (end2 - begin2);
				mux1.unlock();
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_totaltime);

	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_totaltime);

	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks * rounds * ntimes, malloc_totaltime + free_totaltime);
}

int main()
{
	size_t n = 3000;
	std::cout << "Memory_Pool" << std::endl;
	//申请和释放内存的次数--线程数--轮数
	TestConcurrentAlloc(n, 30, 10);
	std::cout << std::endl << std::endl;

	std::cout << "Malloc" << std::endl;
	//申请和释放内存的次数--线程数--轮数
	TestMalloc(n, 30, 10);
	std::cout << std::endl << std::endl;

	return 0;
}