#include "h264_packet.h"
#include "binary.h"

namespace h264
{
    int min(int a, int b) {
        if (a < b) {
            return a;
        }
        return b;
    }

    bool H264Payloader::Payload(uint16_t mtu, const uint8_t* payload, int32_t payloadLen, std::vector<std::string>& payloads)
    {
        if (!payload || payloadLen <= 0) {
            return true;
        }

        auto emit = [&](const uint8_t* nalu, int32_t naluLen) {

            if (!nalu || naluLen <= 0) {
                return;
            }

            int8_t  naluType = nalu[0] & naluTypeBitmask;
            int8_t naluRefIdc = nalu[0] & naluRefIdcBitmask;

            switch (naluType)
            {
            case audNALUType:
            case fillerNALUType:
                return;
            case spsNALUType:
                this->spsNalu.assign((char*)nalu, naluLen);
                return;
            case ppsNALUType:
                this->ppsNalu.assign((char*)nalu, naluLen);
                return;
            default:
                break;
            }

            if (this->spsNalu.size() > 0 && this->ppsNalu.size() > 0)
            {
                char spsLen[2] = { 0 };
                binary::BigEndian::PutUint16(spsLen, uint16_t(this->spsNalu.size()));

                char ppsLen[2] = { 0 };
                binary::BigEndian::PutUint16(ppsLen, uint16_t(this->ppsNalu.size()));

                char _outputStapAHeader[1] = { outputStapAHeader };

                std::string stapANalu;
                stapANalu.append(_outputStapAHeader, 1);
                stapANalu.append(spsLen, 2);
                stapANalu.append(this->spsNalu);
                stapANalu.append(ppsLen, 2);
                stapANalu.append(this->ppsNalu);

                if (stapANalu.size() <= mtu) {
                    std::string out(stapANalu);
                    payloads.push_back(out);
                }

                this->spsNalu.resize(0);
                this->ppsNalu.resize(0);
            }

            if (naluLen <= mtu) {
                std::string out((char*)nalu, naluLen);
                payloads.push_back(out);
                return;
            }

            // FU-A
            int32_t maxFragmentSize = mtu - fuaHeaderSize;

            // The FU payload consists of fragments of the payload of the fragmented
            // NAL unit so that if the fragmentation unit payloads of consecutive
            // FUs are sequentially concatenated, the payload of the fragmented NAL
            // unit can be reconstructed.  The NAL unit type octet of the fragmented
            // NAL unit is not included as such in the fragmentation unit payload,
            // 	but rather the information of the NAL unit type octet of the
            // fragmented NAL unit is conveyed in the F and NRI fields of the FU
            // indicator octet of the fragmentation unit and in the type field of
            // the FU header.  An FU payload MAY have any number of octets and MAY
            // be empty.

            const uint8_t* naluData = nalu;
            // According to the RFC, the first octet is skipped due to redundant information
            int32_t naluDataIndex = 1;
            int32_t naluDataLength = naluLen - naluDataIndex;
            int32_t naluDataRemaining = naluDataLength;

            if (min(maxFragmentSize, naluDataRemaining) <= 0) {
                return;
            }

            while (naluDataRemaining > 0)
            {
                int32_t currentFragmentSize = min(maxFragmentSize, naluDataRemaining);

                std::string _out;
                _out.resize(fuaHeaderSize + currentFragmentSize);

                char* out = (char*)_out.data();

                // +---------------+
                // |0|1|2|3|4|5|6|7|
                // +-+-+-+-+-+-+-+-+
                // |F|NRI|  Type   |
                // +---------------+
                out[0] = fuaNALUType;
                out[0] |= naluRefIdc;

                // +---------------+
                // |0|1|2|3|4|5|6|7|
                // +-+-+-+-+-+-+-+-+
                // |S|E|R|  Type   |
                // +---------------+

                out[1] = naluType;
                if (naluDataRemaining == naluDataLength) {
                    // Set start bit
                    out[1] |= 1 << 7;
                }
                else if (naluDataRemaining - currentFragmentSize == 0) {
                    // Set end bit
                    out[1] |= 1 << 6;
                }

                memcpy(out + fuaHeaderSize, naluData + naluDataIndex, currentFragmentSize);
                payloads.push_back(_out);

                naluDataRemaining -= currentFragmentSize;
                naluDataIndex += currentFragmentSize;
            }
        };


        this->emitNalus(payload, payloadLen, emit);


        return true;
    }


    void H264Payloader::emitNalus(const uint8_t* nals, int32_t nalsLen, std::function<void(const uint8_t* nalu, int32_t naluLen)> emit)
    {
        int32_t nextIndStart = 0, nextIndLen = 0;

        nextInd(nals, nalsLen, 0, nextIndStart, nextIndLen);
        if (nextIndStart == -1) {
            emit(nals, nalsLen);
        }
        else {
            while (nextIndStart != -1)
            {
                int prevStart = nextIndStart + nextIndLen;
                nextInd(nals, nalsLen, prevStart, nextIndStart, nextIndLen);
                if (nextIndStart != -1) {
                    //printf("%d - %d\n", prevStart, nextIndStart);
                    emit(nals + prevStart, nextIndStart - prevStart);
                }
                else {
                    // Emit until end of stream, no end indicator found
                    //printf("%d - %d\n", prevStart, nalsLen);
                    emit(nals + prevStart, nalsLen - prevStart);
                }
            }
        }

        //printf("");
    }

    bool H264Payloader::nextInd(const uint8_t* nalu, int32_t naluLen, int32_t start, int32_t& indStart, int32_t& indLen)
    {
        int32_t zeroCount = 0;
        for (int32_t i = 0; i < naluLen - start; i++)
        {
            uint8_t b = nalu[start + i];
            if (b == 0) {
                zeroCount++;
                continue;
            }
            else if (b == 1) {
                if (zeroCount >= 2) {
                    indStart = start + i - zeroCount;
                    indLen = zeroCount + 1;

                    return true;
                }
            }
            zeroCount = 0;
        }

        indStart = -1;
        indLen = -1;

        return false;
    }


    const std::string& H264Payloader::annexbNALUStartCode()
    {
        static std::string v({ 0x00, 0x00, 0x00, 0x01 }, 4);

        return v;
    }


}
