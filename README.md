# rvTTS

Neural text to speech on a RISC-V microcontroller. Consists of a rule based frontend, a 198k parameter prosody model and a 404k parameter dilated convolutional acoustic model emit the 20 dimensional LPCNet feature vector. Two vocoders consume it. One is a portable FARGAN reimplementation in three C99 files with no Opus build dependency. The other is a classical LPC synthesiser holding no learned parameters, which reaches 83 percent of FARGAN's score for 0.7 percent of the arithmetic.
On a CH32H417 the LPC configuration renders typed text to audio at 1.24 times real time. 

## Results

| Vocoder | SCOREQ | MMAC per second | Code |
| --- | --- | --- | --- |
| FARGAN | 3.744 | 304.7 | 8132 B |
| LPC | 3.172 | 2.01 | 5012 B |

| Build | Total ms per second of audio | RTF |
| --- | --- | --- |
| 150 MHz, soft float | 85348 | 85.3x |
| 150 MHz, hard float | 20016 | 20.0x |
| 300 MHz core | 11260 | 11.3x |
| Plus acoustic weight staging | 3852 | 3.9x |
| Plus prosody weight staging | 1706 | 1.7x |
| Plus int8 activations | 1245 | 1.2x |

The two staging rows change no arithmetic and are bit identical to the code they replace. Together they are worth more than every arithmetic optimisation in the project combined, because the acoustic model had been sweeping 397 KB of weights out of flash once per frame.

## Layout

| Directory | Contents |
| --- | --- |
| `Fargan_x86_Source/` | Host build, FARGAN vocoder |
| `LPC_x86_source/` | Host build, LPC vocoder |
| `TTS_DEMO_DUO/` | CH32H417 dual core firmware, Rovari SDK |


Opus and Xiph files in the fargan source are upstream and unmodified.

## Firmware

`TTS_DEMO_DUO/` is a Rovari Studio project. `app.rova` is the V3F image at 150 MHz, `app_v5f.rova` the V5F image at 300 MHz.

Three things will cost you an afternoon.

The V5F wake address is set in `Link_v5f.ld`, in `targets.py` and in `include/ch32h417_conf.h`. The header is included last and silently wins. A mismatch makes the second core execute a weight blob as instructions, which left this part read protected twice. A compile time check now fails the build instead.

Program the two images in separate debugger invocations. The second command in one session reports a read protect that is not set, after which the debug module returns all ones until a power cycle.

Set the ABI to hard float and delete the object cache afterwards. The cache is keyed on source path, not on compiler flags.

## Hardware

| Function | Wiring |
| --- | --- |
| Console | SERIAL1, TX PA9, RX PA10, 115200 8N1 |
| Audio | DAC channel 1 on PA4, through 1k to the amp, 22n to ground |
| Clock | TIMER1 at 16 kHz, TIMER7 belongs to rovari_tick |
| Card | SPI2, PA12 SCK, PC1 MOSI, PC2 MISO, PC3 CS |

## Flash

Two 480 KB banks with the boundary at 0x00078000. An image may not live in bank 1 or cross the boundary, so the 715 KB of weights are written as data rather than linked.

| Address | Contents | Size |
| --- | --- | --- |
| 0x00000000 | V3F code | 56 KB |
| 0x0000E000 | Prosody weights | 196 KB |
| 0x00040000 | V5F code | 64 KB |
| 0x00050000 | Word vocabulary | 109 KB |
| 0x00078000 | Acoustic weights, bank 1 | 397 KB |
| 0x000DC000 | Manifest | 8 KB |

Put the blobs and the packed dictionary on an SD card. First boot programs them into flash and writes a manifest of address, length and CRC32. Later boots check the three checksums and skip provisioning. First boot costs 4.1 seconds. The 1373 KB dictionary stays on the card and costs one read of a few hundred bytes per word.

## Kernel costs

Retired instruction counts under QEMU with `-icount shift=0`, reproduced on a second toolchain to within a third of one percent.

| Kernel | rv32imac | rv32imafc |
| --- | --- | --- |
| int8 weights, float32 activations | 229.2 | 7.06 |
| int8 weights, int32 accumulator | 7.06 | 7.06 |
| tanh, 5 term polynomial and a divide | 1172 | 11 |

Swapping the float accumulator for int32 is worth a factor of 32 on a core with no FPU and is bit exact, so one numeric path serves both target classes.

## Limits

This is one part, with one configuration, and no FARGAN build on hardware. SCOREQ over three sentences and DNSMOS over five, enough to order configurations that differ widely and nothing finer. Instruction counts overpredicted two of the five hardware wins by factors of 7 and 1.1, since they cannot see a weight fetch. Text normalisation is unrepaired and the system is single speaker.

## Licence

Apache 2.0, copyright Armstrong Subero, for the pipeline, vocoder, frontend and firmware.

`fargan_source/` also carries upstream Opus Audio Codec files by Xiph.Org under the 3 clause BSD licence, unmodified, with their headers intact. FARGAN network and weights are copyright Amazon. The CH32H417 vendor headers and startup code are copyright Nanjing Qinheng Microelectronics.

The preprint in the repo is not Apache 2.0. Copyright Armstrong Subero, all rights reserved. You may read it and cite it. You may not redistribute it, host a copy of it, or use it as training data.

NOTE: "Opus" here in the paper and source means the audio codec, RFC 6716, not any language model. 
