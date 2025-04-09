#ifndef _PAGEMAP
#define _PAGEMAP

#include "Common.h"

//两层基数树
template <int BITS>
class TCMalloc_PageMap2 
{
public:
	typedef uintptr_t Number;
	
	explicit TCMalloc_PageMap2() 
	{
		memset(root_, 0, sizeof(root_));
		PreallocateMoreMemory();
	}

	void* get(Number k) const 
	{
		const Number i1 = k >> LEAF_BITS; //第一层编号
		const Number i2 = k & (LEAF_LENGTH - 1); //第二层编号
		if ((k >> BITS) > 0 || root_[i1] == NULL)
		{
			return NULL;
		}

		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) 
	{
		const Number i1 = k >> LEAF_BITS; //获取高5位
		const Number i2 = k & (LEAF_LENGTH - 1); //获取低14位
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}
	
	void unset(Number k)
	{
		const Number i1 = k >> LEAF_BITS; //获取高5位
		const Number i2 = k & (LEAF_LENGTH - 1); //获取低14位
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = nullptr;
	}

	bool Ensure(Number start, size_t n) 
	{
		for (Number key = start; key <= start + n - 1;)
		{
			const Number i1 = key >> LEAF_BITS;
			//检查是否溢出
			if (i1 >= ROOT_LENGTH)
				return false;
			//第二层的节点为空，分配内存
			if (root_[i1] == NULL) 
			{
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = reinterpret_cast<Leaf*>(leafPool.New());

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}
			//下一个叶节点
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void PreallocateMoreMemory() 
	{
		//一次性开好所有的内存
		Ensure(0, 1 << BITS);
	}

private:

	static const int ROOT_BITS = 5; //第一层位数
	static const int ROOT_LENGTH = 1 << ROOT_BITS; //第一层表示多少个元素
	static const int LEAF_BITS = BITS - ROOT_BITS; //第二层位数
	static const int LEAF_LENGTH = 1 << LEAF_BITS; //第二层表示多少个元素
	
	struct Leaf //叶节点
	{
		void* values[LEAF_LENGTH];//指针数组用于存地址
	};

	Leaf* root_[ROOT_LENGTH]; //第一层的
};

// 三层基数树
template <int BITS>
class TCMalloc_PageMap3 
{
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap3() 
	{
		root_ = NewNode();
	}
	//获取一个节点
	void* get(Number k) const 
	{
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 ||root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL) 
		{
			return NULL;
		}
		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3];
	}
	//设置一个节点
	void set(Number k, void* v) 
	{
		assert(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		PreallocateMoreMemory(k);

		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
	}
	//取消一个节点
	void unset(Number k)
	{
		assert(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);

		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = nullptr;
	}

	bool Ensure(Number start, size_t n) 
	{
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
			//检查是否溢出
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
				return false;

			//第二层的节点为空，分配内存
			if (root_->ptrs[i1] == NULL) 
			{
				Node* n = NewNode();

				if (n == NULL) return false;
				root_->ptrs[i1] = n;
			}

			//叶节点为空，分配内存
			if (root_->ptrs[i1]->ptrs[i2] == NULL) 
			{
				static ObjectPool<Leaf> LeafPool;
				Leaf* leaf = reinterpret_cast<Leaf*>(LeafPool.New());

				if (leaf == NULL) return false;
				memset(leaf, 0, sizeof(*leaf));
				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
			}
			
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //下一个叶节点
		}
		return true;
	}

	void PreallocateMoreMemory(Number k)
	{
		//一次开8个叶节点
		Ensure(k, 8 << LEAF_BITS);
	}

private:
	
	static const int INTERIOR_BITS = (BITS + 2) / 3; //第一层 位数(第二层和第一层一样)
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS; //第三层 位数
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	struct Node 
	{
		Node* ptrs[INTERIOR_LENGTH];//第二层
	};
	//叶节点
	struct Leaf 
	{
		void* values[LEAF_LENGTH]; //第三层
	};

	Node* root_;
	
	Node* NewNode() 
	{
		static ObjectPool<Node> NodePool;
		Node* result = reinterpret_cast<Node*>(NodePool.New());
		if (result != NULL)
		{
			memset(result, 0, sizeof(*result));
		}
		return result;
	}
};
#endif
