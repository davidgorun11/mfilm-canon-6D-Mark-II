M-Film for the Canon EOS 6D Mark II (Film OS + Magic Lantern)
==============================================================
A fork of M-Film by HNGSeries, built on Magic Lantern.
For ONE camera and ONE Canon firmware: the EOS 6D Mark II with firmware 1.1.1. Don't put it on
any other camera.

Read this first
  - Magic Lantern on the 6D Mark II (DIGIC 7) is experimental. This build has been tried on one
    camera. You use it at your own risk.
  - Nothing here replaces Canon's firmware. The update in step 3 below only switches the camera
    to start Magic Lantern when the card has it. Without such a card, the camera starts as usual.
  - Copy your photos off the card before you start: step 2 formats it.

What is in here
  ML-SETUP.FIR   Magic Lantern's installer, run once from Canon's menu (step 3)
  autoexec.bin   Magic Lantern
  ML/            its files, and the film OS (ML/modules/filmos.mo), films, fonts, colour tables
  You don't need to install Magic Lantern separately: it is all in this zip.

Install
  1. Check the camera's firmware: MENU > the last settings tab > Firmware ver.
     It must say 1.1.1. If it doesn't, update Canon's firmware to 1.1.1 first.
  2. Format the card in the camera. Then copy everything in this zip to the card's root. The
     card's top level must show autoexec.bin, ML-SETUP.FIR and the ML folder, not a folder with
     them inside (unzipping often makes one).
  3. Put the card in the camera, then MENU > Firmware ver. > update, and follow the screen.
     You do this once per camera, not once per card.
  4. Switch the camera off and on. Press Trash to open the film OS (with or without LiveView).

Update to a new version of this fork
  Don't copy a new version over the old one.
  1. Develop the roll and copy your photos off the card.
  2. Format the card in the camera.
  3. Copy the new zip's files to the card.
  Step 3 of the install isn't needed again.

If something goes wrong
  - The camera doesn't start (black screen, card light stays on): take the battery out, take the
    card out, and put the battery back. The camera then starts normally, without Magic Lantern.
  - Magic Lantern's own menu shows instead of the film OS: ML/modules/filmos.mo didn't load.
    Its line in the Modules tab says why.
  - Logs for a bug report: ML/FILMROLL.LOG on the card. Report problems with this build to the
    fork, not to HNGSeries:
    https://github.com/davidgorun11/mfilm-canon-film-simulation/issues

Remove
  Format the card in the camera, or delete autoexec.bin and ML/. A card without Magic Lantern
  starts the camera as usual.

Prints
  The darkroom develops Small (the screen's 720 x 480), Medium or Large. On this camera one frame
  takes about 10-12 s in Small, 21-25 s in Medium and 36-39 s in Large. LiveView pauses while it
  develops.

Buttons, the same on every screen
  MENU   back (on the home screen: to the camera)
  PLAY   Prints
  Trash  to the camera (in the Darkroom list: deletes the marked rolls)
  INFO   the screen's second action, shown at the bottom
  SET    select
  Half-press the shutter or press LV: to the camera

Free software (GPL-2.0, LICENSE.txt). The source of this build is the 6d2 branch at
https://github.com/davidgorun11/mfilm-canon-film-simulation
The original M-Film is at https://github.com/HNGSeries/mfilm-canon-film-simulation
Other parts and their licences, among them the Film-Luts colours: THIRD_PARTY.txt.
