# MemoryPool


简介：基于池化技术的思想，实现了三层结构的内存池，在多线程同时竞争申请内存的情况下，其效率比malloc高15~20%


知识储备：C/C++、链表、哈希桶、基数树、单例模式、多线程、互斥锁、操作系统内存管理。


开发环境：VS 2019、Windows10


功能：内存池的结构有三层，分别是ThreadCache，CentralCache，PageCache。
![image](https://github.com/user-attachments/assets/d95326d3-7696-4413-850d-a7fee9d473b2)


ThreadCache: 线程缓存(无锁)，为了减少锁竞争带来的性能损耗，将其设计为每个线程独有的全区变量(线程局部存储)。

不同区间的长度，按照不同的对齐规则，结构利用哈希桶，相同大小的内存块，按映射位置链接在一起(单链表)。
![image](https://github.com/user-attachments/assets/76175628-f833-4169-b77f-9bc75b6a8467)


CentralCache：中心缓存(设置的是桶锁)，所有线程共享一个，哈希桶结构，区间映射规则和ThreadCache相同，但是挂的不是一个个小块内存，

挂的是从PageCache中申请而来的页为单位的内存块(用一种结构存储起来页号和页数)(双向带头链表)，然后内部再切割成等长的小块内存(单链表)。
![image](https://github.com/user-attachments/assets/7b080b28-340f-4756-b0d0-8f9335e186fc)


PageCache：页缓存(一个锁)，记录[1,128]页，线程申请的内存大于256KB时，直接找PageCache，小于等于2565KB，找CentralCache
![image](https://github.com/user-attachments/assets/3afffc24-8b7e-4a06-bc31-0b5499a9720f)












