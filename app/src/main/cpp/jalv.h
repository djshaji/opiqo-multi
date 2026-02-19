//
// Created by djshaji on 2/11/26.
//

#ifndef OPIQO_KITTY_JALV_H
#include <jalv/jalv.h>
#include "lv2/buf-size/buf-size.h"

#define N_BUFFER_CYCLES 16
/// These features have no data
static const LV2_Feature static_features[] = {
        {LV2_STATE__loadDefaultState, NULL},
        {LV2_BUF_SIZE__powerOf2BlockLength, NULL},
        {LV2_BUF_SIZE__fixedBlockLength, NULL},
        {LV2_BUF_SIZE__boundedBlockLength, NULL}};


int jalv_open_(Jalv* const jalv, const char* const load_arg);
void jalv_connect_ports(JalvBackend* const backend,
                         JalvProcess* const proc,
                         const uint32_t     port_index);
#endif //OPIQO_KITTY_JALV_H
