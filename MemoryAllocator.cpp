#include "pch.h"
#include "MemoryAllocator.h"


jh::MemoryAllocator::MemoryAllocator() : m_nodeStack{}, m_pPool{}
{

}

jh::MemoryAllocator::~MemoryAllocator()
{
	if (nullptr == m_pPool)
		return;

	for (int i = 0; i < kPoolCount; i++)
	{
		NodeStack& stack = m_nodeStack[i];

		while (1)
		{
			// 스택이 빌 때까지 다음 노드를 꺼냅니다.
			Node* node = stack.Pop();

			if (nullptr == node)
				break;

			m_pPool[i]->TryPushNode(node);
		}
	}
}

void* jh::MemoryAllocator::Alloc(size_t allocSize)
{
	MEMORY_POOL_PROFILE_FLAG;
	int poolIdx = poolTable[allocSize];

	void* allocedPointer = m_nodeStack[poolIdx].Pop();

	// 할당할 메모리가 없는 경우 L2에서 가져와서 다시 할당한다.
	if (nullptr == allocedPointer)
	{
		AcquireBlockFromPool(poolIdx);
		return m_nodeStack[poolIdx].Pop();
	}
	return allocedPointer;
}



void jh::MemoryAllocator::Dealloc(void* ptr, size_t allocSize)
{
	MEMORY_POOL_PROFILE_FLAG;
	int poolIdx = poolTable[allocSize];

	NodeStack& nodeStack = m_nodeStack[poolIdx];
	// 해제한 메모리 반납한다.
	nodeStack.Push(static_cast<Node*>(ptr));

	// 일정 수량 이상이면 절반을 LEVEL 2에 반납한다.

	if (nodeStack.GetTotalCount() == (kNodeCountPerBlock * 2))
	{
		m_pPool[poolIdx]->TryPushBlock(nodeStack.m_pSubHead, kNodeCountPerBlock);
		nodeStack.m_pSubHead = nullptr;
		nodeStack.m_subCount = 0;

	}
}

jh::MemoryAllocator::NodeStack::NodeStack() : m_pMainHead{}, m_pSubHead{}, m_mainCount{}, m_subCount{} {}

void jh::MemoryAllocator::NodeStack::Push(Node* newNode)
{
	if (nullptr == newNode)
		return;

	if (kNodeCountPerBlock == m_mainCount)
		SwapHead();

	newNode->m_pNextNode = m_pMainHead;
	m_pMainHead = newNode;
	m_mainCount++;

	// 크기가 최대가 되면 하나의 블락으로 두고 다른 블락을 쌓도록 한다.
}
jh::Node* jh::MemoryAllocator::NodeStack::Pop()
{
	if (true == IsMainEmpty())
	{
		SwapHead();
		if (true == IsMainEmpty())
			return nullptr;
	}

	Node* ret = m_pMainHead;

	m_pMainHead = m_pMainHead->m_pNextNode;
	m_mainCount--;
	return ret;
}
