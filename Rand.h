#pragma once
#include <mutex>
#include <random>
namespace jh
{
	class MyRandom
	{
	public:
		// ¿ëµµ : static GetRand
		MyRandom() = delete;

		static int GetRandInt(int s, int e) 
		{
			//static thread_local std::random_device rd;
			//static thread_local std::mt19937_64 gen(rd());

			static thread_local std::mt19937 gen(0);
			std::uniform_int_distribution<int> dist(s, e);

			return dist(gen);
		}
	};
	

}

