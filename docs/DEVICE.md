# Putting the engines on your OP-XY

Open-XY builds your own firmware: the SDK links these engines into your
OP-XY's own OS 1.2.0 and checks the result, and you install it from the
device's boot menu. It is unofficial and experimental, and its checks cannot
guarantee a recoverable device. Every step that touches the OP-XY is yours to
do. These steps follow the SDK's guide (`../open-xy/docs/build.md`), which
has the details.

## What you need

- An OP-XY on **OS 1.2.0** exactly. The build accepts nothing else.
- A USB cable, and [OpenMTP](https://openmtp.ganeshrvel.com) on the Mac for
  file transfer. Quit Field Kit while it is open.
- On the Mac, already set up: the SDK beside this folder (`../open-xy`) and
  its Blackfin toolchain (`sdk.py toolchain fetch`).

## 1. Check the engines on the Mac

```bash
python3 tools/test_voices.py
```

```bash
../open-xy/.venv/bin/python ../open-xy/sdk.py compile --machine machines/mdtrx --out local/module-check
```

The compile prints each engine's share of the 2,040 KiB module memory.

## 2. Copy the OS off your OP-XY (once)

1. Switch the OP-XY off. Hold **COM** while switching it on, to open te-boot.
   It must show **3. dev menu**. If it doesn't, enable the dev menu with the
   SDK's browser builder (`../open-xy/docs/builder.md`, "Dev menu").
2. Press **track 3** ("3. dev menu"). Then, on the factory menu, press
   **track 2** ("2. mtp file transfer (mac)") and wait for "mtp mode active".
3. Open the OP-XY in OpenMTP and copy `/upgrade/firmware/blackfin/bin/os.ldr` to
   `~/Library/Application Support/OPXY-SDK/private/stock-1.2.0/os.ldr`
   (the folder is already made). Never copy it into a checkout.
4. Press **track 2** ("2. exit").

If the OP-XY freezes on "mtp mode active" as soon as the Mac connects,
reinstall the official OS 1.2.0 update and try again.

## 3. Build the firmware

```bash
python3 tools/build_firmware.py
```

This builds every engine in `machines/` into a new folder,
`~/Library/Application Support/OPXY-MD/builds/NNN/`, and lists the files to
install. Each build reuses the previous build's `layout.json`, so engines keep
their numbers and saved projects keep their machines. Keep the build folders.
Name engines to build only some: `python3 tools/build_firmware.py mdtrx`.

## 4. Install

1. Open file transfer as in step 2 (COM at power-on, track 3, track 2).
2. Copy the files from the build's `candidate/` folder to the same paths on
   the OP-XY, replacing what is there and keeping everything else:

   | From `candidate/` | To the OP-XY |
   | --- | --- |
   | `os.ldr` | `/os.ldr` |
   | `content/op1_factory.json` | `/content/op1_factory.json` |
   | `content/display/<engine>.svg`, one per engine | `/content/display/<engine>.svg` |
   | `content/presets/<engine>/`, a folder per engine | `/content/presets/<engine>/` |

   The preset folders matter: the OP-XY lists an engine for a track only once
   it has a preset folder, and `op1_factory.json` must be replaced too, or the
   firmware runs but offers none of the new engines.

   Or, with OpenMTP closed, let `python3 tools/install_firmware.py --copy`
   do this step over MTP (`brew install android-file-transfer-linux`): it
   saves the device's own `os.ldr` and `op1_factory.json` in the build's
   `replaced/` folder, copies the latest build (or the build number given)
   and reads every file back to check it. Without `--copy` it only lists
   what it would copy.

3. Press **track 2** ("2. exit").
4. Press **track 5** ("5. write os to boot partition") with the OP-XY on
   power. When "firmware written ok!" appears, switch it off and on.

## 5. Play

Pick a drum track and choose a kit (MD TRX, MD EFM, MD P-I or MD E12), or a
synth track for a Monomachine engine. On a kit each key plays its own drum,
from the drum page's first pad: BD SD HT MT LT CP RS CB CH OH RC CC M1 to M4,
then the variations. Shift and the first encoder choose the selected key's
machine; the encoders hold its controls, with their alternates on press.

## Going back to stock

Reinstall the official OP-XY OS 1.2.0 update the usual way. Tracks that use
these engines can't play them on stock firmware.

## CPU on the device

The CPU figures so far are host estimates. To measure on the OP-XY, follow
the SDK's `docs/profiling.md` with a build made with `--profiling on` (the
default here).
