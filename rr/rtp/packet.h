#pragma once

// rtp packet

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>
#include <sstream>

namespace rtp
{
    enum
    {
        headerLength = 4,
        versionShift = 6,
        versionMask = 0x3,
        paddingShift = 5,
        paddingMask = 0x1,
        extensionShift = 4,
        extensionMask = 0x1,
        extensionProfileOneByte = 0xBEDE,
        extensionProfileTwoByte = 0x1000,
        extensionIDReserved = 0xF,
        ccMask = 0xF,
        markerShift = 7,
        markerMask = 0x1,
        ptMask = 0x7F,
        seqNumOffset = 2,
        seqNumLength = 2,
        timestampOffset = 4,
        timestampLength = 4,
        ssrcOffset = 8,
        ssrcLength = 4,
        csrcOffset = 12,
        csrcLength = 4,
    };

    class Extension {
    public:
        uint8_t id;
        std::string payload;
    };

    class Header
    {
    public:

        // MarshalSize returns the size of the header once marshaled.
        int32_t MarshalSize();

        // MarshalTo serializes the header and writes to the buffer.
        int32_t Marshal(std::string& _buf, std::string& err);

        // Unmarshal parses the passed byte slice and stores the result in the Header.
        // It returns the number of bytes read n and any error.
        int32_t Unmarshal(const std::string& _buf, std::string& err);

        // SetExtension sets an RTP header extension
        bool SetExtension(uint8_t id, const std::string& payload);

    public:

        uint8_t version;
        bool padding;
        bool extension;
        bool marker;
        uint8_t payloadType;
        uint16_t  sequenceNumber;
        uint32_t timestamp;
        uint32_t ssrc;
        std::vector<uint32_t>  csrc;
        uint16_t extensionProfile;
        std::vector<Extension> extensions;
    };


    class Packet
    {
    public:
        // MarshalSize returns the size of the packet once marshaled.
        int32_t MarshalSize();

        // Marshal serializes the packet into bytes.
        int32_t Marshal(std::string& _buf, std::string& err);

        // Unmarshal parses the passed byte slice and stores the result in the Packet.
        bool Unmarshal(std::string& _buf, std::string& err);

        std::string String();

    public:
        Header header;
        std::string payload;
        uint8_t paddingSize = 0;
    };

}
