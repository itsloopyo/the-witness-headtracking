# The Witness Head Tracking

![The Witness running with this mod](https://raw.githubusercontent.com/itsloopyo/the-witness-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for The Witness that moves the view with
your head while your mouse or controller keeps control of look and
interaction, driven by a webcam, phone, or any OpenTrack compatible
tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking moves the view; the mouse or controller still points and interacts
- **6DOF positional tracking** - lean and peek with head position
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [The Witness](https://store.steampowered.com/app/210970/The_Witness/)
  (Steam build, 64-bit `witness64_d3d11.exe`).
- A tracking source that sends the OpenTrack UDP protocol to
  `127.0.0.1:4242`. [OpenTrack](https://github.com/opentrack/opentrack/releases)
  itself covers webcams and most tracking hardware.
- Windows 10 or 11, 64-bit.

## Installation

### Standalone Installer

1. Download the latest installer ZIP from the
   [Releases page](https://github.com/itsloopyo/the-witness-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`. It finds The Witness through Steam.
4. Configure OpenTrack to output UDP to `127.0.0.1` port `4242`
   (see [Setting Up OpenTrack](#setting-up-opentrack)).
5. Launch the game.

If the installer cannot find your copy, point it at the game folder
yourself, either with an argument:

```powershell
install.cmd "D:\Games\The Witness"
```

or with an environment variable:

```powershell
$env:THE_WITNESS_PATH = "D:\Games\The Witness"
.\install.cmd
```

### Manual Installation

The installer ZIP carries `openvr_api.dll` and `HeadTracking.ini` under
`plugins\`. Put both into the game folder next to `witness64_d3d11.exe`.
Rename the `openvr_api.dll` already there to `openvr_api.dll.backup` before
you copy ours over it: the shim chains through that backup to reach the real
OpenVR entry points, and overwriting it without the rename leaves nothing to
chain to.

Mod managers do not deploy this mod, so there is no Nexus ZIP. Vortex has no
extension for The Witness, so it cannot manage the game at all, and its one
generic route for a DLL in the game folder only recognizes an archive
containing `dinput8.dll`. Neither Vortex nor Mod Organizer 2 renames a file
the game already ships, which this mod needs. Use `install.cmd`, or the
manual steps above.

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimeters, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.
OpenTrack's **UDP over network** output sends exactly this, and the steps below
set it up.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

### VR Headset Setup

1. Get the headset running in SteamVR, however it normally connects to your
   PC.
2. Start SteamVR.
3. Set OpenTrack's **Input** to the SteamVR tracker.
4. Leave **Output** on **UDP over network**, host `127.0.0.1`, port `4242`.

### Webcam Setup

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam, with no
markers and no IR hardware. Select it under **Input**, pick your camera in its
settings, and use the output settings above. How well it tracks depends on your
camera and your lighting, so try it before buying anything.

### Phone App Setup

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the datagram described above. Point it at this PC's IP address (run `ipconfig`
to find it) on port `4242`. Not every phone tracker speaks this protocol, so
check yours for an OpenTrack or UDP output option first. I made
[Headcam](https://headcam.app) so decent tracking is free for anybody with a
phone already in their pocket; it filters on-device, so it can send direct.

On-device filtering is what decides the wiring. The mod's smoothing is sized to
take the edge off a clean signal rather than to rescue a noisy one, so a raw or
lightly filtered feed sent direct will jitter. Test it: send direct, hold your
head still, and if the view drifts or shakes, point the app at OpenTrack's
**UDP over network** *input* on some other port, say 5252, and let OpenTrack's
filters and curves clean it up before its output forwards to `127.0.0.1:4242`.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

### Centering

Centering belongs to your tracker. The mod applies the pose it receives exactly
as it arrives, so a stream of zeros holds the view where the game itself puts
it. Press the center control in your tracker, either OpenTrack's **Center** bind
or the CENTER button in Headcam, and the view is centered.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Page Down` / `Ctrl+Shift+H` switches yaw mode. Horizon-locked (the
default) turns the head about the world's vertical, so looking down a
slope and turning your head sweeps the horizon. Camera-local turns it
about the camera's own up axis instead, which tips the horizon at steep
pitches.

## Configuration

`HeadTracking.ini` lands next to the game executable, alongside
`witness64_d3d11.exe`, on first install. Edit it and relaunch the game. An
update leaves whatever you tuned in place.

```ini
[Network]
; The tracker port. Listening happens on every interface, so a tracker on
; another device on your network can reach it.
UDPPort=4242

[Smoothing]
; Smoothing applied when the tracker runs on this machine (loopback).
; 0 = no smoothing, 1 = heavy. Covers rotation and position.
LocalSmoothing=0.0
; Smoothing applied when the tracker is a remote device on the network.
; 0 = no smoothing, 1 = heavy. Covers rotation and position.
RemoteSmoothing=0.15

[Position]
; The pose is used exactly as the tracker sends it. There is no sensitivity or
; axis inversion here for rotation or position - set those in opentrack or your
; phone app and one profile then behaves the same in every game.
Enabled=true
; How far the eye may leave the body, in metres. Forward gets more room than
; back, which is what stops a backward lean reaching the inside of your head.
LimitX=0.30
LimitY=0.20
LimitZ=0.40
LimitZBack=0.10

[Collision]
; Stops a lean putting the view inside a wall. Off until the clamp has been
; confirmed engaging on real geometry in game.
CollisionEnabled=false
; Metres held off a blocking surface. Must be above the near clip (0.06), or the
; wall is culled and you see through it anyway. Keep it below the Position
; limits above: at or over one of them, a wall the sweep finds cancels that lean
; outright instead of shortening it.
CollisionMargin=0.15
; How fast the lean reopens once an obstruction clears. 0.9 is 200ms.
CollisionReleaseSmoothing=0.9

[Hotkeys]
; Virtual key codes (hex). End=0x23, PageUp=0x21, PageDown=0x22.
; Ctrl+Shift+Y/G/H chord alternatives are also registered.
ToggleKey=0x23
CycleModeKey=0x21
ToggleYawModeKey=0x22

[General]
AutoEnable=true
LogToFile=true
; Yaw mode: true = horizon-locked yaw, false = camera-local.
; Page Down / Ctrl+Shift+H switches it while the game runs.
WorldSpaceYaw=true
; Writes the pose, the rotation and lean applied, and the reticle position
; about twice a second. For diagnosing a wrong axis.
LogDiagnostics=false
```

With `LogToFile=true` the mod writes `HeadTracking.log` next to the game
executable. It is rewritten from scratch on every launch, and the previous
launch is kept as `HeadTracking.prev.log`, so it is safe to attach as-is when
reporting an issue. Look for `UDP: First UDP packet received` to confirm your
tracker's data is reaching the game.

## Troubleshooting

- **Mod not loading.** Check that `HeadTracking.log` exists next to the
  game executable. If it does not, the DLL never loaded - verify
  `openvr_api.dll` is the shipped one and that `openvr_api.dll.backup`
  exists. `openvr_api_diag.txt` next to the game executable records the
  load attempt itself and is also rewritten on every launch.
- **No tracking response.** The log line `UDP: First UDP packet received`
  is the test. If it is missing, your tracker's data is not reaching the
  mod: check that OpenTrack is started, that its **Output** is **UDP over
  network** on `127.0.0.1:4242`, and that `[Network] UDPPort` matches.
  Tracking is also off if you have pressed `End` / `Ctrl+Shift+Y`, or if
  `[General] AutoEnable` is `false`.
- **Jittery or unstable tracking.** Raise the smoothing key that matches
  your tracker: `LocalSmoothing` for a tracker on this machine,
  `RemoteSmoothing` for one on the network. A phone over WiFi usually
  wants 0.15 or higher. A raw phone feed sent direct is better fixed by
  routing it through OpenTrack's filters, as described above.
- **Wrong rotation axis.** Set `[General] LogDiagnostics=true` and read
  the pose and the applied rotation in `HeadTracking.log` to see which
  axis is moving. There is no inversion setting here: the pose is used
  exactly as it arrives, so an axis that runs backwards is corrected in
  the tracker - opentrack's Mapping tab, or your phone app's own axis
  settings - and one profile then behaves the same in every game.
- **Game crashes on launch.** Run `uninstall.cmd` and try again - the
  installer keeps the original VR DLL as `.backup` and the uninstaller
  restores it.
- **Yaw feels wrong when looking up or down at extreme angles.** Try
  toggling between horizon-locked and camera-local yaw with `Page Down`.
  Horizon-locked (the default) turns the head about the world's vertical;
  camera-local follows the camera's current up axis.
- **The game window moved when I launched.** By design, and only when you
  play windowed: once the game has finished placing its window, the mod
  centers it on the work area of the monitor it opened on. A window the
  game centered itself, and a fullscreen or borderless one that already
  fills the screen, are left where they are.

## Updating

Download the new release and run `install.cmd` again. Your config is
preserved, and the original VR DLL backup is left untouched.

## Uninstalling

Run `uninstall.cmd`. It removes the mod DLL, restores the game's original
`openvr_api.dll` from the backup, and clears `HeadTracking.ini` and the log
files. `uninstall.cmd /force` removes a mod loader the installer did not put
there; this mod installs no loader, so the flag makes no difference here.

## Building from Source

Requires Visual Studio 2022 or newer (with C++ build tools), CMake 3.20+, and
[pixi](https://pixi.sh).

```powershell
git clone --recursive https://github.com/itsloopyo/the-witness-headtracking
cd the-witness-headtracking
pixi run build-release
pixi run package
```

`pixi run test` builds and runs the unit tests, which cover the pure maths
the camera hook sits on: the tracker-to-engine axis signs, the rotation
composition, the horizon-locked lean basis, the field-of-view compensation
and the reticle solve.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- The Witness developed by Jonathan Blow / Thekla, Inc.
- Built on [OpenTrack](https://github.com/opentrack/opentrack) (protocol)
  and [MinHook](https://github.com/TsudaKageyu/minhook) (hooking).
- The `openvr_api.dll` this mod installs is our own forwarder. It declares the
  exports of Valve's [OpenVR](https://github.com/ValveSoftware/openvr) API and
  passes every call to the game's original DLL, and it carries no OpenVR code.
- Shared tracking core: [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core).
- See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for full attributions.


## Disclaimer

This is an unofficial mod. It is not made by, endorsed by, or affiliated with
Thekla, Inc. The Witness is their work and their trademark; this mod only
identifies the game it applies to. It needs a legitimately purchased copy, it
changes nothing on disk beyond its own files and the renamed VR DLL the
uninstaller puts back, and it is provided as is under the MIT licence in
LICENSE.
