#include <AL/al.h>
#include <AL/alc.h>
#include <stdio.h>

int main(void) {
    ALCdevice *dev = alcOpenDevice(NULL);
    printf("openal %s\n", dev ? "OK" : "NODEV");
    if (dev) alcCloseDevice(dev);
    return 0;
}
