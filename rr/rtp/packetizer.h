#pragma once

// packetizer

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>
#include "packet.h"

namespace rtp {

    class Payloader
    {
    public:
        virtual~Payloader() {}

        virtual bool Payload(uint16_t mtu, const uint8_t* payload, int32_t payloadLen, std::vector<std::string>& payloads) = 0;
    };

    class Sequencer {
    public:
        Sequencer(uint16_t s) {
            this->sequenceNumber = s - 1; // -1 because the first sequence number prepends 1
        }

        virtual~Sequencer() {}
        virtual uint16_t NextSequenceNumber() {
            std::lock_guard<std::mutex> lock(this->mutex);

            this->sequenceNumber++;
            if (this->sequenceNumber == 0) {
                this->rollOverCount++;
            }

            return this->sequenceNumber;
        }
        virtual uint64_t RollOverCount() {
            std::lock_guard<std::mutex> lock(this->mutex);

            return this->rollOverCount;
        }

    private:
        uint16_t sequenceNumber = 0;
        uint64_t rollOverCount = 0;
        std::mutex mutex;
    };


    class Packetizer
    {
    public:
        Packetizer(uint16_t mtu, uint8_t pt, uint32_t ssrc, Payloader* payloader, Sequencer* sequencer, uint32_t clockRate);

        bool Packetize(const uint8_t* payload, int32_t payloadLen, uint32_t samples, std::vector<Packet>& packets);

        // SkipSamples causes a gap in sample count between Packetize requests so the
        // RTP payloads produced have a gap in timestamps
        void SkipSamples(uint32_t skippedSamples ) {
            this->timestamp += skippedSamples;
        }

    public:
        uint16_t mtu;
        uint8_t payloadType;
        uint32_t ssrc;
        Payloader* payloader;
        Sequencer* sequencer;
        uint32_t timestamp;
        uint32_t clockRate;
    };


    void PacketizeTest();
}
