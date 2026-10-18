# Cut-A-Dor

**The dirty little beat dojo. Free M-VAVE FM-1 firmware by Kid Bunka.**

Dusty drums. Funky pockets. Eight recordable punches. A small box with terrible manners.
The name comes from **Matador**: the matador of the crossfader. No bulls required.

[Play the browser editor](https://cut-a-dor.sugarypixie10.chatgpt.site/dojo.html) · [Release website](https://cut-a-dor.sugarypixie10.chatgpt.site/) · [Install / Recovery](https://cut-a-dor.sugarypixie10.chatgpt.site/install/) · [Download release](https://github.com/kidbee85-beep/cut-a-dor-release/releases/tag/v1.2.1)

## Play, remix, share

The website is public, with no account required. The source is public too: use it,
change it, fork it and redistribute it under GPL-3.0-only, keeping the corresponding
source and licence when distributing covered software. Component licences and
copyright notices remain in place. Samples are CC0; fonts have their own licences.
The bundled manufacturer recovery image remains M-VAVE software, not GPL code.

## What's in the dojo?

- 76 factory sounds: 64 drum one-shots and 12 original break arrangements.
- 48 additional recorded hits in three separate 80 KiB expansion banks.
- Six styles, sixteen rhythm families, swing, human timing, evolving variation,
  per-lane generation locks, A/B parts, intro, drop, outro and fills.
- Eight lanes and 32 editable steps, velocity and timing nudges, eight lane levels.
- Roll, stutter, reverse, brake, filter, echo, crush and gate. Record and overdub
  a 1/2/4/8-bar FX tape, bypass it or undo the latest take.
- Low/high-pass filters, dust, noise, punch and a simple bass voice.
- DJ, Drum and Break keyboard modes. Hold a page button for on-screen help.
- SETUP controls active key lights, idle glow, motion and the welcome splash.
- USB MIDI clock output with Start/Stop/Continue; USB or jack MIDI input.
- Browser editor: real audio engine, step editing, mixer, FX tape, project files,
  WAV bounce, WAV import and custom sound-bank assembly.

## Browser editor

Open the hosted editor and press **START BEAT**. Nothing connects to a device
or installs firmware automatically. Chrome or Edge on desktop is recommended.
The site can also launch the editor directly inside the release page.

Save `.cadp` projects and `.cadb` audio banks separately: projects do not embed audio.
Sound Crate can import PCM WAV files, convert to mono 16 kHz, trim, audition, assign
sounds, check the 80 KiB budget, and export banks. The three curated banks are ready
to audition. WAV bounce exports 2/4/8 bars with the recorded FX tape.

Connecting an FM-1 is optional and explicit. USB tools upload sample banks and
export the device project. Browser patterns cannot be sent live to the FM-1, and
this firmware protocol has no project-upload command. Disconnect the editor before
opening the installer. Multiple editor/installer tabs cannot claim the device together.

## Install and recover

Release **1.2.1**, expected identity **FM-1_7010201**.
Start from **M-VAVE Original V15 / FM-1_015**. No intermediate custom firmware is
required. Read the [quick start](dist/downloads/QUICKSTART.txt), download the recovery
kit first and follow the [installer](https://cut-a-dor.sugarypixie10.chatgpt.site/install/).

Firmware SHA-256: `5d4be92061cc5bf1396d3ff599ac3a1ef9ec1261afe003b74e32f4298307f614`.
The released firmware is the unchanged owner-tested image. Automated test results
and the limits of hardware coverage are in [TEST-REPORT.txt](dist/downloads/TEST-REPORT.txt).
Recovery includes the official Original V15 image and normal browser restoration;
the separate Mac chip rescue tool is a last resort. Recovery is not a backup of
personal music and cannot guarantee recovery from every hardware failure.

## Controls

HOME opens Live / DJ. EDIT opens MIX, then Beat DNA. SEQUENCE opens Step Dojo,
then Parts. FX opens Punch, then Tape Color. ENV opens FX Tape; LFO opens Crate.
GLO opens SETUP, then MIDI Sync. SAVE stores the project while stopped. Holding
page buttons for 0.7 seconds displays the corresponding control card.

DJ white keys: Roll, Stutter, Reverse, Brake, Filter, Echo, Crush, Gate,
Lock, New, Fill, A, B, Drop, Undo, Clean. First eight black keys mute eight lanes;
last three are Tap, Tape and Undo. In Step Dojo, white keys edit sixteen steps,
black keys 1–8 select lanes. Follow the four labelled knobs on the display.

## What it absolutely cannot do

No microphone recording. No Bluetooth teleportation. No independent time stretching.
No MIDI note output or full song workstation. No turntable included. No tax returns.
Breaks change pitch with speed. Two FM-1s need a USB MIDI host/router to connect;
the jack is input only. Your scratching skills remain entirely your responsibility.

## Build and contribute

The complete editable firmware, shared DSP, browser editor, sample inputs, fonts,
generated headers, tests and build scripts are included. See [BUILDING.txt](docs/BUILDING.txt).
Device builds use the official JieLi toolchain and AC79 SDK, obtained separately
from upstream. Host tests require a C11 compiler, Python and Node; sample/font generation
also needs Pillow. Browser builds use the official WASI SDK. No build script flashes a device.

For local play: `python3 -m http.server 8768 --directory dist`, then open
`http://127.0.0.1:8768/` in Chrome or Edge. Run `sh tools/test_all.sh` for the main
suite. The installer DOM simulation additionally uses the `linkedom` test dependency.

Forks and pull requests are welcome. Describe audible/visible changes, include tests
where useful, and keep the FM-1 recovery paths intact. Please credit contributors.

## Credits

GPL-3.0-only code; retained upstream lineage and notices are documented in
[UPSTREAM-LICENSING.md](UPSTREAM-LICENSING.md). Audio evidence and credits are in
[sound-src](sound-src/) and [SOURCES.txt](dist/SOURCES.txt). Fonts retain their
licence files. Independent project; not endorsed by M-VAVE or upstream developers.
