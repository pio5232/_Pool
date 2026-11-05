#pragma once


namespace jh
{
	class MemoryPool;

#ifdef _DEBUG
	const LONGLONG kGuardValue = 0x1234'5678'9abc'def0;

	struct alignas (16) MemoryHeader
	{
		// [MemoryHeader][Data]
		MemoryHeader(size_t size) : m_size{ size }, m_kFrontGuard{kGuardValue} {}

		static void* AttachHeader(MemoryHeader* header, size_t size)
		{
			new (header) MemoryHeader(size);

			// 끝부분을 GuardValue로 설정한다.
			LONGLONG* rearGuard = reinterpret_cast<LONGLONG*>(reinterpret_cast<char*>(header) + size - sizeof(m_kFrontGuard));

			*rearGuard = kGuardValue;

			return reinterpret_cast<void*>(++header);
		}

		static MemoryHeader* DetachHeader(void* ptr)
		{
			MemoryHeader* header = reinterpret_cast<MemoryHeader*>(ptr) - 1;

			// frontGuard와 맞지 않을때
			if (header->m_kFrontGuard != kGuardValue)
				jh_utility::CrashDump::Crash();

			size_t size = header->m_size;

			LONGLONG* rearGuard = reinterpret_cast<LONGLONG*>(reinterpret_cast<char*>(header) + size - sizeof(m_kFrontGuard));

			if (*rearGuard != kGuardValue)
				jh_utility::CrashDump::Crash();

#ifdef JH_POOL_TEST
			*rearGuard = 0;
#endif
			return header;
		}


		// Debug모드인 경우 FrontGuard와 RearGuard를 포함한 크기를 추가한다.
		// (FrontGuard는 이미 sizeof(MemoryHeader)에 포함되어있다.
		// RearGuard의 크기는 FrontGuard와 동일. sizeof(m_kFrontGuard)를 추가한다.
		static size_t GetExtraSize()
		{
			return sizeof(MemoryHeader) + sizeof(m_kFrontGuard);
		}
		const size_t m_size; // 64bit : 8byte, 32bit : 4byte
		const LONGLONG m_kFrontGuard;
	};

#else
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

		// Release 모드인 경우 MemoryHeader 사이즈만큼을 요청한 size에 더한다.
		static size_t GetExtraSize() 
		{
			return sizeof(MemoryHeader);
		}
		const size_t m_size; // 64bit : 8byte, 32bit : 4byte
	};
#endif
}