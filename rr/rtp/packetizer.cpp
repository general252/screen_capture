#include "packetizer.h"
#include "h264_packet.h"

namespace rtp {

    Packetizer::Packetizer(uint16_t mtu,
        uint8_t pt, uint32_t ssrc, 
        Payloader* payloader, 
        Sequencer* sequencer, 
        uint32_t clockRate)
    {
        this->mtu = mtu;
        this->payloadType = pt;
        this->ssrc = ssrc;
        this->payloader = payloader;
        this->sequencer = sequencer;
        this->clockRate = clockRate;
        this->timestamp = 555;
    }

    bool Packetizer::Packetize(const uint8_t* payload, int32_t payloadLen, uint32_t samples, std::vector<Packet>& packets)
    {
        if (!payload || payloadLen <= 0) {
            return false;
        }

        std::vector<std::string> payloads;
        if (false == this->payloader->Payload(this->mtu - 12, payload, payloadLen, payloads)) {
            return false;
        }


        for (size_t i = 0; i < payloads.size(); i++)
        {
            Packet pkt;
            Header& h = pkt.header;

            h.version = 2;
            h.padding = false;
            h.extension = false;
            h.marker = (i == payloads.size() - 1);
            h.payloadType = this->payloadType;
            h.sequenceNumber = this->sequencer->NextSequenceNumber();
            h.timestamp = this->timestamp;
            h.ssrc = this->ssrc;

            pkt.payload = payloads[i];

            packets.push_back(pkt);
        }

        this->timestamp += samples;


        return true;
    }


    void PacketizeTest()
    {
        uint16_t mtu = 1200;
        uint8_t pt = 96;
        uint32_t ssrc = 360;
        uint32_t clockRate = 90000;

        h264::H264Payloader payloader;
        Sequencer sequencer(1);

        std::vector<Packet> packets;
        uint32_t samples = 40 * clockRate / 1000; //  // uint32((time.Millisecond * 40).Seconds() * 90000)

        Packetizer packetizer(mtu, pt, ssrc, &payloader, &sequencer, clockRate);

        packetizer.Packetize(NULL, 0, samples, packets);

        printf("");
    }

}
