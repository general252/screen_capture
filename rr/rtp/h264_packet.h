#pragma once

#include <string>
#include <vector>
#include <functional>
#include "packetizer.h"

namespace h264
{
    enum {
		stapaNALUType = 24,
		fuaNALUType = 28,
		fubNALUType = 29,
		spsNALUType = 7,
		ppsNALUType = 8,
		audNALUType = 9,
		fillerNALUType = 12,

		fuaHeaderSize = 2,
		stapaHeaderSize = 1,
		stapaNALULengthSize = 2,

		naluTypeBitmask = 0x1F,
		naluRefIdcBitmask = 0x60,
		fuStartBitmask = 0x80,
		fuEndBitmask = 0x40,

		outputStapAHeader = 0x78,
    };

    class H264Payloader : public rtp::Payloader
    {
    public:

		// Payload fragments a H264 packet across one or more byte arrays
		bool Payload(uint16_t mtu, const uint8_t* payload, int32_t payloadLen, std::vector<std::string>& payloads);


	protected:
		const std::string& annexbNALUStartCode();
		void emitNalus(const uint8_t* nals, int32_t nalsLen, std::function<void(const uint8_t* nalu, int32_t naluLen)> emit);

	private:
		bool nextInd(const uint8_t* nalu, int32_t naluLen, int32_t start, int32_t& indStart, int32_t& indLen);

    public:
        std::string spsNalu, ppsNalu;
    };

}
