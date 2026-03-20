#0  t1buv_8 (ri=<optimized out>, ii=<optimized out>, W=0x80d300, rs=0x7e8100, mb=<optimized out>, 
    me=32, ms=32) at dft/simd/avx2-128/../../../../dft/simd/avx2-128/../common/t1buv_8.c:159
#1  0x00007ffff74034bc in apply (ego_=0x7e5d00, rio=0x794bc4, iio=0x794bc0)
    at dft/../../dft/dftw-direct.c:53
#2  0x00007ffff740ea43 in apply (ego_=0x7e6a80, ri=<optimized out>, ii=<optimized out>, 
    ro=<optimized out>, io=<optimized out>) at dft/../../dft/vrank-geq1.c:62
#3  0x00007ffff7eb3de1 in Convlevel::process (this=<optimized out>, skip=<optimized out>)
    at /usr/src/debug/zita-convolver-4.0.3-18.fc44.x86_64/source/zita-convolver.cc:803
#4  0x00007ffff7eb445f in Convlevel::readout (this=0x794af0, sync=sync@entry=false, 
    skipcnt=<optimized out>)
    at /usr/src/debug/zita-convolver-4.0.3-18.fc44.x86_64/source/zita-convolver.cc:840
#5  0x00007ffff7eb456a in Convproc::process (this=0x78eaa8, sync=false)
    at /usr/src/debug/zita-convolver-4.0.3-18.fc44.x86_64/source/zita-convolver.cc:321
#6  Convproc::process (this=0x78eaa8, sync=false)
    at /usr/src/debug/zita-convolver-4.0.3-18.fc44.x86_64/source/zita-convolver.cc:308
#7  0x00007ffff7871b68 in __start_rt_text () from /usr/lib64/lv2/gx_amp.lv2/gx_amp.so
#8  0x00007ffff7836a13 in ?? () from /usr/lib64/lv2/gx_amp.lv2/gx_amp.so
#9  0x00007ffff781e5da in ?? () from /usr/lib64/lv2/gx_amp.lv2/gx_amp.so
#10 0x00000000004010ec in lilv_instance_run (instance=0x78e820, sample_count=512)
    at /usr/include/lilv-0/lilv/lilv.h:1894
#11 0x0000000000403542 in LV2Plugin::process (this=0x7fffffffda90, inputBuffer=0x7fffffffd290, 
    outputBuffer=0x7fffffffca90, numFrames=512)
    at /home/djshaji/AndroidStudioProjects/opiqoGuitarMultiEffectsProcessor/app/src/main/cpp/LV2Plugin.hpp:595
#12 0x0000000000401078 in main () at test.cpp:25
