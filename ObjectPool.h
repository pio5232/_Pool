#pragma once


namespace jh
{
	template <typename T>
	class ObjectPool
	{
	public:

		//  생성자 호출 후 포인터를 반환
		template <typename... Args>
		static T* Alloc(Args&&... args)
		{
			T* obj = static_cast<T*>(g_pMemSystem->Alloc(sizeof(T)));

			new (obj) T(std::forward<Args>(args)...);

			return obj;
		}

		// 소멸자 호출 후 풀에 반환
		static void Free(T* obj)
		{
			if (nullptr == obj)
				return;	

			obj->~T();

			g_pMemSystem->Free(obj);
		}

	};

	template <typename T>
	struct CustomDeleter
	{
		void operator()(T* ptr) const
		{
			ObjectPool<T>::Free(ptr);
		}
	};

	template <typename T>
	using UniquePtr = std::unique_ptr<T, jh::CustomDeleter<T>>;


	template<typename T, typename... Args>
	std::shared_ptr<T> MakeShared(Args&&... args)
	{
		// 포인터와 삭제자를 등록 -> 생성
		//std::shared_ptr<T> ret = std::shared_ptr<T>(ObjectPool<T>::Alloc(std::forward<Args>(args)...), CustomDeleter<T>());
		std::shared_ptr<T> ret = std::shared_ptr<T>(ObjectPool<T>::Alloc(std::forward<Args>(args)...), ObjectPool<T>::Free);
		return ret;
		
		
		jh::MySTLAllocator<T> my_allocator;
		return std::allocate_shared<T, jh::MySTLAllocator<T>>(my_allocator, std::forward<Args>(args)...);
		// allocated_shared를 통해 내가 만든 메모리 할당자에서 참조카운트관리와 관리 대상을 연속적인 메모리 공간으로 할당할 수 있도록 한다.
		
		//return std::allocate_shared<T, Args...
	
	}

	template <typename T, typename... Args>
	UniquePtr<T> MakeUnique(Args&&... args)
	{
		return UniquePtr<T>(ObjectPool<T>::Alloc(std::forward<Args>(args)...), CustomDeleter<T>());
	}
}