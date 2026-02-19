/**
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>
#include "logging_macros.h"
#include "LiveEffectEngine.h"
#include <jalv/jalv.h>
#include <jalv/backend.h>
#include <lilv/lilv.h>
#include <fstream>
#include "jalv.h"
#include "LV2Plugin.hpp"

static const int kOboeApiAAudio = 0;
static const int kOboeApiOpenSLES = 1;

static LiveEffectEngine *engine = nullptr;

std::string readFileToString(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("Failed to open file: " + path);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_create(JNIEnv *env, jclass) {
    if (engine == nullptr) {
        engine = new LiveEffectEngine();
    }

    return (engine != nullptr) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_delete(JNIEnv *env,
                                                               jclass) {
    if (engine) {
        engine->setEffectOn(false);
        delete engine;
        engine = nullptr;
    }
}

JNIEXPORT jboolean JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setEffectOn(
    JNIEnv *env, jclass, jboolean isEffectOn) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return JNI_FALSE;
    }

    return engine->setEffectOn(isEffectOn) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setRecordingDeviceId(
    JNIEnv *env, jclass, jint deviceId) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return;
    }

    engine->setRecordingDeviceId(deviceId);
}

JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setPlaybackDeviceId(
    JNIEnv *env, jclass, jint deviceId) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return;
    }

    engine->setPlaybackDeviceId(deviceId);
}

JNIEXPORT jboolean JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setAPI(JNIEnv *env,
                                                               jclass type,
                                                               jint apiType) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine "
            "before calling this method");
        return JNI_FALSE;
    }

    oboe::AudioApi audioApi;
    switch (apiType) {
        case kOboeApiAAudio:
            audioApi = oboe::AudioApi::AAudio;
            break;
        case kOboeApiOpenSLES:
            audioApi = oboe::AudioApi::OpenSLES;
            break;
        default:
            LOGE("Unknown API selection to setAPI() %d", apiType);
            return JNI_FALSE;
    }

    return engine->setAudioApi(audioApi) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_isAAudioRecommended(
    JNIEnv *env, jclass type) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine "
            "before calling this method");
        return JNI_FALSE;
    }
    return engine->isAAudioRecommended() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_native_1setDefaultStreamValues(JNIEnv *env,
                                               jclass type,
                                               jint sampleRate,
                                               jint framesPerBurst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) sampleRate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) framesPerBurst;
}
} // extern "C"


extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_test(JNIEnv *env, jclass clazz, jstring dir) {
    std::string path;
    if (dir != nullptr) {
        const char* cstr = env->GetStringUTFChars(dir, nullptr);
        if (cstr) {
            path.assign(cstr);
            env->ReleaseStringUTFChars(dir, cstr);
        }
    } else {
        LOGE("[test] path is null");
        return ;
    }

    LilvWorld* world = lilv_world_new();
    LOGD ("[test] LV2 path set to %s", path.c_str());

    LilvNode* lv2_path = lilv_new_string(world, path.c_str());
    lilv_world_set_option(world, LILV_OPTION_LV2_PATH, lv2_path);
    lilv_node_free(lv2_path);

    lilv_world_load_all(world);

    const LilvPlugins* plugins = lilv_world_get_all_plugins(world);

    LILV_FOREACH (plugins, i, plugins) {
        const LilvPlugin* p = lilv_plugins_get(plugins, i);
        LOGD("[test] plugin %s\n", lilv_node_as_uri(lilv_plugin_get_uri(p)));


    }

    LV2Plugin * lv2Plugin = new LV2Plugin(world, "http://guitarix.sourceforge.net/plugins/gx_sloopyblue_#_sloopyblue_", 48000., 4096);
    lv2Plugin->initialize();
    lv2Plugin->start();
    engine -> plugin = lv2Plugin ;
    lv2Plugin->getControl("GAIN")->setValue(0.f);
    lv2Plugin->getControl("VOLUME")->setValue(0.f);
    lv2Plugin->getControl("TONE")->setValue(0.f);
//    lv2Plugin->ports_.at(3).control = 1.f;
    lv2Plugin->ports_.at(4).control = 0.4f;
//    lv2Plugin->ports_.at(5).control = 0.f;
    return ;

    LilvNode* plugin_uri = lilv_new_uri(world, "http://guitarix.sourceforge.net/plugins/gx_sloopyblue_#_sloopyblue_");
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, plugin_uri);
    if (plugin == NULL) {
        LOGD ("[test] Failed to find plugin");
        return ;
    }

    LOGD ("[test] Found plugin [%s] %s\n", lilv_node_as_string(lilv_plugin_get_name(plugin)), lilv_node_as_uri(lilv_plugin_get_uri(plugin)));
    LOGD ("[test] Plugin has %d ports\n", lilv_plugin_get_num_ports(plugin));

    LilvInstance* instance = lilv_plugin_instantiate(plugin, 48000.0, nullptr);
    if (instance != NULL) {
        LOGD("Ladies and Gentlemen we have liftoff") ;
    } else {
        LOGD ("[test] Failed to instantiate plugin");
    }

    engine -> instance = instance;
    for (int i = 0 ; i < lilv_plugin_get_num_ports(plugin); i++) {
        const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
        LOGD ("[test] Port %d: %s\n", i, lilv_node_as_string(lilv_port_get_symbol(plugin, port)));
        if (!lilv_port_is_a(plugin, port, lilv_new_uri(world, LV2_CORE__AudioPort))) {
            float * d = static_cast<float *>(malloc(sizeof(float)));
            lilv_instance_connect_port(instance, i, d);
            LOGD ("[test] Connected control port %d to %p [%s]\n", i, d, lilv_node_as_string(
                    lilv_port_get_name(plugin, port)));
            LilvNode * min = lilv_new_float(world, 0.0f);
            LilvNode * max = lilv_new_float(world, 0.0f);
            LilvNode * def = lilv_new_float(world, 0.0f);
            lilv_port_get_range(plugin, port, &def, &min, &max);
            LOGD ("[test] Port %d range: min=%f max=%f default=%f\n", i, lilv_node_as_float(min),
                  lilv_node_as_float(max), lilv_node_as_float(def));
            switch (i) {
                case 2:
                    *d = 0.f;
                    break;
                case 3:
                case 4:
                case 5:
                    *d = 1.f;
                    break;
                default:
//                    *d = 0.0f;

            }
        }
    }

    lilv_instance_activate(instance);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setCacheDir(JNIEnv *env, jclass clazz,
                                                                 jstring path) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return;
    }

    engine->cacheDir = std::string (env->GetStringUTFChars(path, nullptr));
    free((void*)env->GetStringUTFChars(path, nullptr));

}
extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setValue(JNIEnv *env, jclass clazz, jint index,
                                                              jfloat value) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                "method");
        return;
    }

    switch (index) {
        case 0:
            engine->plugin->ports_.at(2).control = value;
            break;
        case 1:
            engine->plugin->ports_.at(3).control = value;
            break;
        case 2:
            engine->plugin->ports_.at(4).control = value;
            break;
        case 3:
            engine->plugin->ports_.at(5).control = value;
            break;
        default:
            LOGE("Unknown control index %d", index);

    }
}