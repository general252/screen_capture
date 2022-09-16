#include <stdio.h>
#include <thread>

#include "capture/d3d11_screen_capture.h"
#include "capture/d3d9_screen_capture.h"

#include "encode/h264_encoder.h"

#pragma comment(lib, "./lib/ffmpeg/lib/avdevice.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/avformat.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/swresample.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/avcodec.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/swscale.lib")
#pragma comment(lib, "./lib/ffmpeg/lib/avutil.lib")

int main(int argc, char* argv[])
{
    DX::D3D9ScreenCapture screen_capture;
    if (!screen_capture.Init()) {
        return -2;
    }

    int framerate = 25;

    ffmpeg::AVConfig encoder_config_;
    encoder_config_.video.framerate = framerate;
    encoder_config_.video.bitrate = 8000 * 1000;
    encoder_config_.video.gop = framerate;
    encoder_config_.video.format = AV_PIX_FMT_BGRA;
    encoder_config_.video.width = 640;// screen_capture.GetWidth();
    encoder_config_.video.height = 480;//  screen_capture.GetHeight();

    ffmpeg::H264Encoder h264_encoder_;

    if (!h264_encoder_.Init(encoder_config_)) {
        return -3;
    }

    FILE* fp = fopen("out.h264", "wb+");

    time_t now;
    time(&now);

    printf("now: %I64d\n", now);
    for (size_t i = 0; i < 25 * 5; i++)
    {
        if (true) {
            DX::Image bgra_image;
            if (screen_capture.Capture(bgra_image))
            {
                int in_width = bgra_image.width;
                int in_height = bgra_image.height;
                uint8_t* in_buffer = &bgra_image.bgra[0];
                int image_size = bgra_image.bgra.size();

                std::vector<uint8_t> out_frame;
                h264_encoder_.Encode(in_buffer, in_width, in_height, image_size, out_frame);

                if (out_frame.size() > 0) {
                    //printf("%d\n", out_frame.size());
                    fwrite(&out_frame[0], 1, out_frame.size(), fp);
                }
            }
        }


        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    time(&now);

    printf("now: %I64d\n", now);

    fclose(fp);


    return 0;
}
