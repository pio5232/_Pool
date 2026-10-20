#pragma once

namespace jh
{
	template <typename T>
	class MySTLAllocator
	{
	public:
		using value_type = T;

		MySTLAllocator() {}
		
		template<typename Other>
		MySTLAllocator(const MySTLAllocator<Other>& other) {};

		T* allocate(size_t count)
		{
			return static_cast<T*>(g_pMemSystem->Alloc(sizeof(T) * count));
		}

		void deallocate(T* ptr, size_t count)
		{
			g_pMemSystem->Free(ptr);
			return;
		}
	};
}
