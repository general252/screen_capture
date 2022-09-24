#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <list>

#include "net/udp_client.h"
#include "net/easywsclient.hpp"

#include "binary/timestamp.h"
#include "binary/base64.h"

#include "rtp/h264_packet.h"
#include "rtp/packetizer.h"


#include "capture/d3d11_screen_capture.h"
#include "capture/d3d9_screen_capture.h"

#include "encode/h264_encoder.h"

//#pragma comment(lib, "./lib/ffmpeg/lib/avdevice.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/avformat.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/swresample.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/avcodec.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/swscale.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/avutil.lib")


#pragma comment(lib, "ws2_32.lib")

const char* SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 6666;

#define USE_WEBSOCKET 0
#define USE_RTP  1
#define OUTPUT_FILE 0


int main(int argc, char* argv[])
{
    xop::net_init();

    uint16_t mtu = 1200;
    uint8_t pt = 96;
    uint32_t ssrc = 360;
    uint32_t clockRate = 90000;

    h264::H264Payloader payloader;
    rtp::Sequencer sequencer(1);

    uint32_t samples = 40 * clockRate / 1000; //  // uint32((time.Millisecond * 40).Seconds() * 90000)

    rtp::Packetizer packetizer(mtu, pt, ssrc, &payloader, &sequencer, clockRate);

#if USE_RTP
    xop::udp_client cli;
    if (!cli.Create(SERVER_IP, 6666)) {
        return -1;
    }
#endif


#if USE_WEBSOCKET
    easywsclient::WebSocket::pointer ws = easywsclient::WebSocket::from_url("ws://127.0.0.1:6666/h264_sender");
    if (!ws) {
        return -1;
    }
#endif


#if OUTPUT_FILE
    FILE* fpH264 = fopen("out.h264", "wb+");
#endif

    // 使用d3d11, d3d9采集耗时较长
    DX::D3D11ScreenCapture screen_capture;
    if (!screen_capture.Init()) {
        return -2;
    }

    int framerate = 25;

    ffmpeg::AVConfig encoder_config_;
    encoder_config_.video.framerate = framerate;
    encoder_config_.video.bitrate = 8*1024* 1000; // 8000 KBps
    encoder_config_.video.gop = framerate;
    encoder_config_.video.format = AV_PIX_FMT_BGRA;
    encoder_config_.video.width = 1920;// screen_capture.GetWidth();
    encoder_config_.video.height = 1080;//  screen_capture.GetHeight();


    // H264编码
    ffmpeg::H264Encoder h264_encoder_;

    if (!h264_encoder_.Init(encoder_config_)) {
        return -3;
    }

#if USE_RTP
    std::list<std::string> rtpRawList;
    std::mutex rtpRawMutext;

    // 添加rtp packet
    auto AddRtpRaw = [&](const std::string& rtpRaw) {
        std::lock_guard<std::mutex> locker(rtpRawMutext);

        rtpRawList.push_back(rtpRaw);
    };

    std::thread rtpSendThread([&]() {

        auto popRtpRawData = [&]()->std::string {
            std::lock_guard<std::mutex> locker(rtpRawMutext);

            if (rtpRawList.empty()) {
                return std::string();
            }

            std::string rtpRawData = rtpRawList.front();

            rtpRawList.pop_front();

            return rtpRawData;
        };

        const size_t PerSendSize = 10 * 1024 * 1024 / (1000 / 5);
        while (true)
        {
            Sleep(5);

            size_t sendSize = 0;
            
            while (sendSize < PerSendSize)
            {
                std::string rtpRawData = popRtpRawData();
                if (rtpRawData.empty()) {
                    break;
                }

                cli.Send(rtpRawData.data(), rtpRawData.size());

                sendSize += rtpRawData.size();
            }
        }

        });

#endif



    auto lastTime = std::chrono::high_resolution_clock::now();
    xop::Timestamp ts;
    int peerFrameDuration = 1000 / framerate; // 25fps, 每40ms采集一帧

    for (size_t i = 0; ; i++)
    {
#if USE_WEBSOCKET
        ws->poll();
        ws->dispatch([](const std::string& message) {
            printf(">>> %s\n", message.c_str());
            });
#endif // USE_WEBSOCKET

        bool isCapture = false;
        int64_t duration = 0;
        int64_t captureDuration = 0;

        if (false) {
            // 计算采集时间
            duration = ts.Elapsed();
            if (duration >= peerFrameDuration) {
                if (duration >= 1000) {
                    ts.Reset();
                }
                else {
                    ts.AddMilliseconds(peerFrameDuration);
                }

                isCapture = true;

                //printf("%I64d ms, is capture: %d\n", duration, isCapture);
            }
            else {
                auto ms = peerFrameDuration - duration;
                if (ms > 0) {
                    Sleep(ms - 0);
                    //std::this_thread::sleep_for(std::chrono::milliseconds(ms - 5));
                }
            }
        }
        else {
            isCapture = true;
        }

        // 采集 + 编码 + UDP发送
        if (isCapture)
        {
            auto a = std::chrono::high_resolution_clock::now();

            DX::Image bgra_image;
            if (screen_capture.Capture(bgra_image))
            {
                int in_width = bgra_image.width;
                int in_height = bgra_image.height;
                uint8_t* in_buffer = &bgra_image.bgra[0];
                int image_size = bgra_image.bgra.size();
                std::vector<uint8_t> out_h264_packet;

                // frame 编码为 H264 packet
                h264_encoder_.Encode(in_buffer, in_width, in_height, image_size, out_h264_packet);

                if (out_h264_packet.size() > 0)
                {
#if OUTPUT_FILE
                    // 写文件
                    fwrite(&out_h264_packet[0], 1, out_h264_packet.size(), fpH264);
#endif

#if USE_WEBSOCKET
                    // websocket 发送 packet
                    ws->sendBinary(out_frame);
#endif

#if USE_RTP
                    // H264打包为rtp, 发送rtp
                    std::vector<rtp::Packet> rtp_packets;
                    packetizer.Packetize(out_h264_packet.data(), out_h264_packet.size(), samples, rtp_packets);

                    for (size_t i = 0; i < rtp_packets.size(); i++) {
                        std::string rtpRawData;
                        std::string err;
                        if (rtp_packets[i].Marshal(rtpRawData, err) > 0) {
                            // 添加 rtp 到发送队列
                            AddRtpRaw(rtpRawData);
                            //cli.Send(rtpRawData.data(), rtpRawData.size());
                        }
                    }
#endif


                    auto b = std::chrono::high_resolution_clock::now();
                    captureDuration = std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                    auto lastTimeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(b - lastTime).count();
                    lastTime = b;

                    printf("[%I64d] capture&encode: %I64d, lastTime: %I64d, type: %02X, len: %d\n",
                        now, captureDuration,
                        lastTimeDuration,
                        out_h264_packet[4], out_h264_packet.size());

                }
            }
        }

        auto sleepTime = 40 - captureDuration;
        if (sleepTime > 0) {
            if (sleepTime > 5) {
                Sleep(sleepTime - 5);
            }
        }
    }


#if OUTPUT_FILE
    fclose(fpH264);
#endif


    xop::net_cleanup();
    return 0;
}




int main3()
{
    FILE* fp = fopen("out2.h264", "rb");

    const int SIZE = 2048 * 1024;
    char* buf = (char*)malloc(SIZE);
    if (!buf) {
        return -1;
    }

    memset(buf, 0, SIZE);

    int n = fread(buf, 1, SIZE, fp);


    {
        h264::H264Payloader payloader;

        std::vector<std::string> payloads;
        payloader.Payload(1200, (uint8_t*)buf, n, payloads);
        printf("%d\n", payloads.size());
    }

    {
        uint16_t mtu = 1200;
        uint8_t pt = 96;
        uint32_t ssrc = 360;
        uint32_t clockRate = 90000;

        h264::H264Payloader payloader;
        rtp::Sequencer sequencer(1);

        std::vector<rtp::Packet> packets;
        uint32_t samples = 40 * clockRate / 1000; //  // uint32((time.Millisecond * 40).Seconds() * 90000)

        rtp::Packetizer packetizer(mtu, pt, ssrc, &payloader, &sequencer, clockRate);

        packetizer.Packetize((uint8_t*)buf, n, samples, packets);

        FILE* fp = fopen("out.log", "wt+");

        for (size_t i = 0; i < packets.size(); i++)
        {
            std::string buf;
            std::string err;
            if (packets[i].Marshal(buf, err) > 0) {
                std::string out;
                binary::Base64::Encode(buf, out);


                out.append("\n\n");
                char header[128] = { 0 };
                sprintf(header, ">> %d\n", i);

                fwrite(header, 1, strlen(header), fp);
                fwrite(out.data(), 1, out.size(), fp);
            }
            else {
                printf("marshal fail. %s\n", err.data());
            }
        }

        fclose(fp);

        printf("");

    }

    return 0;
}
