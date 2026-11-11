# Testing M-Film on your camera

M-Film was made on the **Canon EOS M**. The same code **builds** for 15 more cameras (100D, 650D,
700D, 6D, 70D, 5D Mark III on two firmwares, 550D, 600D, 60D, 1100D, 1300D, 500D, 50D, 5D Mark II), and
testers have tried most of them: the [camera table](README.md#cameras) says how far each one got and
what's still wrong. If yours needs a first tester, or has a known problem you can check, you can help:
it takes about 20 minutes, and you can always go back.

> **Is it safe?** Magic Lantern runs from the card and changes nothing permanent in the camera.
> The worst that usually happens is that the camera doesn't start with the card in: take the
> battery and the card out, and it starts normally again. Still, this is untested software on your
> camera, at your own risk. Keep a fully charged battery and don't try it in the middle of a shoot.

## 1. Get the files

From the [latest release](../../releases/latest) (or the newest run under *Actions*), download
`M-Film-camera-<your camera>.zip`:

<!-- cameras:start -->
| Camera | Canon firmware it needs | File |
|---|---|---|
| Canon EOS M | 2.0.2 | `M-Film-camera-EOSM.202.zip` |
| Canon EOS 5D Mark II | 2.1.2 | `M-Film-camera-5D2.212.zip` |
| Canon EOS 5D Mark III (firmware 1.1.3) | 1.1.3 | `M-Film-camera-5D3.113.zip` |
| Canon EOS 5D Mark III (firmware 1.2.3) | 1.2.3 | `M-Film-camera-5D3.123.zip` |
| Canon EOS 6D | 1.1.6 | `M-Film-camera-6D.116.zip` |
| Canon EOS 6D Mark II | 1.1.1 | `M-Film-camera-6D2.111.zip` |
| Canon EOS 50D | 1.0.9 | `M-Film-camera-50D.109.zip` |
| Canon EOS 60D | 1.1.1 | `M-Film-camera-60D.111.zip` |
| Canon EOS 70D | 1.1.2 | `M-Film-camera-70D.112.zip` |
| Canon EOS 100D / Rebel SL1 / Kiss X7 | 1.0.1 | `M-Film-camera-100D.101.zip` |
| Canon EOS 500D / Rebel T1i / Kiss X3 | 1.1.1 | `M-Film-camera-500D.111.zip` |
| Canon EOS 550D / Rebel T2i / Kiss X4 | 1.0.9 | `M-Film-camera-550D.109.zip` |
| Canon EOS 600D / Rebel T3i / Kiss X5 | 1.0.2 | `M-Film-camera-600D.102.zip` |
| Canon EOS 650D / Rebel T4i / Kiss X6i | 1.0.4 | `M-Film-camera-650D.104.zip` |
| Canon EOS 700D / Rebel T5i / Kiss X7i | 1.1.5 | `M-Film-camera-700D.115.zip` |
| Canon EOS 1100D / Rebel T3 / Kiss X50 | 1.0.5 | `M-Film-camera-1100D.105.zip` |
| Canon EOS 1300D / Rebel T6 / Kiss X80 | 1.1.0 | `M-Film-camera-1300D.110.zip` |
<!-- cameras:end -->

The firmware must match exactly (MENU → last settings tab → *Firmware ver.*).

**One file a camera (since 0.2).** The film OS is a module that loads after the camera has started
(`ML/modules/filmos.mo`), on every camera: Magic Lantern then takes about as much of the camera's memory
at power on as it does alone ([MEMORY.md](ml-dev/MEMORY.md)). The 100D, 70D, 550D, 600D, 1100D, 650D and
700D have run it this way since 0.1.5. On the EOS M, 6D, 5D Mark III, 60D, 500D, 50D and 5D Mark II the
film OS was built into Magic Lantern until 0.1.6 (their module build was the `-module.zip`), so on those
0.2 is the first release that loads it: **say whether it starts**, even if all you can say is "it works".
If the film shelf doesn't appear and you see Magic Lantern's own menu instead, the module didn't load:
its line in the *Modules* tab says why. Tell us what it says.

## 2. Install

1. Format a card **in the camera**, then copy everything from the zip to the card's root.
2. Camera off, card in, camera on. MENU → *Firmware ver.* → update (first time only: this lets the
   camera start Magic Lantern from the card).
3. Switch the camera off and on.
4. In Canon's menu set **Image quality** to **RAW** (RAW+JPEG works too) and **Image review** to
   **Off**. The in-camera darkroom develops the RAWs: frames shot as JPEG can't be developed.

### Canon EOS 1300D

The 1300D has no `ML-SETUP.FIR`: Magic Lantern doesn't make one for it. Step 2 above is done from a
computer instead, with [mlinstall](https://github.com/petabyt/mlinstall/releases) (free, GPL), as Magic
Lantern's developers suggest ([magiclantern_simplified#325](https://github.com/reticulatedpines/magiclantern_simplified/issues/325)).
Magic Lantern's 1300D port is new (October 2026) and marked experimental by its authors.

1. Format a card in the camera and copy the zip's files to it, as in step 1.
2. Camera on, connected to the computer by USB. In mlinstall, wait until it finds the camera, then
   **Enable Boot Disk** (*Enable Custom Boot* in some versions). This tells the camera to start Magic
   Lantern from a card. It doesn't need administrator rights.
3. Camera off, card in the computer's reader. In mlinstall's card tab, **Write card boot flags**
   (`EOS_DEVELOP` and `BOOTDISK`). This one needs administrator rights (Windows: *Run as
   administrator*; Mac and Linux: `sudo`). It writes to the first card called `EOS_DIGITAL` it finds:
   have only that card plugged in.
4. Card back in the camera, camera on, then step 4 above.

Good to know before you start:
- **Two firmwares called 1.1.0.** Canon shipped the 1300D's 1.1.0 in two builds, and Magic Lantern may
  not tell them apart ([magiclantern_simplified#310](https://github.com/reticulatedpines/magiclantern_simplified/issues/310)).
  If the camera hangs at power on: battery out, put the old card back. Tell us either way: `ML/FILMROLL.LOG`
  and Magic Lantern's crash log, if there is one, say which build yours is.
- **Loading a roll doesn't change the camera's settings here.** Magic Lantern can't change ISO, white
  balance or picture style on the 1300D yet, so set ISO yourself. The film's look comes when the roll is
  developed, in the camera or in Darkrooms.
- **No trash button:** a short press of **Av** opens Home.
- **Don't add Magic Lantern's RAW video modules** (`mlv_lite` and the like) to the card: Magic Lantern
  has reports of them damaging 1300Ds for good
  ([magiclantern_simplified#199](https://github.com/reticulatedpines/magiclantern_simplified/pull/199)).
  The M-Film zip leaves them out.

## 3. What to try, and what to tell us

Go through these and note what happens (a photo of the screen helps a lot):

- [ ] The camera starts, and the **film shelf** appears (on a DSLR, from the normal shooting screen
      with LiveView off; give it up to 15 seconds).
- [ ] **Colours** look right (shelf canisters: Portra yellow, Ektar red; Settings → Advanced → Screen colour has a chart).
- [ ] **Buttons**: the dial and arrows move along the shelf, SET loads a film, MENU goes back,
      half-shutter returns to the camera. Which buttons do nothing?
- [ ] **Trash** (a short Av press on the 1300D) opens Home; **PLAY** opens the prints instead of Canon's playback.
- [ ] Load a roll and shoot a few frames **through the viewfinder**, and a few in **LiveView**.
      Does the frame count go up? Is the HUD drawn in LiveView?
- [ ] *Settings → Viewfinder*: try **Minimal**, **Rangefinder** and **Clean**. Is each drawn whole, and does
      the one before it leave nothing behind? Is the **battery** beside the clock right (a percentage on the
      60D, 6D, 70D, 5D Mark II and III; an icon on the others)?
- [ ] With a manual (adapted) lens, turn off *Settings → Exposure simulation*. Is LiveView bright enough
      to focus? Does the HUD's meter needle still move when you half-press?
- [ ] Finish the roll and **develop it in the camera** (Darkroom → INFO): once **Small**, once **Medium** or
      **Large** (delete the prints in between: ◀ → *Prints only*). Do prints come out at the size you chose,
      or does the camera say *Not enough memory* and make a smaller one? How long per frame at each size?
      `ML/FILMROLL.LOG` has a `dev: frame N: read ... ms, ... big print ... ms` line for each: please attach it.
- [ ] In the prints, open a frame developed Medium or Large and turn the **wheel**: does it zoom, and do the
      arrows move about? While developing, **INFO** dims the screen: does any button bring it back, and is
      the screen as bright as before once it is done?
- [ ] In the Darkroom, **◀** on a developed roll: *Delete roll* → *Prints only* → *Delete*. On a computer:
      are its JPEG folder in `DCIM` and `ML/DARKROOM/Rnnn` gone, with no empty folder left?
- [ ] Open the card in **Darkrooms** on a computer: are the rolls and films there?

Then [open an issue](../../issues/new/choose) with: your camera and firmware, what worked, what didn't,
and the file **`ML/FILMROLL.LOG`** from the card attached.

### If *Firmware ver.* says "Memory card containing firmware is required to update"

The camera found no update file it accepts on the card:
- The Canon firmware isn't exactly the one in the table: `ML-SETUP.FIR` only works on that version.
- `ML-SETUP.FIR` isn't at the top of the card. Unzipping on a Mac, or with Windows' *Extract All*, often
  makes a folder (`M-Film-camera-EOSM.202/`). The card's top level should show `autoexec.bin`,
  `ML-SETUP.FIR` and `ML`.
- On a Mac, hidden `._` files next to them can trip the camera. With the card in the reader, run
  `dot_clean -m /Volumes/EOS_DIGITAL` in *Terminal* (your card's name in place of `EOS_DIGITAL`),
  then eject the card before taking it out.

### If the camera says "ML directory not found"

Magic Lantern's installer (`ML-SETUP.FIR`, run by *Firmware ver.* → update) prints this, with
"Please copy all ML files.": it found `autoexec.bin` at the top of the card, but no `ML` folder next to
it ([#42](../../issues/42)). On a computer, the card's top level should look like this, with no folder
around it:

```
autoexec.bin
ML-SETUP.FIR
ML/
```

- Unzipping often makes a folder (`M-Film-camera-6D.116/`; Windows' *Extract All* does too). Copy what
  is inside it, the `ML` folder included, not the folder itself.
- Format the card **in the camera** first, not on the computer, then copy the files onto it.
- A card bigger than 32 GB is formatted as exFAT. If a big card keeps saying this, a card of 32 GB or
  less is a good second try.

### If Canon's screen flickers over M-Film's

Some DSLRs keep drawing their own screen over M-Film's (the top plate, the HUD or a film OS page).
M-Film puts its own back as soon as it sees that. With **Settings → Advanced → Debug log** on, `ML/FILMROLL.LOG`
says what it saw. A line with "seen N times so far" is written the 1st, 51st, 101st... time: N counts
the times, not the lines.
- `hud: the plate: Canon drew over rows 22-60 (39 of them), put back; seen 51 times so far, logged every 50th`
  (`film os:` on a film OS page).
- `Canon took its screen back, held again`: Canon let go of the hold M-Film keeps on its screen.
- `Canon took its screen back 5 times in 3 s: this camera doesn't keep the hold, not taking it again
  until a restart`: on this camera the hold never sticks (the 100D, #73), so from here M-Film works as
  0.1.5 did: it only puts back what Canon draws over.
- `screen: Canon put its other buffer on the screen`: Canon showed its own copy of the screen in
  front of M-Film's, rather than drawing over it.
- `film os: open; Canon GUI mode 2 (asked 2) after 60 ms, hold taken`: how the film OS opened. A mode
  other than the one asked means Canon kept its own screen, and its buttons.
- `drawn over for 2 s on end ..., not checking for 5 s`: Canon draws there all the time, and M-Film
  stops putting its screen back for a while.

Attach the log and say when the flicker happens: when the camera is idle, when the meter moves,
after each shot, or when you switch screens.

### If you turned on Magic Lantern's console

The MENU button opens M-Film, so Magic Lantern's menu (where the console is switched off) can't be
reached while M-Film runs. On a computer, open `ML/SETTINGS/magic.cfg` on the card and change
`module.console = 1` to `module.console = 0`.

### If it doesn't start

Battery out, card out. Then please tell us, because it most likely means the program is too big for
that camera's memory (on the EOS M the limit is about 727 KB at boot; with the film OS as a module
Magic Lantern takes 490–575 KB, and most cameras' limits aren't known yet). Say
which camera and firmware, and whether the card light stayed on.

## 4. Remove it

Format the card in the camera. To stop the camera looking for Magic Lantern at all, run
`ML-SETUP.FIR` again from *Firmware ver.* and choose to remove the boot flag. On the 1300D, connect it to
mlinstall and **Disable Boot Disk**.

Thank you! Every camera someone tries moves it from *testing* to *working* in the
[list on the front page](README.md#cameras).
