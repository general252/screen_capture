#pragma once
#include <chrono>
#include <Windows.h>

namespace xop
{
    class Timestamp
    {
    public:
        Timestamp()
        {
            //begin_time_point_ = std::chrono::high_resolution_clock::now();
            tickCount = GetTickCount64();
        }

        void Reset()
        {
            //begin_time_point_ = std::chrono::high_resolution_clock::now();
            tickCount = GetTickCount64();
        }

        int64_t Elapsed()
        {
            // return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin_time_point_).count();
            auto t = GetTickCount64();
            return (t - tickCount);
        }

        void AddMilliseconds(int64_t ms) {
            //begin_time_point_ += std::chrono::milliseconds(ms);
            tickCount += ms;
        }

    private:
        //std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_point_;
        int64_t tickCount = 0;
    };
}
