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
#include <dlfcn.h>
#include "jalv.h"
#include "LV2Plugin.hpp"
#include "utils.h"

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
        engine ->initLV2();
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

/*
How to use:

Stop effect: AudioEngine.setEffectOn(false)
Set block size: AudioEngine.setPluginBlockSize(128) or 256
Re-enable effect: AudioEngine.setEffectOn(true)

*/
JNIEXPORT jboolean JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setPluginBlockSize(
    JNIEnv *env, jclass type, jint blockFrames) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine "
            "before calling this method");
        return JNI_FALSE;
    }

    return engine->setPluginBlockSize((int32_t) blockFrames) ? JNI_TRUE : JNI_FALSE;
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
    engine -> plugin1 = lv2Plugin ;
    lv2Plugin->getControl("GAIN")->setValue(0.f);
    lv2Plugin->getControl("VOLUME")->setValue(0.f);
    lv2Plugin->getControl("TONE")->setValue(0.f);
//    lv2Plugin->ports_.at(3).control = 1.f;
    lv2Plugin->ports_.at(4).control = 0.4f;
//    lv2Plugin->ports_.at(5).control = 0.f;
//    return ;

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
        if (!lilv_port_is_a(plugin, port, engine -> audio_class_)) {
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
                    break;

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

    const char* cachePath = env->GetStringUTFChars(path, nullptr);
    if (!cachePath) {
        LOGE("setCacheDir: failed to get UTF chars");
        return;
    }
    engine->cacheDir = std::string(cachePath);
    env->ReleaseStringUTFChars(path, cachePath);

}
extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setValue(JNIEnv *env, jclass clazz, jint p, jint index,
                                                              jfloat value) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                "method");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(engine->pluginMutex);

        LV2Plugin * plugin = nullptr ;
        switch (p) {
            case 1:
                plugin = engine->plugin1;
                break;
            case 2:
                plugin = engine->plugin2;
                break;
            case 3:
                plugin = engine->plugin3;
                break;
            case 4:
                plugin = engine->plugin4;
                break;
            default:
                LOGE("Unknown plugin index %d", p);
                return;
        }

        if (plugin == nullptr) {
            LOGE("Plugin %d is null", p);
            return;
        }

        LOGD("[setValue] Setting plugin %d port %d to value %f", p, index, value);
        plugin->ports_.at (index).control = value;
    }
}


extern "C"
JNIEXPORT jint JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_addPlugin(JNIEnv *env, jclass clazz, jint position,
                                                         jstring uri) {

    engine -> bypass = true ;

    const char* uriChars = env->GetStringUTFChars(uri, nullptr);
    if (!uriChars) {
        LOGE("addPlugin: failed to get UTF chars for plugin uri");
        engine -> bypass = false ;
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(engine->pluginMutex);

        LV2Plugin * plugin = nullptr ;
        switch (position) {
            case 1:
                plugin = engine->plugin1;
                engine ->plugin1 = nullptr;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin1 = nullptr;
                break;
            case 2:
                plugin = engine->plugin2;
                engine ->plugin2 = nullptr;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin2 = nullptr;
                break;
            case 3:
                plugin = engine->plugin3;
                engine ->plugin3 = nullptr;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin3 = nullptr;
                break;
            case 4:
                plugin = engine->plugin4;
                engine ->plugin4 = nullptr;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin4 = nullptr;
                break;
            default:
                LOGE("Unknown plugin index %d", position);
                engine -> bypass = false ;
                return -1;
        }

        if (plugin != nullptr) {
            plugin ->closePlugin();
            delete plugin;
        }

        plugin = new LV2Plugin(engine -> world, uriChars, engine -> sampleRate, engine -> blockSize);
        LOGD("[plugin %s] Created plugin %s at position %d", lilv_node_as_string(
            lilv_plugin_get_library_uri(plugin->plugin_)) , uriChars, position);

        if ( !plugin->initialize()) {
            LOGE("[load plugin] Failed to initialize plugin %s", uriChars);
            char * libname = const_cast<char *>(basename(
                    lilv_node_as_string(lilv_plugin_get_library_uri(plugin->plugin_))));
            LOGE("[load plugin] Failed to initialize plugin %s from library %s", uriChars, libname);
            void * lib = dlopen (libname, RTLD_NOW | RTLD_LOCAL);
            if (lib == nullptr) {
                LOGE("[load plugin] Failed to load library %s: %s", libname, dlerror());
            } else {
                void (*init_func)(void) = (void (*)(void)) dlsym(lib, "lv2_descriptor");
                if (init_func == nullptr) {
                    LOGE("[load plugin] Failed to find lv2_descriptor in library %s: %s", libname, dlerror());
                } else {
                    LOGD("[debug] Successfully found lv2_descriptor in library %s", libname);
                }

                LOGW("[debug] Successfully loaded library %s manually", libname);
                dlclose(lib);
            }

            env->ReleaseStringUTFChars(uri, uriChars);
            engine -> bypass = false ;
            return -1;
        } else {
            plugin->start();
            LOGD("Successfully added plugin %s at position %d", uriChars, position);
//            LOGD ("[plugininfo] %s", engine->pluginInfo[env->GetStringUTFChars(uri, nullptr)].dump(4).c_str());
            int portsTotal = lilv_plugin_get_num_ports(plugin->plugin_);
//            LOGD("[debug] Plugin %s has [%d/%d] ports", env->GetStringUTFChars(uri, nullptr), plugin->ports_.size(), portsTotal);
        }

        switch (position) {
            case 1:
                engine->plugin1 = plugin;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin1 = plugin;
                break;
            case 2:
                engine->plugin2 = plugin;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin2 = plugin;
                break;
            case 3:
                engine->plugin3 = plugin;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin3 = plugin;
                break;
            case 4:
                engine->plugin4 = plugin;
                if (engine -> mDuplexStream)
                    engine -> mDuplexStream -> plugin4 = plugin;
                break;
            default:
                LOGE("Unknown plugin index %d", position);
                env->ReleaseStringUTFChars(uri, uriChars);
                engine -> bypass = false ;
                return -1;
        }
    }

    env->ReleaseStringUTFChars(uri, uriChars);

    engine -> bypass = false ;
    return 0 ;
}



extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_initPlugins(JNIEnv *env, jclass clazz,
                                                           jstring dir) {
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


    LOGD ("[test] LV2 path set to %s", path.c_str());

    LilvNode* lv2_path = lilv_new_string(engine -> world, path.c_str());
    lilv_world_set_option(engine -> world, LILV_OPTION_LV2_PATH, lv2_path);
    lilv_node_free(lv2_path);

    lilv_world_load_all(engine -> world);

    engine -> plugins = lilv_world_get_all_plugins(engine -> world);
    engine -> pluginInfo = {} ;

    LILV_FOREACH (plugins, i, engine -> plugins) {
        const LilvPlugin* p = lilv_plugins_get(engine -> plugins, i);
        if (isNoLoadPlugin(lilv_node_as_string(lilv_plugin_get_uri(p)))) {
            LOGD("[initPlugins] Skipping no-load plugin %s", lilv_node_as_uri(lilv_plugin_get_uri(p)));
            continue;
        }

        engine -> pluginCount++;
//        LOGD("[plugin] %s\n", lilv_node_as_uri(lilv_plugin_get_uri(p)));
        json pluginInfo = {
                {"name", lilv_node_as_string(lilv_plugin_get_name(p))},
                {"uri", lilv_node_as_string(lilv_plugin_get_uri(p))},
                {"author", lilv_node_as_string(lilv_plugin_get_author_name(p))},
                {"ports", lilv_plugin_get_num_ports(p)}};

        pluginInfo["port"] = {};
        for (int index = 0 ; index < lilv_plugin_get_num_ports(p); index++) {
            const LilvPort* port = lilv_plugin_get_port_by_index(p, index);
//            LOGD ("[%s] Port %d: %s\n", lilv_node_as_string(lilv_plugin_get_name(p)),
//                  index, lilv_node_as_string(lilv_port_get_symbol(p, port)));
            pluginInfo ["port"][index] = {};
            pluginInfo ["port"][index]["index"] = index ;
            pluginInfo ["port"][index]["name"] = lilv_node_as_string(lilv_port_get_name(p, port));
            if (lilv_port_is_a(p, port, engine -> audio_class_)) {
//                LOGD("[%s] Port %d is an audio port\n", lilv_node_as_string(lilv_plugin_get_name(p)), index);
                pluginInfo["port"][index]["type"] = "audio";
            }
            else if (lilv_port_is_a(p, port, engine -> control_class_) &&
                    lilv_port_is_a(p, port, engine -> input_class_)) {
                pluginInfo["port"][index]["type"] = "control";

                if (lilv_port_has_property(p, port, engine -> toggle_class_)) {
                    pluginInfo["port"][index]["type"] = "toggled";
                }

                if (lilv_port_has_property(p, port, engine -> enum_class_)) {
                    pluginInfo["port"][index]["type"] = "dropdown";

                    // Query Scale Points (Labels for the enum)
                    LilvScalePoints* points = lilv_port_get_scale_points(p, port);
                    pluginInfo["port"][index]["options"] = json::array();

                    LILV_FOREACH(scale_points, s, points) {
                        const LilvScalePoint* point = lilv_scale_points_get(points, s);
                        json sp;
                        sp["label"] = lilv_node_as_string(lilv_scale_point_get_label(point));
                        sp["value"] = lilv_node_as_float(lilv_scale_point_get_value(point));
                        pluginInfo["port"][index]["options"].push_back(sp);
                    }
                }
//                LOGD("[%s] Port %d is a control port\n", lilv_node_as_string(lilv_plugin_get_name(p)), index);
                LilvNode * def = lilv_new_float(engine -> world, 0.0f);
                LilvNode * min = lilv_new_float(engine -> world, 0.0f);
                LilvNode * max = lilv_new_float(engine -> world, 0.0f);
                lilv_port_get_range(p, port, reinterpret_cast<LilvNode **>(&def),
                                    reinterpret_cast<LilvNode **>(&min),
                                    reinterpret_cast<LilvNode **>(&max));
                pluginInfo["port"][index]["min"] = lilv_node_as_float(min) ;
                pluginInfo["port"][index]["max"] = lilv_node_as_string(max);
                pluginInfo["port"][index]["default"] = lilv_node_as_string(def);

            }
            else if (lilv_port_is_a(p, port, engine -> atom_class_)) {
                pluginInfo["port"][index]["type"] = "atom";
//                LOGD ("[%s] Port %d is an atom port\n", lilv_node_as_string(lilv_plugin_get_name(p)), index);
            } else {
                LOGW ("[%s] Port %d (%s) is of unknown type\n", lilv_node_as_string(lilv_plugin_get_name(p)), index, lilv_node_as_string(lilv_port_get_symbol(p, port)));
            }

//            LOGI("[plugin ok] Port %d: %s\n", index, lilv_node_as_string(lilv_port_get_symbol(p, port)));
        }

        // probe atom path writable ports
        printf("\n== patch:writable parameters (lv2:Parameter) ==\n");
        const LilvNodes* writables = lilv_plugin_get_value(p, engine -> patch_writable);
        json writableParams = {};
        bool hasWritableParams = false ;
        LILV_FOREACH(nodes, i, writables) {
            hasWritableParams = true ;
            json info = {};
            const LilvNode* param = lilv_nodes_get(writables, i); // e.g. rata:Neural_Model

            LilvNodes* labels = lilv_world_find_nodes(engine -> world, param, engine -> rdfs_label, NULL);
            LilvNodes* ranges = lilv_world_find_nodes(engine -> world, param, engine -> rdfs_range, NULL);
            LilvNodes* types  = lilv_world_find_nodes(engine -> world, param, engine -> mod_filetypes, NULL);

            LOGD ("Writable parameter: %s", lilv_node_as_string(param));
            LILV_FOREACH(nodes, j, labels) {
                const LilvNode* label = lilv_nodes_get(labels, j);
                LOGD ("  label: %s", lilv_node_as_string(label));
                info ["label"] = lilv_node_as_string(label);
            }

            LILV_FOREACH(nodes, j, ranges) {
                const LilvNode* range = lilv_nodes_get(ranges, j);
                LOGD ("  range: %s", lilv_node_as_string(range));
                info ["range"] = lilv_node_as_string(range);
            }

            LILV_FOREACH(nodes, j, types) {
                const LilvNode* type = lilv_nodes_get(types, j);
                LOGD ("  type: %s", lilv_node_as_string(type));
                info ["type"] = lilv_node_as_string(type);
            }

            lilv_nodes_free(labels);
            lilv_nodes_free(ranges);
            lilv_nodes_free(types);
            writableParams [lilv_node_as_string(param)] = info ;
        }

        engine->pluginInfo [pluginInfo["uri"]] = pluginInfo;
        if (hasWritableParams) {
            engine->pluginInfo [pluginInfo["uri"]]["writableParams"] = writableParams;
        }
    }

//    LOGD("[plugininfo] %s", engine -> pluginInfo.dump(4).c_str());
    LOGD ("[initPlugins] Found %d plugins", engine -> pluginCount);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_getPluginInfo(JNIEnv *env, jclass clazz) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                "method");
        return env->NewStringUTF("{}");
    }

    return env->NewStringUTF(to_string (engine -> pluginInfo).c_str());
}
extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_deletePlugin(JNIEnv *env, jclass clazz,
                                                            jint plugin) {
    IN
    engine -> bypass = true ;

    {
        std::lock_guard<std::mutex> lock(engine->pluginMutex);

        switch (plugin) {
            case 1:
                if (engine->plugin1) {
                    if (engine -> mDuplexStream)
                        engine->mDuplexStream->plugin1 = nullptr;
                    engine->plugin1->closePlugin();
                    delete engine->plugin1;
                    engine->plugin1 = nullptr;
                }
                break;
            case 2:
                if (engine->plugin2) {
                    if (engine -> mDuplexStream)
                        engine->mDuplexStream->plugin2 = nullptr;
                    engine->plugin2->closePlugin();
                    delete engine->plugin2;
                    engine->plugin2 = nullptr;
                }
                break ;
            case 3:
                if (engine->plugin3) {
                    if (engine -> mDuplexStream)
                        engine->mDuplexStream->plugin3 = nullptr;
                    engine->plugin3->closePlugin();
                    delete engine->plugin3;
                    engine->plugin3 = nullptr;
                }
                break ;
            case 4:
                if (engine->plugin4) {
                    if (engine -> mDuplexStream)
                        engine->mDuplexStream->plugin4 = nullptr;
                    engine->plugin4->closePlugin();
                    delete engine->plugin4;
                    engine->plugin4 = nullptr;
                }
                break;
            default:
                LOGE("Unknown plugin index %d", plugin);
        }
    }

    engine -> bypass = false ;
    OUT
}
extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setGain(JNIEnv *env, jclass clazz, jfloat gain) {
    * engine -> gain = gain ;
}
extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setPluginEnabled(JNIEnv *env, jclass clazz,
                                                                jint plugin, jboolean is_enabled) {
    {
        std::lock_guard<std::mutex> lock(engine->pluginMutex);

        switch (plugin) {
            case 1:
                if (engine->plugin1) {
                    engine->plugin1->enabled = is_enabled;
                }
                break;
            case 2:
                if (engine->plugin2) {
                    engine->plugin2->enabled = is_enabled;
                }
                break ;
            case 3:
                if (engine->plugin3) {
                    engine->plugin3->enabled = is_enabled;
                }
                break ;
            case 4:
                if (engine->plugin4) {
                    engine->plugin4->enabled = is_enabled;
                }
                break;
            default:
                LOGE("Unknown plugin index %d", plugin);
        }
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_bypass(JNIEnv *env, jclass clazz,
                                                      jboolean is_bypassed) {
    engine -> bypass = is_bypassed;
}

json getPreset (int plugin) {
    LV2Plugin * p = nullptr ;
    switch (plugin) {
        case 1:
            p = engine->plugin1;
            break;
        case 2:
            p = engine->plugin2;
            break ;
        case 3:
            p = engine->plugin3;
            break ;
        case 4:
            p = engine->plugin4;
            break;
        default:
            LOGE("Unknown plugin index %d", plugin);
            return "{}";
    }

    if (p == nullptr) {
        LOGE("Plugin %d is null", plugin);
        return "{}";
    }

    json preset = {};
    preset ["name"] = lilv_node_as_string(lilv_plugin_get_name(p->plugin_));
    preset ["uri"] = lilv_node_as_string(lilv_plugin_get_uri(p->plugin_));
    preset ["controls"] = {};
    for (const auto& port : p->ports_) {
        if (! port.is_audio)
            preset ["controls"][port.name] = port.control;
    }

    preset ["enabled"] = p->enabled;
    preset ["writables"] = p->writables;
    return preset;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_printPreset(JNIEnv *env, jclass clazz, jint plugin) {
    json preset = getPreset(plugin);
    LOGD("[preset] Plugin %d preset: %s", plugin, preset.dump(4).c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_getPreset(JNIEnv *env, jclass clazz, jint plugin) {
    json preset = getPreset(plugin);
    return env->NewStringUTF(preset.dump().c_str());
}

/*  This function name is a misnomer.
 *  It returns the active preset for each plugin slot, but it does not return a list of presets.
 *  The preset for each plugin is a JSON object containing the plugin URI, control values, and enabled state.
 */
extern "C"
JNIEXPORT jstring JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_getPresetList(JNIEnv *env, jclass clazz) {
    json preset = {};
    preset ["app"] = "opiqo-android";
    preset ["gain"] = *engine -> gain ;

    preset ["plugin1"] = getPreset(1);
    preset ["plugin2"] = getPreset(2);
    preset ["plugin3"] = getPreset(3);
    preset ["plugin4"] = getPreset(4);

    return env->NewStringUTF(preset.dump().c_str());
}


extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_setFilePath(JNIEnv *env, jclass clazz, jint plugin,
                                                           jstring uri, jstring path) {
    LV2Plugin * p = nullptr ;
    switch (plugin) {
        case 1:
            p = engine->plugin1;
            break;
        case 2:
            p = engine->plugin2;
            break ;
        case 3:
            p = engine->plugin3;
            break ;
        case 4:
            p = engine->plugin4;
            break ;
        default:
            LOGE("Unknown plugin index %d", plugin);
            return;
    }

    int port = -1 ;
    for (const auto& prt : p->ports_) {
        if (prt.is_atom && prt.is_input) {
            port = prt.index;
            break;
        }
    }

    if (port == -1) {
        LOGE("Plugin %d does not have an input atom port", plugin);
        return;
    } else {
        LOGD ("Plugin %d atom input port found at index %d", plugin, port);
    }

    std::string pathStr, uriStr;
    if (path != nullptr) {
        const char* cstr = env->GetStringUTFChars(path, nullptr);
        if (cstr) {
            pathStr.assign(cstr);
            env->ReleaseStringUTFChars(path, cstr);
        }

        const char* cstrUri = env->GetStringUTFChars(uri, nullptr);
        if (cstrUri) {
            uriStr.assign(cstrUri);
            env->ReleaseStringUTFChars(uri, cstrUri);
        }
    }

    if (pathStr.empty() || uriStr.empty()) {
        LOGE("Path or URI is empty");
        return;
    }

    LOGD("Setting plugin %d port %d to file path %s with URI %s", plugin, port, pathStr.c_str(), uriStr.c_str());
    p ->send_path_parameter(uriStr.c_str(), pathStr.c_str());

    std::vector<uint8_t> msg;
    if (p->readAtomMessage("notify", msg)) {
        std::string path, property;
        if (p->extractPathFromAtomMessage(msg.data(), msg.size(), path, &property)) {
            LOGD("Got file path '%s' for property '%s'", path.c_str(), property.c_str());
        } else {
            LOGE("Failed to extract file path from atom message for plugin %d", plugin);
        }
    } else {
        LOGE ("Failed to read atom message from plugin %d after setting file path", plugin);
    }

}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_getWritables(JNIEnv *env, jclass clazz,
                                                            jint plugin) {
    LV2Plugin * p = nullptr ;
    switch (plugin) {
        case 1:
            p = engine->plugin1;
            break;
        case 2:
            p = engine->plugin2;
            break;
        case 3:
            p = engine->plugin3;
            break;
        case 4:
            p = engine->plugin4;
            break;
        default:
            LOGE("Unknown plugin index %d", plugin);
            return env->NewStringUTF("{}");
    }

    return env->NewStringUTF(to_string(p->writables).c_str());
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_startRecording(JNIEnv *env, jclass clazz,
                                                              jint fd, jint file_type, jint quality) {
    return engine->fileWriter->open(fd,
                                    static_cast<FileType>(file_type), quality);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_stopRecording(JNIEnv *env, jclass clazz) {
    engine->fileWriter->close();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_acoustixaudio_opiqo_multi_AudioEngine_stressTest(JNIEnv *env) {
    const char * uri = "http://guitarix.sourceforge.net/plugins/gx_amp#GUITARIX";
    LV2Plugin plugin(engine->world, uri, 48000., 4096);
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

}
