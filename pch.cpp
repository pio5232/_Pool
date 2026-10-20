#include "pch.h"

jh::MemorySystem* g_pMemSystem = nullptr;
class GGen
{
public : 
	GGen()
	{
		g_pMemSystem = new jh::MemorySystem();
	}

	~GGen()
	{
		delete g_pMemSystem;
	}
} gen;