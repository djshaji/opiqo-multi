#define LOG_TAG "Test"
#define LOGD printf
#define LOGE printf
#define IN printf("[%s] ", __func__);
#define OUT printf("[%s] done\n", __func__);
#define HERE printf("[%s:%d] ", __FILE__, __LINE__);
#include <lilv/lilv.h>
#include "LV2Plugin.hpp"
#include <unistd.h>

int main () {
    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);
    const char * uri = "http://guitarix.sourceforge.net/plugins/gx_amp#GUITARIX";
    LV2Plugin plugin(world, uri, 48000., 4096);
    if (plugin.initialize()) {
        plugin.start();
    } else {
        LOGE("Failed to initialize plugin");
    }
    
    float input[512] = {0.5};
    float output[512] = {0.5};
    int t = 0;
    while (true) {
        LOGD("Running test iteration %d\n", t++);
        plugin.process(input, output, 512);
        usleep (10000); // Sleep for 100ms to simulate time between process calls
    }

    return 0;
  }