# Changelog

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Changed

- Settings move to `CameraUnlock.ini`, next to `witness64_d3d11.exe`. Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A first start with no `HeadTracking.ini` no longer writes one. It creates `CameraUnlock.ini` instead. The installer ZIP no longer carries a config file, and `uninstall.cmd` leaves `CameraUnlock.ini` and `HeadTracking.ini` in place.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default earlier versions used, because `HeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor is this, where your old file had it:
  - The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default. That is `CollisionEnabled`, the lean clamp that stops a lean at a wall: `CollisionEnabled=false` as earlier versions shipped it is written as `default`, which is on. Set `CollisionEnabled=false` in `CameraUnlock.ini` to turn it off.
- An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. `[Hotkeys] ToggleKey`, `CycleModeKey` and `ToggleYawModeKey`, virtual key codes, are imported into `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`, each as the plain key and the Ctrl+Shift chord earlier versions fixed in code (Y, G and H). The chords can now be rebound too.
- The tracking mode (Page Up / Ctrl+Shift+G) and the yaw mode (Page Down / Ctrl+Shift+H) are saved to `CameraUnlock.ini` when you change them, and the game starts in the modes you left it in. Turning tracking on or off (End / Ctrl+Shift+Y) is not saved; the game starts with head tracking on or off as `EnableOnStartup` says.
- The keys are renamed to the names every head tracking mod on `CameraUnlock.ini` uses: `[Network] UDPPort` is `UdpPort`, `[General] AutoEnable` is `EnableOnStartup`, the lean limits are `PositionLimitX`, `PositionLimitZ` and `PositionLimitZBack`, and `[Collision] CollisionEnabled`, `CollisionMargin` and `CollisionReleaseSmoothing` move to `[Position]`. `LimitY` set both vertical limits, so it becomes `PositionLimitY` (up) and `PositionLimitYDown` (down), both imported from it. `[Position] Enabled`, which chose the mode tracking started in, is imported as that mode: `Enabled=false` starts in rotation only, as it did. `WorldSpaceYaw`, `LogToFile` and `LogDiagnostics` keep their names.
- A value in `CameraUnlock.ini` that is outside a setting's range is no longer pulled to the nearest value the mod accepts. The setting keeps its default and the log names the line. `UdpPort` takes 1 to 65535, where `HeadTracking.ini` held it to 1024 to 65535, the lean limits 0 to 10 metres, where `HeadTracking.ini` held them to 0.01 to 10, and the smoothing values 0 to 1. `CollisionMargin` is still used from 0.06 to 10 metres. The import carries every value exactly as earlier versions used it.

## [0.0.0] - 2026-09-08

First release.

### Added

- Added head tracking to the camera. Rotation and a 6DOF lean are applied to
  the view the frame is drawn from, while the camera the game reads for its
  interaction pick keeps the rotation and position the mouse gave it, so
  looking around never changes what you are pointing at.
- Added reticle compensation. The game's reticle is moved onto the point the
  interaction ray actually reaches, found with the engine's own world ray, so
  it stays on that spot at any distance while the head turns or leans.
- Added field-of-view compensation, so a zoom does not change how far a head
  movement moves the picture.
- Added `[Collision]` keys for clamping a lean against level geometry. Off in
  the shipped configuration until the clamp has been confirmed against real
  walls in game.
- Added a log line for a tracker that goes quiet mid-session, and another for
  when its packets start arriving again, so a session where head tracking
  stopped can be told apart from one where it never started.
- Added `[General] LogDiagnostics`, which periodically writes the pose, the
  rotation and lean applied, and the reticle position, for diagnosing an axis
  that moves the wrong way.
- Added a registry of known builds. The mod matches the running executable
  against it and stays fully dormant, with no hooks installed, on a build it
  does not recognise.
- Added window centering. A windowed game is centered on the work area of the
  monitor it opened on, once its window has settled. A window the game
  centered itself, and a fullscreen or borderless one, are left where they
  are.
- Added `LocalSmoothing` (default 0.0) and `RemoteSmoothing` (default 0.15)
  in `[Smoothing]`, selected per connection from the packet source address,
  so a tracker running on this machine and a remote device on the network can
  be smoothed differently. Both cover rotation and position.
- Added centring in the tracker rather than the mod. The pose is applied as it
  arrives, so centre with opentrack's Center bind or the CENTER button in
  Headcam. There is no recenter hotkey.
- Added `HeadTracking.prev.log`, which keeps the previous launch's log, so a
  crash that is only visible after a relaunch is still diagnosable.
