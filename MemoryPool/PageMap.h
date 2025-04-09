#ifndef _PAGEMAP
#define _PAGEMAP

#include "Common.h"

//���������
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
		const Number i1 = k >> LEAF_BITS; //��һ����
		const Number i2 = k & (LEAF_LENGTH - 1); //�ڶ�����
		if ((k >> BITS) > 0 || root_[i1] == NULL)
		{
			return NULL;
		}

		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) 
	{
		const Number i1 = k >> LEAF_BITS; //��ȡ��5λ
		const Number i2 = k & (LEAF_LENGTH - 1); //��ȡ��14λ
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}
	
	void unset(Number k)
	{
		const Number i1 = k >> LEAF_BITS; //��ȡ��5λ
		const Number i2 = k & (LEAF_LENGTH - 1); //��ȡ��14λ
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = nullptr;
	}

	bool Ensure(Number start, size_t n) 
	{
		for (Number key = start; key <= start + n - 1;)
		{
			const Number i1 = key >> LEAF_BITS;
			//����Ƿ����
			if (i1 >= ROOT_LENGTH)
				return false;
			//�ڶ���Ľڵ�Ϊ�գ������ڴ�
			if (root_[i1] == NULL) 
			{
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = reinterpret_cast<Leaf*>(leafPool.New());

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}
			//��һ��Ҷ�ڵ�
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void PreallocateMoreMemory() 
	{
		//һ���Կ������е��ڴ�
		Ensure(0, 1 << BITS);
	}

private:

	static const int ROOT_BITS = 5; //��һ��λ��
	static const int ROOT_LENGTH = 1 << ROOT_BITS; //��һ���ʾ���ٸ�Ԫ��
	static const int LEAF_BITS = BITS - ROOT_BITS; //�ڶ���λ��
	static const int LEAF_LENGTH = 1 << LEAF_BITS; //�ڶ����ʾ���ٸ�Ԫ��
	
	struct Leaf //Ҷ�ڵ�
	{
		void* values[LEAF_LENGTH];//ָ���������ڴ��ַ
	};

	Leaf* root_[ROOT_LENGTH]; //��һ���
};

// ���������
template <int BITS>
class TCMalloc_PageMap3 
{
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap3() 
	{
		root_ = NewNode();
	}
	//��ȡһ���ڵ�
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
	//����һ���ڵ�
	void set(Number k, void* v) 
	{
		assert(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		PreallocateMoreMemory(k);

		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
	}
	//ȡ��һ���ڵ�
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
			//����Ƿ����
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
				return false;

			//�ڶ���Ľڵ�Ϊ�գ������ڴ�
			if (root_->ptrs[i1] == NULL) 
			{
				Node* n = NewNode();

				if (n == NULL) return false;
				root_->ptrs[i1] = n;
			}

			//Ҷ�ڵ�Ϊ�գ������ڴ�
			if (root_->ptrs[i1]->ptrs[i2] == NULL) 
			{
				static ObjectPool<Leaf> LeafPool;
				Leaf* leaf = reinterpret_cast<Leaf*>(LeafPool.New());

				if (leaf == NULL) return false;
				memset(leaf, 0, sizeof(*leaf));
				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
			}
			
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //��һ��Ҷ�ڵ�
		}
		return true;
	}

	void PreallocateMoreMemory(Number k)
	{
		//һ�ο�8��Ҷ�ڵ�
		Ensure(k, 8 << LEAF_BITS);
	}

private:
	
	static const int INTERIOR_BITS = (BITS + 2) / 3; //��һ�� λ��(�ڶ���͵�һ��һ��)
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS; //������ λ��
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	struct Node 
	{
		Node* ptrs[INTERIOR_LENGTH];//�ڶ���
	};
	//Ҷ�ڵ�
	struct Leaf 
	{
		void* values[LEAF_LENGTH]; //������
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
