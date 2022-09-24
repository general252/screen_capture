#pragma once

#include <cstdint>

namespace binary {

	class BigEndian
	{
	public:
		static uint16_t Uint16(const char* b);
		static uint32_t Uint32(const char* b);
		static void PutUint16(char* b, uint16_t v);
		static void PutUint32(char* b, uint32_t v);
	};


}
