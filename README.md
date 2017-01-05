when-i-get-that-feeling
=======================

What is this?
-------------

For as long as I've owned a Raspberry Pi I've wanted to get a software synth
running on it 'baremetal', as an exercise in music and real-time operating
systems programming.  After a lot of tinkering, I'm getting there!

DEMO: https://youtu.be/o-VpaaMLXDg

Overview
--------

Practical advice for turning a Raspberry Pi into a flexible Linux-based soft
synth is readily available [1].  That's _not_ what this project is about.
Instead, my goal has been to build a dedicated platform for real-time software
synthesis on the Raspberry Pi from scratch, that I understand down to the last
line of code, with these properties:

* pre-emptive real-time scheduling for glitch-free, low-latency synthesis
* USB-MIDI support, so that users need only speakers, their favourite MIDI
  instrument and the Pi

Making it reality:

- A few months ago I wrote a little RTOS with my partner Jacob Carew for the
  University of Waterloo's infamous CS452, and decided it would be useable as a
  starting point for this project so I factored out the platform independent
  bits of it into CaboOSe [2] and ported it to the Raspberry Pi.
- With an RTOS in hand, I next needed a driver for the onboard 3.5mm audio jack.
  After spending a lot of time with the BCM2835 peripherals datasheet and
  drivers from other baremetal projects, I put together a from-first principles
  guide to getting that hardware going and wrote a thoroughly-documented driver
  that I think has a lot of value beyond this individual project [3].
- The other big hurdle was USB-MIDI... it turns out USB (and the host controller
  on the Raspberry Pi in particular) is a bit too tangly to tackle from scratch,
  so I had to compromise on my original line-for-line goal a bit and bring in
  rsta2's USPi.  However,

    - USPi didn't have USB-MIDI support, so I implemented it and contributed it
      upstream [4].
    - USPi processes thousands of IRQs/second, which imposes a substantial
      burden on the application (I measured ~2000 IRQs/second with my USB-MIDI
      class driver at steady state).  To keep interrupt latency on the main
      application core sane, my first thought was therefore to take advantage of
      my Raspberry Pi 2's multiple cores and run USPi on a dedicated core, but
      this turned out to be quite an adventure because of the way the BCM2835
      interrupt controller is set up.  It turns out IRQs can only be routed to
      one core, _but_ FIQs can be routed to a different core :)  So I hacked
      USPi and its portability layer in the project a bit more to use the FIQ
      line, delivering only actual MIDI data (which arrives much less
      frequently) to the main application core via an IPI.
- With audio output and MIDI input, all that's left is the actual synth.  Right
  now all I have is a proof-of-concept that synthesizes a simple square wave,
  since my focus has been almost entirely on the platform.  Going forward, I
  could flesh this out in a few ways:

    1. I could build a simple sample-based synth, like [5].  USPi has USB mass
       storage support so this would still have the potential to be quite
       flexible.
    2. I could pick up a DSP book and implement something more interesting
       myself from scratch.  I know just enough to be dangerous already.
    3. I could go the same route as pitracker [6], which is probably the closest
       project to this one (has everything except for USB-MIDI support).
       pitracker implements part of the LV2 plugin interface, meaning it
       theoretically supports arbitrary plugins.

    Right now I'm leaning toward 3), since what I'd _really_ like is to get
    amsynth running on this platform and amsynth provides an LV2 plugin.

Building
--------

You'll need a cross-compiler/toolchain - I've been using the GNU ARM embedded
toolchain, which is available through the gcc-arm-none-eabi package for Ubuntu.
If you're using a different one, just tweak PREFIX in the Makefile before
building with plain GNU make.

The result of the build is a kernel.img that you can drop on your SD card in
place of the Linux kernel.img.  You'll need to be running relatively recent
firmware (bootcode.bin/start.elf, available from [7]), and must _not_ use the
kernel\_old config.txt option.

NOTE: As I mentioned in the technical description above, this runs only on the
Raspberry Pi 2 as I pin parts of the platform to different cores.  Getting it up
and running on the Raspberry Pi 3 is probably possible with relatively minimal
effort, but I don't have one to test with.

FAQs
----

Q: Why is the kernel train-themed?

A: The CS452 course project I originally wrote the RTOS for is a real-time model
   train control application.

Q: What's with all the references to [8]?

A: I wrote all of the MMU code over the course of an all-nighter hackathon a
   little while ago, during which the Kygo remix was played on repeat for a few
   hours at one point.

Acknowledgements
----------------

Thanks to @rsta2 for USPi, and also for Circle [9], which was an invaluable
reference when writing the peripherals drivers.

[1] http://wiki.linuxaudio.org/wiki/raspberrypi

[2] https://github.com/jtotto/CaboOSe

[3] https://github.com/jtotto/when-i-get-that-feeling/blob/master/audio.c

[4] https://github.com/rsta2/uspi/pull/11

[5] http://www.samplerbox.org/

[6] https://github.com/Joeboy/pixperiments/tree/master/pitracker

[7] https://github.com/raspberrypi/firmware

[8] https://www.youtube.com/watch?v=rjlSiASsUIs

[9] https://github.com/rsta2/circle
