//
// Created by djshaji on 3/4/26.
//

#ifndef OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_UTILS_H
#define OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_UTILS_H

#include <string>
#include <vector>

const char * __no_load__ [] = {
        "http://rakarrack.sourceforge.net/effects.html#awha",
        "http://VeJaPlugins.com/plugins/Release/BassCab",
        nullptr
};

bool isNoLoadPlugin(const char* uri) {
    for (int i = 0; __no_load__[i] != nullptr; i++) {
        if (strcmp(uri, __no_load__[i]) == 0) {
            return true;
        }
    }

    return false;
}

#endif //OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_UTILS_H
