# Credits

This project bundles LV2 plugins under `app/src/main/assets/lv2`.

Credits in this file are derived from plugin metadata (`*.ttl` files), including fields such as:
- `doap:name`
- `lv2:project`
- `doap:maintainer`
- `doap:license`
- `rdfs:seeAlso`

## Core Attribution

- **opiqo Guitar Multi Effects Pedal for Android**
  Native Android LV2 host and app integration.

- **LV2 ecosystem and supporting libraries**
  LV2 plugin standard and related open-source components used by bundled plugins and host integration.

## Bundled LV2 Plugin Credits

The `assets/lv2` directory contains many LV2 bundles (scan detected 255 bundle folders in the current repository state).

Representative upstream/plugin families identified in metadata include:

1. **Guitarix / Gx plugins**
	- Project URL found in metadata: `http://guitarix.sourceforge.net`
	- Includes many `Gx*` and `gx_*` plugin bundles.

2. **Rakarrack-derived / rkr plugins**
	- Project URLs in metadata include Rakarrack references (for example, `http://rakarrack.sourceforge.net/effects.html`).
	- Includes effect models and cabinet/amp style presets and controls.

3. **AIDA-X / neural amp model plugins**
	- Project URLs found in metadata include `http://aidadsp.cc` and `http://github.com/mikeoliphant/neural-amp-modeler-lv2`.

4. **Additional third-party LV2 plugins and collections**
	- Metadata includes multiple upstream references and plugin-specific attribution URLs.

## License Notes

Plugin metadata references multiple open-source licenses, including values such as:
- GPL-family license URLs
- ISC license URL references

## Libraries Used

In addition to bundled LV2 plugins, this project includes prebuilt native libraries under
`app/src/main/libs` for multiple Android ABIs, including `armeabi-v7a`, `arm64-v8a`, `x86`, and `x86_64`.

Representative bundled native libraries include:

- `liblilv.a`, `liblilv.so`
	Lilv is a C library that makes LV2 plugins easier for applications to discover, load, and use.

- `libjalv_static.a`
	Jalv is part of the LV2 hosting stack and is used here as host-side support for running LV2 plugins.

- `libjack_static.a`, `libjackserver_static.a`
	JACK is a professional sound server API and implementation for real-time, low-latency audio and MIDI connections between applications.

- `libserd.a`, `libserd-0.a`
	Serd is a lightweight C library for reading and writing RDF data, including Turtle-family formats used by LV2 metadata.

- `libsord.a`, `libsord-0_static.a`
	Sord is a lightweight C library for storing RDF statements in memory.

- `libsratom.a`
	Sratom is a small library for serializing LV2 atoms so binary and text forms can be converted or stored in RDF models.

- `libzix.a`
	Zix is a lightweight C portability and data-structure library used by the LV2 software stack.

- `libsndfile.a`
	libsndfile is a C library for reading and writing sampled sound files through one standard interface.

- `libfftw3.a`, `libfftw3f.a`
	FFTW is a C library for computing discrete Fourier transforms in one or more dimensions.

- `libmp3lame.a`
	LAME is a high-quality MPEG Audio Layer III (MP3) encoder.

- `libogg.a`
	Ogg is an open multimedia container format used to encapsulate compressed media streams.

- `libvorbis.a`, `libvorbisenc.a`
	Vorbis is an open, royalty-free compressed audio format, and the bundled libraries provide decoding and encoding support.

- `libopus.a`, `libopusenc.a`
	Opus is an open, royalty-free audio codec designed for interactive speech and music, and the bundled libraries provide codec and encoding support.

- `libFLAC.a`
	FLAC is a free lossless audio codec for compressing audio without quality loss.

These libraries support LV2 hosting, metadata parsing, real-time audio infrastructure,
audio file I/O, signal processing, and audio encoding/decoding used by the application.

For complete legal terms and redistribution conditions, refer to:
- `LICENSE.MD`
- `NOTICE.md`
- Original upstream plugin repositories/projects referenced by each plugin's TTL metadata

## Trademark And Naming Notice

Some bundled plugins, controls, or preset names may reference third-party products, amplifier models,
speaker models, or brand names.

All trademarks and brand names are the property of their respective owners.
Their inclusion is for identification/compatibility/reference purposes only.
No endorsement, sponsorship, or affiliation is implied.

## Maintainer Note

If an attribution is missing or incorrect, please open an issue on the project repository so credits can be updated.

## Appendix: Bundled LV2 Attribution Table

<!-- BEGIN AUTO-GENERATED LV2 APPENDIX -->
| Bundle | Plugin Name | Project URL |
|---|---|---|
| bentdelay.lv2 | the infamous bent delay | N/A |
| bluesbreaker.lv2 | bluesbreaker | urn:brummer:bluesbreaker |
| butterworth-swh.lv2 | Glame Butterworth X-over Filter | N/A |
| casynth.lv2 | the infamous cellular automaton synth | N/A |
| cheapdist.lv2 | the infamous cheap distortion | N/A |
| delay-swh.lv2 | Simple delay line, noninterpolating | N/A |
| delayorama-swh.lv2 | Delayorama | N/A |
| dyson_compress-swh.lv2 | Dyson compressor | N/A |
| ewham.lv2 | the infamous ewham | N/A |
| FatFrog.lv2 | FatFrog_ | https://github.com/brummer10/FatFrog |
| Gx4BandEQ.lv2 | N/A | N/A |
| gx_aclipper.lv2 | Gx_aclipper_ | http://guitarix.sourceforge.net/plugins/gx_aclipper_ |
| gx_alembic.lv2 | Gx_alembic | http://guitarix.sourceforge.net/plugins/gx_alembic |
| gx_amp.lv2 | GxAmplifier | http://guitarix.sourceforge.net/plugins/gx_amp |
| gx_amp_stereo.lv2 | GxAmplifier | http://guitarix.sourceforge.net/plugins/gx_amp_stereo |
| gx_bajatubedriver.lv2 | Gx_bajatubedriver_ | http://guitarix.sourceforge.net/plugins/gx_bajatubedriver_ |
| gx_barkgraphiceq.lv2 | Gx_barkgraphiceq_ | http://guitarix.sourceforge.net/plugins/gx_barkgraphiceq_ |
| gx_bmp.lv2 | Gx_bmp_ | http://guitarix.sourceforge.net/plugins/gx_bmp_ |
| gx_bossds1.lv2 | Gx_bossds1_ | http://guitarix.sourceforge.net/plugins/gx_bossds1_ |
| gx_cabinet.lv2 | GxCabinet | http://guitarix.sourceforge.net/plugins/gx_cabinet |
| gx_chorus.lv2 | Gx_chorus_stereo | http://guitarix.sourceforge.net/plugins/gx_chorus_stereo |
| gx_colwah.lv2 | Gx_colwah_ | http://guitarix.sourceforge.net/plugins/gx_colwah_ |
| gx_compressor.lv2 | Gx_compressor | http://guitarix.sourceforge.net/plugins/gx_compressor |
| gx_cstb.lv2 | Gx_cstb_ | http://guitarix.sourceforge.net/plugins/gx_cstb_ |
| gx_delay.lv2 | Gx_delay_stereo | http://guitarix.sourceforge.net/plugins/gx_delay_stereo |
| gx_detune.lv2 | Gx_detune_ | http://guitarix.sourceforge.net/plugins/gx_detune_ |
| gx_digital_delay.lv2 | Gx_digital_delay_ | http://guitarix.sourceforge.net/plugins/gx_digital_delay_ |
| gx_digital_delay_st.lv2 | Gx_digital_delay_st_ | http://guitarix.sourceforge.net/plugins/gx_digital_delay_st_ |
| gx_duck_delay.lv2 | Gx_duck_delay_ | http://guitarix.sourceforge.net/plugins/gx_duck_delay_ |
| gx_duck_delay_st.lv2 | Gx_duck_delay_st_ | http://guitarix.sourceforge.net/plugins/gx_duck_delay_st_ |
| gx_echo.lv2 | Gx_echo_stereo | http://guitarix.sourceforge.net/plugins/gx_echo_stereo |
| gx_expander.lv2 | Gx_expander | http://guitarix.sourceforge.net/plugins/gx_expander |
| gx_flanger.lv2 | Gx_flanger | http://guitarix.sourceforge.net/plugins/gx_flanger |
| gx_fumaster.lv2 | Gx_fumaster_ | http://guitarix.sourceforge.net/plugins/gx_fumaster_ |
| gx_fuzz.lv2 | Gx_fuzz_ | http://guitarix.sourceforge.net/plugins/gx_fuzz_ |
| gx_fuzzface.lv2 | Gx_fuzzface_ | http://guitarix.sourceforge.net/plugins/gx_fuzzface_ |
| gx_fuzzfacefm.lv2 | Gx_fuzzfacefm_ | http://guitarix.sourceforge.net/plugins/gx_fuzzfacefm_ |
| gx_gcb_95.lv2 | Gx_gcb_95_ | http://guitarix.sourceforge.net/plugins/gx_gcb_95_ |
| gx_graphiceq.lv2 | Gx_graphiceq_ | http://guitarix.sourceforge.net/plugins/gx_graphiceq_ |
| gx_hfb.lv2 | Gx_hfb_ | http://guitarix.sourceforge.net/plugins/gx_hfb_ |
| gx_hogsfoot.lv2 | Gx_hogsfoot_ | http://guitarix.sourceforge.net/plugins/gx_hogsfoot_ |
| gx_hornet.lv2 | Gx_hornet_ | http://guitarix.sourceforge.net/plugins/gx_hornet_ |
| gx_jcm800pre.lv2 | Gx_jcm800pre_ | http://guitarix.sourceforge.net/plugins/gx_jcm800pre_ |
| gx_jcm800pre_st.lv2 | Gx_jcm800pre_ST | http://guitarix.sourceforge.net/plugins/gx_jcm800pre_st |
| gx_mbcompressor.lv2 | Gx_mbcompressor_ | http://guitarix.sourceforge.net/plugins/gx_mbcompressor_ |
| gx_mbdelay.lv2 | Gx_mbdelay_ | http://guitarix.sourceforge.net/plugins/gx_mbdelay_ |
| gx_mbdistortion.lv2 | Gx_mbdistortion_ | http://guitarix.sourceforge.net/plugins/gx_mbdistortion_ |
| gx_mbecho.lv2 | Gx_mbecho_ | http://guitarix.sourceforge.net/plugins/gx_mbecho_ |
| gx_mbreverb.lv2 | Gx_mbreverb_ | http://guitarix.sourceforge.net/plugins/gx_mbreverb_ |
| gx_mole.lv2 | Gx_mole_ | http://guitarix.sourceforge.net/plugins/gx_mole_ |
| gx_muff.lv2 | Gx_muff_ | http://guitarix.sourceforge.net/plugins/gx_muff_ |
| gx_mxrdist.lv2 | Gx_mxrdist_ | http://guitarix.sourceforge.net/plugins/gx_mxrdist_ |
| gx_oc_2.lv2 | Gx_oc_2_ | http://guitarix.sourceforge.net/plugins/gx_oc_2_ |
| gx_phaser.lv2 | Gx_phaser | http://guitarix.sourceforge.net/plugins/gx_phaser |
| gx_rangem.lv2 | Gx_rangem_ | http://guitarix.sourceforge.net/plugins/gx_rangem_ |
| gx_redeye.lv2 | GxRedeye | http://guitarix.sourceforge.net/plugins/gx_redeye |
| gx_reverb.lv2 | Gx_reverb_stereo | http://guitarix.sourceforge.net/plugins/gx_reverb_stereo |
| gx_room_simulator.lv2 | Gx_room_simulator_ | http://guitarix.sourceforge.net/plugins/gx_room_simulator_ |
| gx_scream.lv2 | Gx_scream_ | http://guitarix.sourceforge.net/plugins/gx_scream_ |
| gx_shimmizita.lv2 | Gx_shimmizita_ | http://guitarix.sourceforge.net/plugins/gx_shimmizita_ |
| gx_sloopyblue.lv2 | Gx_sloopyblue_ | http://guitarix.sourceforge.net/plugins/gx_sloopyblue_ |
| gx_studiopre.lv2 | Gx_studiopre | http://guitarix.sourceforge.net/plugins/gx_studiopre |
| gx_studiopre_st.lv2 | Gx_studiopre_st | http://guitarix.sourceforge.net/plugins/gx_studiopre_st |
| gx_susta.lv2 | Gx_susta_ | http://guitarix.sourceforge.net/plugins/gx_susta_ |
| gx_switched_tremolo.lv2 | Gx_switched_tremolo_ | http://guitarix.sourceforge.net/plugins/gx_switched_tremolo_ |
| gx_tremolo.lv2 | Gx_tremolo | http://guitarix.sourceforge.net/plugins/gx_tremolo |
| gx_w20.lv2 | Gx_w20 | http://guitarix.sourceforge.net/plugins/gx_w20 |
| gx_zita_rev1.lv2 | Gx_zita_rev1_stereo | http://guitarix.sourceforge.net/plugins/gx_zita_rev1_stereo |
| gxautowah.lv2 | GxAutoWah | http://guitarix.sourceforge.net/plugins/gxautowah |
| GxAxisFace.lv2 | Gx_AxisFace_ | http://guitarix.sourceforge.net/plugins/gx_AxisFace_ |
| GxBaJaTubeDriver.lv2 | Gx_bajatubedriver_ | http://guitarix.sourceforge.net/plugins/gx_bajatubedriver_ |
| GxBlueAmp.lv2 | Gx_blueamp_ | http://guitarix.sourceforge.net/plugins/gx_blueamp_ |
| GxBoobTube.lv2 | Gx_boobtube_ | http://guitarix.sourceforge.net/plugins/gx_boobtube_ |
| gxbooster.lv2 | Gxbooster | http://guitarix.sourceforge.net/plugins/gxbooster |
| GxBottleRocket.lv2 | Gx_bottlerocket_ | http://guitarix.sourceforge.net/plugins/gx_bottlerocket_ |
| GxCabSim.lv2 | N/A | N/A |
| GxClubDrive.lv2 | Gx_clubdrive_ | http://guitarix.sourceforge.net/plugins/gx_clubdrive_ |
| GxCreamMachine.lv2 | Gx_CreamMachine_ | http://guitarix.sourceforge.net/plugins/gx_CreamMachine_ |
| GxDenoiser2.lv2 | N/A | N/A |
| GxDistortionPlus.lv2 | N/A | N/A |
| GxDOP250.lv2 | Gx_DOP250_ | http://guitarix.sourceforge.net/plugins/gx_DOP250_ |
| gxechocat.lv2 | GxEchoCat | http://guitarix.sourceforge.net/plugins/gxechocat |
| GxEpic.lv2 | Gx_epic_ | http://guitarix.sourceforge.net/plugins/gx_epic_ |
| GxEternity.lv2 | Gx_eternity_ | http://guitarix.sourceforge.net/plugins/gx_eternity_ |
| GxFenderizer.lv2 | N/A | N/A |
| GxFz1b.lv2 | Gx_maestro_fz1b_ | http://guitarix.sourceforge.net/plugins/gx_maestro_fz1b_ |
| GxFz1s.lv2 | Gx_maestro_fz1s_ | http://guitarix.sourceforge.net/plugins/gx_maestro_fz1s_ |
| GxGuvnor.lv2 | Gx_guvnor_ | http://guitarix.sourceforge.net/plugins/gx_guvnor_ |
| GxHeathkit.lv2 | Gx_Heathkit_ | http://guitarix.sourceforge.net/plugins/gx_Heathkit_ |
| GxHotBox.lv2 | Gx_hotbox_ | http://guitarix.sourceforge.net/plugins/gx_hotbox_ |
| GxHyperion.lv2 | Gx_hyperion_ | http://guitarix.sourceforge.net/plugins/gx_hyperion_ |
| GxKnightFuzz.lv2 | Gx_KnightFuzz_ | http://guitarix.sourceforge.net/plugins/gx_KnightFuzz_ |
| GxLiquidDrive.lv2 | Gx_liquiddrive_ | http://guitarix.sourceforge.net/plugins/gx_liquiddrive_ |
| GxLuna.lv2 | Gx_luna_ | http://guitarix.sourceforge.net/plugins/gx_luna_ |
| gxmetal_amp.lv2 | GxMetalAmp | http://guitarix.sourceforge.net/plugins/gxmetal_amp |
| gxmetal_head.lv2 | GxMetalHead | http://guitarix.sourceforge.net/plugins/gxmetal_head |
| GxMicroAmp.lv2 | Gx_MicroAmp_ | http://guitarix.sourceforge.net/plugins/gx_MicroAmp_ |
| GxOsMutantes.lv2 | N/A | N/A |
| GxOverDriver.lv2 | N/A | N/A |
| GxPlexi.lv2 | Gx_plexi_ | http://guitarix.sourceforge.net/plugins/gx_plexi_ |
| GxPushPull.lv2 | N/A | N/A |
| GxQuack.lv2 | Gx_quack_ | http://guitarix.sourceforge.net/plugins/gx_quack_ |
| GxReverseDelay.lv2 | N/A | N/A |
| GxSaturator.lv2 | Gx_saturate_ | http://guitarix.sourceforge.net/plugins/gx_saturate_ |
| GxSD1.lv2 | Gx_sd1sim_ | http://guitarix.sourceforge.net/plugins/gx_sd1sim_ |
| GxSD2Lead.lv2 | Gx_sd2lead_ | http://guitarix.sourceforge.net/plugins/gx_sd2lead_ |
| GxShakaTube.lv2 | Gx_shakatube_ | http://guitarix.sourceforge.net/plugins/gx_shakatube_ |
| GxSloopyBlue.lv2 | Gx_sloopyblue_ | http://guitarix.sourceforge.net/plugins/gx_sloopyblue_ |
| GxSlowGear.lv2 | Gx_slowgear_ | http://guitarix.sourceforge.net/plugins/gx_slowgear_ |
| GxSunFace.lv2 | Gx_SunFace_ | http://guitarix.sourceforge.net/plugins/gx_SunFace_ |
| GxSuperFuzz.lv2 | Gx_sfp_ | http://guitarix.sourceforge.net/plugins/gx_sfp_ |
| GxSupersonic.lv2 | Gx_supersonic_ | http://guitarix.sourceforge.net/plugins/gx_supersonic_ |
| GxSuppaToneBender.lv2 | Gx_vstb_ | http://guitarix.sourceforge.net/plugins/gx_vstb_ |
| GxSVT.lv2 | Gx_ampegsvt_ | http://guitarix.sourceforge.net/plugins/gx_ampegsvt_ |
| GxSwitchlessWah.lv2 | N/A | N/A |
| gxtape.lv2 | GxTape | http://guitarix.sourceforge.net/plugins/gxtape |
| gxtilttone.lv2 | GxTiltTone | http://guitarix.sourceforge.net/plugins/gxtilttone |
| GxTimRay.lv2 | Gx_timray_ | http://guitarix.sourceforge.net/plugins/gx_timray_ |
| GxToneMachine.lv2 | Gx_tonemachine_ | http://guitarix.sourceforge.net/plugins/gx_tonemachine_ |
| GxToneMender.lv2 | N/A | N/A |
| gxts9.lv2 | Gxts9sim | http://guitarix.sourceforge.net/plugins/gxts9 |
| gxtubedelay.lv2 | GxRedeyeFx | http://guitarix.sourceforge.net/plugins/gxtubedelay |
| GxTubeDistortion.lv2 | Gx_TubeDistortion_ | http://guitarix.sourceforge.net/plugins/gx_TubeDistortion_ |
| gxtubetremelo.lv2 | GxTubeTremelo | http://guitarix.sourceforge.net/plugins/gxtubetremelo |
| gxtubevibrato.lv2 | GxTubeVibrato | http://guitarix.sourceforge.net/plugins/gxtubevibrato |
| GxUltraCab.lv2 | Gx_ultracab_ | http://guitarix.sourceforge.net/plugins/gx_ultracab_ |
| GxUVox720k.lv2 | Gx_uvox_ | http://guitarix.sourceforge.net/plugins/gx_uvox_ |
| GxValveCaster.lv2 | Gx_valvecaster_ | http://guitarix.sourceforge.net/plugins/gx_valvecaster_ |
| GxVBassPreAmp.lv2 | Gx_voxbass_ | http://guitarix.sourceforge.net/plugins/gx_voxbass_ |
| GxVintageFuzzMaster.lv2 | Gx_vfm_ | http://guitarix.sourceforge.net/plugins/gx_vfm_ |
| GxVmk2.lv2 | Gx_vmk2d_ | http://guitarix.sourceforge.net/plugins/gx_vmk2d_ |
| GxVoodoFuzz.lv2 | Gx_voodoo_ | http://guitarix.sourceforge.net/plugins/gx_voodoo_ |
| GxZoom.lv2 | N/A | N/A |
| hip2b.lv2 | the infamous Hip2B | N/A |
| ImpulseLoaderStereo.lv2 | N/A | N/A |
| lcr_delay-swh.lv2 | L/C/R Delay | N/A |
| LittleFly.lv2 | LittleFly_ | https://github.com/brummer10/LittleFly.lv2 |
| lookahead_limiter-swh.lv2 | Lookahead limiter | N/A |
| lowpass_iir-swh.lv2 | Glame Lowpass Filter | N/A |
| lushlife.lv2 | the infamous lush life | N/A |
| mbeq-swh.lv2 | Multiband EQ | N/A |
| mindi.lv2 | the infamous mindi | N/A |
| mod-caps-AmpVTS.lv2 | C* AmpVTS - Tube amp + Tone stack | N/A |
| mod-caps-AutoFilter.lv2 | C* AutoFilter | N/A |
| mod-caps-CabinetIII.lv2 | C* CabinetIII - Idealised loudspeaker cabinet emulation | N/A |
| mod-caps-CabinetIV.lv2 | C* CabinetIV - Idealised loudspeaker cabinet emulation | N/A |
| mod-caps-CEO.lv2 | C* CEO - Chief Executive Oscillator | N/A |
| mod-caps-ChorusI.lv2 | C* ChorusI - Mono chorus/flanger | N/A |
| mod-caps-Click.lv2 | C* Click - Metronome | N/A |
| mod-caps-Compress.lv2 | C* Compress - Mono compressor | N/A |
| mod-caps-CompressX2.lv2 | C* CompressX2 - Stereo compressor | N/A |
| mod-caps-Eq10.lv2 | C* Eq10 - 10-band equalizer | N/A |
| mod-caps-Eq10X2.lv2 | C* Eq10X2 - 10-band equalizer | N/A |
| mod-caps-Eq4p.lv2 | C* Eq4p - 4-band parametric equaliser | N/A |
| mod-caps-EqFA4p.lv2 | C* EqFA4p - 4-band parametric shelving equalizer | N/A |
| mod-caps-Fractal.lv2 | C* Fractal - Audio stream from deterministic chaos | N/A |
| mod-caps-Narrower.lv2 | C* Narrower - Stereo image width reduction | N/A |
| mod-caps-Noisegate.lv2 | C* Noisegate - Attenuate noise resident in silence | N/A |
| mod-caps-PhaserII.lv2 | C* PhaserII - Mono phaser modulated by a Lorenz fractal | N/A |
| mod-caps-Plate.lv2 | C* Plate - Versatile plate reverb | N/A |
| mod-caps-PlateX2.lv2 | C* PlateX2 - Stereo in/out Versatile plate reverb | N/A |
| mod-caps-Saturate.lv2 | C* Saturate | N/A |
| mod-caps-Scape.lv2 | C* Scape - Stereo delay + Filters | N/A |
| mod-caps-Sin.lv2 | C* Sin - Sine wave generator | N/A |
| mod-caps-Spice.lv2 | C* Spice | N/A |
| mod-caps-SpiceX2.lv2 | C* SpiceX2 | N/A |
| mod-caps-ToneStack.lv2 | C* ToneStack - Tone stack emulation | N/A |
| mod-caps-White.lv2 | C* White - White noise generator | N/A |
| mod-caps-Wider.lv2 | C* Wider - Stereo image Synthesis | N/A |
| mod-mda-Ambience.lv2 | MDA Ambience | N/A |
| mod-mda-Bandisto.lv2 | MDA Bandisto | N/A |
| mod-mda-BeatBox.lv2 | MDA BeatBox | N/A |
| mod-mda-Combo.lv2 | MDA Combo | N/A |
| mod-mda-DeEss.lv2 | MDA De-ess | N/A |
| mod-mda-Degrade.lv2 | MDA Degrade | N/A |
| mod-mda-Delay.lv2 | MDA Delay | N/A |
| mod-mda-Detune.lv2 | MDA Detune | N/A |
| mod-mda-Dither.lv2 | MDA Dither | N/A |
| mod-mda-DubDelay.lv2 | MDA DubDelay | N/A |
| mod-mda-DX10.lv2 | MDA DX10 | N/A |
| mod-mda-Dynamics.lv2 | MDA Dynamics | N/A |
| mod-mda-EPiano.lv2 | MDA ePiano | N/A |
| mod-mda-Image.lv2 | MDA Image | N/A |
| mod-mda-JX10.lv2 | MDA JX10 | N/A |
| mod-mda-Leslie.lv2 | MDA Leslie | N/A |
| mod-mda-Limiter.lv2 | MDA Limiter | N/A |
| mod-mda-Loudness.lv2 | MDA Loudness | N/A |
| mod-mda-MultiBand.lv2 | MDA LV2 | N/A |
| mod-mda-Overdrive.lv2 | MDA LV2 | N/A |
| mod-mda-Piano.lv2 | MDA LV2 | N/A |
| mod-mda-RePsycho.lv2 | MDA LV2 | N/A |
| mod-mda-RezFilter.lv2 | MDA LV2 | N/A |
| mod-mda-RingMod.lv2 | MDA LV2 | N/A |
| mod-mda-RoundPan.lv2 | MDA LV2 | N/A |
| mod-mda-Shepard.lv2 | MDA LV2 | N/A |
| mod-mda-Splitter.lv2 | MDA LV2 | N/A |
| mod-mda-Stereo.lv2 | MDA LV2 | N/A |
| mod-mda-SubSynth.lv2 | MDA LV2 | N/A |
| mod-mda-TalkBox.lv2 | MDA LV2 | N/A |
| mod-mda-TestTone.lv2 | MDA LV2 | N/A |
| mod-mda-ThruZero.lv2 | MDA LV2 | N/A |
| mod-mda-Tracker.lv2 | MDA LV2 | N/A |
| mod-mda-Transient.lv2 | MDA LV2 | N/A |
| mod-mda-VocInput.lv2 | MDA LV2 | N/A |
| mod-mda-Vocoder.lv2 | MDA LV2 | N/A |
| mod_delay-swh.lv2 | Modulatable delay | N/A |
| multivoice_chorus-swh.lv2 | Multivoice Chorus | N/A |
| neural_amp_modeler.lv2 | Neural Amp Modeler | http://github.com/mikeoliphant/neural-amp-modeler-lv2 |
| octolo.lv2 | the infamous octolo | N/A |
| phasers-swh.lv2 | LFO Phaser | N/A |
| pitch_scale-swh.lv2 | Higher Quality Pitch Scaler | N/A |
| plate-swh.lv2 | Plate reverb | N/A |
| PowerAmpImpulses.lv2 | PowerAmpImpulses | urn:brummer:PowerAmpImpulses |
| PowerAmps.lv2 | PowerAmps | urn:brummer:poweramps |
| powercut.lv2 | the infamous power cut | N/A |
| powerup.lv2 | the infamous power up | N/A |
| PreAmpImpulses.lv2 | PreAmpImpulses | urn:brummer:PreAmpImpulses |
| PreAmps.lv2 | PreAmps | urn:brummer:PreAmps |
| Ratatouille.lv2 | Ratatouille | urn:brummer:ratatouille |
| rate_shifter-swh.lv2 | Rate shifter | N/A |
| retro_flange-swh.lv2 | Retro Flanger | N/A |
| revdelay-swh.lv2 | Reverse Delay (5s max) | N/A |
| ringmod-swh.lv2 | Ringmod with two inputs | N/A |
| rt-neural-generic.lv2 | AIDA-X | http://lv2plug.in/ns/lv2 |
| satan_maximiser-swh.lv2 | Barry's Satan Maximiser | N/A |
| sc1-swh.lv2 | SC1 | N/A |
| sc2-swh.lv2 | SC2 | N/A |
| sc3-swh.lv2 | SC3 | N/A |
| sc4-swh.lv2 | SC4 | N/A |
| slowmo.lv2 | N/A | N/A |
| stuck.lv2 | the infamous stuck | N/A |
| tap-autopan.lv2 | TAP AutoPanner | N/A |
| tap-chorusflanger.lv2 | TAP Chorus/Flanger | N/A |
| tap-deesser.lv2 | TAP DeEsser | N/A |
| tap-doubler.lv2 | TAP Fractal Doubler | N/A |
| tap-dynamics-st.lv2 | TAP Stereo Dynamics | N/A |
| tap-dynamics.lv2 | TAP Mono Dynamics | N/A |
| tap-echo.lv2 | TAP Stereo Echo | N/A |
| tap-eq.lv2 | TAP Equalizer | N/A |
| tap-eqbw.lv2 | TAP Equalizer/BW | N/A |
| tap-limiter.lv2 | TAP Scaling Limiter | N/A |
| tap-pinknoise.lv2 | TAP Pink/Fractal Noise | N/A |
| tap-pitch.lv2 | TAP Pitch Shifter | N/A |
| tap-reflector.lv2 | TAP Reflector | N/A |
| tap-reverb.lv2 | TAP Reverberator | N/A |
| tap-rotspeak.lv2 | TAP Rotary Speaker | N/A |
| tap-sigmoid.lv2 | TAP Sigmoid Booster | N/A |
| tap-tremolo.lv2 | TAP Tremolo | N/A |
| tap-tubewarmth.lv2 | TAP Tubewarmth | N/A |
| tap-vibrato.lv2 | TAP Vibrato | N/A |
| veja-bass-cab.lv2 | Bass Cabinets | N/A |
| VintageAC30.lv2 | VintageAC30 | https://github.com/brummer10/VintageAC30 |
| VintageTubeOverdrive.lv2 | vintagetubeoverdrive | urn:brummer:vintagetubeoverdrive |
| XDarkTerror.lv2 | X_darkterror_ | http://guitarix.sourceforge.net/plugins/XDarkTerror_ |
| XTinyTerror.lv2 | X_tinyterror_ | http://guitarix.sourceforge.net/plugins/XTinyTerror_ |
<!-- END AUTO-GENERATED LV2 APPENDIX -->
