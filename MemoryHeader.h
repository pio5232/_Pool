#pragma once


namespace jh
{
	class MemoryPool;

	struct alignas (16) MemoryHeader
	{
		// [MemoryHeader][Data]
		MemoryHeader(size_t size) : m_size{ size } {}

		static void* AttachHeader(MemoryHeader* header, size_t size)
		{
			new (header) MemoryHeader(size);

			return reinterpret_cast<void*>(++header);
		}

		static MemoryHeader* DetachHeader(void* ptr)
		{
			return reinterpret_cast<MemoryHeader*>(ptr) - 1;
		}

		const size_t m_size;
	};
}