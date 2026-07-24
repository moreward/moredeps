#include <rtc/rtc.hpp>
#include <cstdio>

int main(void) {
    rtc::InitLogger(rtc::LogLevel::Warning);
    auto pc = std::make_unique<rtc::PeerConnection>();
    printf("rtc OK\n");
    return 0;
}
