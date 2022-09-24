#include "packet.h"
#include "binary.h"


namespace rtp
{
    const char* errHeaderSizeInsufficient = "RTP header size insufficient";
    const char* errHeaderSizeInsufficientForExtension = "RTP header size insufficient for extension";
    const char* errTooSmall = "buffer too small";
    const char* errHeaderExtensionsNotEnabled = "h.Extension not enabled";
    const char* errHeaderExtensionNotFound = "extension not found";

    const char* errRFC8285OneByteHeaderIDRange = "header extension id must be between 1 and 14 for RFC 5285 one byte extensions";
    const char* errRFC8285OneByteHeaderSize = "header extension payload must be 16bytes or less for RFC 5285 one byte extensions";

    const char* errRFC8285TwoByteHeaderIDRange = "header extension id must be between 1 and 255 for RFC 5285 two byte extensions";
    const char* errRFC8285TwoByteHeaderSize = "header extension payload must be 255bytes or less for RFC 5285 two byte extensions";

    const char* errRFC3550HeaderIDRange = "header extension id must be 0 for non-RFC 5285 extensions";

    // MarshalSize returns the size of the header once marshaled.
    int32_t Header::MarshalSize()
    {
        int32_t size = 12 + (this->csrc.size() * csrcLength);

        if (this->extension && this->extensions.size() > 0)
        {
            int32_t extSize = 4;

            switch (this->extensionProfile)
            {
                // RFC 8285 RTP One Byte Header Extension
            case extensionProfileOneByte:
                for (size_t i = 0; i < this->extensions.size(); i++) {
                    extSize += 1 + this->extensions[i].payload.size();
                }
                break;

                // RFC 8285 RTP Two Byte Header Extension
            case extensionProfileTwoByte:
                for (size_t i = 0; i < this->extensions.size(); i++) {
                    extSize += 2 + this->extensions[i].payload.size();
                }
                break;
            default:
                extSize += this->extensions[0].payload.size();
                break;
            }

            // extensions size must have 4 bytes boundaries
            size += ((extSize + 3) / 4) * 4;
        }

        return size;
    }

    int32_t Header::Marshal(std::string& _buf, std::string& err)
    {
        /*
         *  0                   1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |                           timestamp                           |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |           synchronization source (SSRC) identifier            |
         * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
         * |            contributing source (CSRC) identifiers             |
         * |                             ....                              |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        int size = this->MarshalSize();
        if (size > _buf.size()) {
            _buf.resize(size);
        }

        char* buf = (char*)_buf.data();


        // The first byte contains the version, padding bit, extension bit,
        // and csrc size.
        buf[0] = (this->version << versionShift) | uint8_t(this->csrc.size());
        if (this->padding) {
            buf[0] |= 1 << paddingShift;
        }

        if (this->extension) {
            buf[0] |= 1 << extensionShift;
        }

        // The second byte contains the marker bit and payload type.
        buf[1] = this->payloadType;
        if (this->marker) {
            buf[1] |= 1 << markerShift;
        }

        binary::BigEndian::PutUint16(buf + 2, this->sequenceNumber);
        binary::BigEndian::PutUint32(buf + 4, this->timestamp);
        binary::BigEndian::PutUint32(buf + 8, this->ssrc);

        int32_t n = 12;
        for (size_t i = 0; i < this->csrc.size(); i++) {
            uint32_t csrc = this->csrc[i];
            binary::BigEndian::PutUint32(buf + n, csrc);
            n += 4;
        }

        if (this->extension && this->extensions.size() > 0) {
            int32_t extHeaderPos = n;
            binary::BigEndian::PutUint16(buf + n, this->extensionProfile);
            n += 4;
            int32_t startExtensionsPos = n;

            switch (this->extensionProfile)
            {
                // RFC 8285 RTP One Byte Header Extension
            case extensionProfileOneByte:
                for (size_t i = 0; i < this->extensions.size(); i++) {
                    Extension extension = this->extensions[i];
                    buf[n] = extension.id << 4 | (uint8_t(extension.payload.size()) - 1);
                    n++;

                    memcpy(buf + n, extension.payload.data(), extension.payload.size());
                    n += extension.payload.size();
                }
                break;
                // RFC 8285 RTP Two Byte Header Extension
            case extensionProfileTwoByte:
                for (size_t i = 0; i < this->extensions.size(); i++) {
                    Extension extension = this->extensions[i];
                    buf[n] = extension.id;
                    n++;
                    buf[n] = uint8_t(extension.payload.size());
                    n++;
                    memcpy(buf + n, extension.payload.data(), extension.payload.size());
                    n += extension.payload.size();
                }
                break;
            default:

                // RFC3550 Extension
                int32_t extlen = int32_t(this->extensions[0].payload.size());
                if (extlen % 4 != 0) {
                    // the payload must be in 32-bit words.
                    err.assign("short buffer");
                    return -1;
                }

                memcpy(buf + n, this->extensions[0].payload.data(), this->extensions[0].payload.size());
                n += this->extensions[0].payload.size();
                break;
            }


            // calculate extensions size and round to 4 bytes boundaries
            int32_t extSize = n - startExtensionsPos;
            int32_t roundedExtSize = ((extSize + 3) / 4) * 4;

            binary::BigEndian::PutUint16(buf + extHeaderPos + 2, uint16_t(roundedExtSize / 4));

            // add padding to reach 4 bytes boundaries
            for (int32_t i = 0; i < roundedExtSize - extSize; i++) {
                buf[n] = 0;
                n++;
            }
        }

        if (n != size) {
            printf("size error. n: %d, size: %d\n", n, size);
        }

        return n;
    }

    int32_t Header::Unmarshal(const std::string& _buf, std::string& err)
    {
        err.clear();

        if (_buf.size() < headerLength) {
            err.assign(errHeaderSizeInsufficient);
            printf("%s: %d < %d\n", errHeaderSizeInsufficient, _buf.size(), headerLength);
            return 0;
        }

        /*
         *  0                   1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |                           timestamp                           |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |           synchronization source (SSRC) identifier            |
         * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
         * |            contributing source (CSRC) identifiers             |
         * |                             ....                              |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        const char* buf = _buf.data();

        this->version = buf[0] >> versionShift & versionMask;
        this->padding = (buf[0] >> paddingShift & paddingMask) > 0;
        this->extension = (buf[0] >> extensionShift & extensionMask) > 0;
        int32_t nCSRC = int32_t(buf[0] & ccMask);

        this->csrc.resize(nCSRC);

        int32_t n = csrcOffset + (nCSRC * csrcLength);
        if (_buf.size() < n) {
            err.assign(errHeaderSizeInsufficient);
            printf("size %d < %d: %s\n", _buf.size(), n, errHeaderSizeInsufficient);
            return n;
        }

        this->marker = (buf[1] >> markerShift & markerMask) > 0;
        this->payloadType = buf[1] & ptMask;

        this->sequenceNumber = binary::BigEndian::Uint16(buf + seqNumOffset);
        this->timestamp = binary::BigEndian::Uint32(buf + timestampOffset);
        this->ssrc = binary::BigEndian::Uint32(buf + ssrcOffset);

        for (size_t i = 0; i < this->csrc.size(); i++) {
            int32_t offset = csrcOffset + (i * csrcLength);
            this->csrc[i] = binary::BigEndian::Uint32(buf + offset);
        }

        this->extensions.clear();

        if (this->extension) {

            int32_t expected = n + 4;
            if (_buf.size() < expected) {
                err.assign(errHeaderSizeInsufficientForExtension);
                printf("size %d < %d: %s\n", _buf.size(), expected, errHeaderSizeInsufficientForExtension);
                return n;
            }

            this->extensionProfile = binary::BigEndian::Uint16(buf + n);
            n += 2;
            int32_t extensionLength = int(binary::BigEndian::Uint16(buf + n)) * 4;
            n += 2;

            expected = n + extensionLength;
            if (_buf.size() < expected) {
                err.assign(errHeaderSizeInsufficientForExtension);
                printf("size %d < %d: %s\n", _buf.size(), expected, errHeaderSizeInsufficientForExtension);
                return n;
            }


            switch (this->extensionProfile)
            {
                // RFC 8285 RTP One Byte Header Extension
            case extensionProfileOneByte:
            {
                int32_t end = n + extensionLength;
                while (n < end)
                {
                    if (buf[n] == 0x00) { // padding
                        n++;
                        continue;
                    }

                    uint8_t extid = buf[n] >> 4;
                    int32_t len = int(buf[n] & ~0xF0 + 1);
                    n++;

                    if (extid == extensionIDReserved) {
                        break;
                    }

                    Extension extension;
                    extension.id = extid;
                    extension.payload.assign(buf + n, len);

                    this->extensions.push_back(extension);
                    n += len;
                }
            } break;
            // RFC 8285 RTP Two Byte Header Extension
            case extensionProfileTwoByte:
            {
                int32_t end = n + extensionLength;
                while (n < end)
                {
                    if (buf[n] == 0x00) { // padding
                        n++;
                        continue;
                    }

                    uint8_t extid = buf[n];
                    n++;

                    int32_t len = int(buf[n]);
                    n++;


                    Extension extension;
                    extension.id = extid;
                    extension.payload.assign(buf + n, len);
                    this->extensions.push_back(extension);
                    n += len;
                }

            } break;
            default:
            {
                if (_buf.size() < n + extensionLength) {
                    err.assign(errHeaderSizeInsufficientForExtension);
                    printf("%s: %d < %d\n", errHeaderSizeInsufficientForExtension, _buf.size(), n + extensionLength);
                    return n;
                }

                Extension extension;
                extension.id = 0;
                extension.payload.assign(buf + n, extensionLength);
                this->extensions.push_back(extension);

                n += extension.payload.size();
            } break;
            }

        }

        return n;
    }

    bool Header::SetExtension(uint8_t id, const std::string& payload)
    {
        if (this->extension)
        {
            switch (this->extensionProfile)
            {
                // RFC 8285 RTP One Byte Header Extension
            case extensionProfileOneByte:
                if (id < 1 || id > 14) {
                    printf("%s actual(%d)\n", errRFC8285OneByteHeaderIDRange, id);
                    return false;
                }
                if (payload.size() > 16) {
                    printf("%s actual(%d)\n", errRFC8285OneByteHeaderSize, payload.size());
                    return false;
                }
                break;
                // RFC 8285 RTP Two Byte Header Extension
            case extensionProfileTwoByte:
                if (id < 1 || id > 255) {
                    printf("%s actual(%d)\n", errRFC8285TwoByteHeaderIDRange, id);

                }
                if (payload.size() > 255) {
                    printf("%s actual(%d)\n", errRFC8285TwoByteHeaderSize, payload.size());
                    return false;
                }
                break;
            default:
                // RFC3550 Extension
                if (id != 0) {
                    printf("%s actual(%d)\n", errRFC3550HeaderIDRange, id);
                    return false;
                }
                break;
            }

            // Update existing if it exists else add new extension
            for (size_t i = 0; i < this->extensions.size(); i++) {
                if (this->extensions[i].id == id) {
                    this->extensions[i].payload.assign(payload);
                    return true;
                }
            }

            Extension object;
            object.id = id;
            object.payload = payload;

            this->extensions.push_back(object);
            return true;
        }

        // No existing header extensions
        this->extension = true;

        int len = payload.size();
        if (len <= 16) {
            this->extensionProfile = extensionProfileOneByte;
        }
        else if (len > 16 && len < 256) {
            this->extensionProfile = extensionProfileTwoByte;
        }

        Extension object;
        object.id = id;
        object.payload = payload;

        this->extensions.push_back(object);

        return true;
    }



    int32_t Packet::MarshalSize()
    {
        int32_t headSize = this->header.MarshalSize();
        int32_t packetSize = headSize + this->payload.size() + int(this->paddingSize);

        return packetSize;
    }

    // Marshal serializes the packet into bytes.
    int32_t Packet::Marshal(std::string& _buf, std::string& err)
    {
        int32_t size = this->MarshalSize();
        if (size > _buf.size()) {
            _buf.resize(size);
        }

        this->header.padding = (this->paddingSize != 0);

        int32_t n = this->header.Marshal(_buf, err);
        if (n < 0) {
            return n;
        }

        char* buf = (char*)_buf.data();

        // Make sure the buffer is large enough to hold the packet.
        if (n + int32_t(this->payload.size()) + int32_t(this->paddingSize) > int32_t(_buf.size())) {
            err.assign("short buffer");
            return -2;
        }

        memcpy(buf + n, this->payload.data(), this->payload.size());
        int32_t m = int32_t(this->payload.size());

        if (this->header.padding) {
            buf[n + m + int32_t(this->paddingSize - 1)] = this->paddingSize;
        }


        return n + m + int32_t(this->paddingSize);
    }

    bool Packet::Unmarshal(std::string& _buf, std::string& err)
    {
        int32_t n = this->header.Unmarshal(_buf, err);
        if (err.size() > 0) {
            return false;
        }

        int32_t end = (int32_t)_buf.size();
        if (this->header.padding) {
            this->paddingSize = _buf[end - 1];
            end -= int32_t(this->paddingSize);
        }

        if (end < n) {
            err.assign(errTooSmall);
            return false;
        }

        this->payload.assign(_buf.data() + n, end - n);

        return true;
    }

    std::string Packet::String()
    {
        std::stringstream out;
        out << "RTP PACKET:\n";
        out << "\tVersion: " << this->header.version << std::endl;
        out << "\tMarker: " << this->header.marker << std::endl;
        out << "\tPayload Type: " << this->header.payloadType << std::endl;
        out << "\tSequence Number: " << this->header.sequenceNumber << std::endl;
        out << "\tTimestamp: " << this->header.timestamp << std::endl;
        out << "\tSSRC: " << this->header.ssrc << std::endl;
        out << "\tPayload Length: " << this->payload.size() << std::endl;

        return out.str();
    }

}
