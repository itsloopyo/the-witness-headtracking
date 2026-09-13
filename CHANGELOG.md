# Changelog

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
