# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic
Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- When a plugin fails to load or when the Wine plugin host process fails to
  start, yabridge will now show you the error in a desktop notification instead
  of only printing it to the logger. This makes diagnosing issues much faster if
  you didn't already start your DAW from a terminal. These notifications require
  `libnotify` and the `notify-send` application to be installed.
- Added an environment variable to disable the watchdog timer. This allows the
  Wine process to run under a separate namespace. If you don't know that you
  need this, then you probably don't need this!
- Added support for building 32-bit versions of the yabridge libraries, so you
  can use both 32-bit and 64-bit Windows VST2 and VST3 plugins under 32-bit
  Linux plugin hosts. This should not be necessary in any normal situation since
  Desktop Linux has been 64-bit only for a while now, but it could be useful in
  some very specific situations.

### Changed

- The audio processing implementation for both VST2 and VST3 plugins has been
  completely rewritten to use both shared memory and message passing to reduce
  expensive memory copies to a minimum. With this change the DSP load overhead
  during audio processing should now be about as low as it's going to get.
- Optimized the management of VST3 plugin instances to reduce the overhead when
  using many instances of a VST3 plugin.
- Slightly optimized the function call dispatch for VST2 plugins.
- Prevented some more potential unnecessary memory operations during yabridge's
  communication. The underlying serialization library was recreating some
  objects even when that wasn't needed, which could in theory result in memory
  allocations in certain situations. This is related to the similar issue that
  got fixed with yabridge 3.3.0. A fix for this issue has also been upstreamed
  to the library.
- Respect `$XDG_DATA_HOME` when looking for yabridge's plugin host binaries
  instead of hardcoding `~/.local/share/yabridge`. This matches the existing
  behaviour in yabridgectl.

### Fixed

- Fixed missing transport information for VST2 plugins in **Ardour**, breaking
  host sync and LFOs in certain plugins. This was a regression from yabridge
  3.2.0.
- Fixed _Insert Piz Here_'s _midiLooper_ crashing in **REAPER** when the plugin
  tries to use REAPER's [host function
  API](https://www.reaper.fm/sdk/vst/vst_ext.php#vst_host) which currently isn't
  supported by yabridge. We now explicitly ignore these requests.
- Fixed the plugin-side watchdog timer that checks whether the Wine plugin host
  process failed to start treating zombie processes as still running. This could
  cause plugins to hang during scanning if the Wine process crashed in a very
  specific (and likely impossible) way.
- Fixed VST2 speaker arrangement configurations returned by the plugin not being
  serialized correctly. No plugins seem to actually use these, so it should not
  have caused any issues.
- When printing the Wine version during initialization, the Wine process used
  for this is now run under the same environment as the Wine plugin host process
  will be run under. This means that when using a custom `WINELOADER` script to
  use different Wine versions depending on the Wine prefix, the `wine version:`
  line in the initialization message will always match the version of Wine the
  plugin is going to be run under.

### yabridgectl

- Added support for setting up merged VST3 bundles with a 32-bit version of
  `libyabridge-vst3.so`.
- Fixed the post-installation setup checks when the default Wine prefix over at
  `~/.wine` was created with `WINEARCH=win32` set. This would otherwise result
  in an `00cc:err:process:exec_process` error when running `yabridgectl sync`
  because yabridgectl would try to run the 64-bit `yabridge-host.exe` in that
  prefix.
- Merged VST3 bundles set up in `~/.vst3/yabridge` are now always cleared before
  yabridgectl adds new files to them. This makes it easier to switch from the
  64-bit version of a plugin to the 32-bit version, or from a 64-bit version of
  yabridge to the 32-bit version. I don't know why you would want to do either
  of those things, but now you can!
- Copies of `libyabridge-vst2.so` and `libyabridge-vst3.so` are now reflinked
  when supported by the file system. This speeds up the file coyping process
  while also reducing the amount of disk space used for yabridge when using
  Btrfs or XFS.
- Print a more descriptive error message instead of panicing if running
  `$WINELOADER --version` during yabridgectl's post-setup verification checks
  does not result in any output. This is only relevant when using a custom
  `WINELOADER` script that modifies Wine's output.

## [3.3.1] - 2021-06-09

### Added

- Added thread names to all worker threads created by yabridge. This makes it
  easier to debug and profile yabridge.

### Fixed

- Fixed the `IPlugView::canResize()` cache added in yabridge 3.2.0 sometimes not
  being initialized properly, preventing host-driven resizes in certain
  situations. This was mostly noticeable in **Ardour**.
- Fixed mouse clicks in VST2 editors in **Tracktion Waveform** being offset
  vertically by a small amount because of the way Waveform embeds VST2 editors.
- Fixed _Shattered Glass Audio_ plugins crashing when opening the plugin editor
  because those plugins don't initialize Microsoft COM before trying to use it.
  We now always initialize the Microsoft COM library unconditionally, instead of
  doing it only when a plugin fails to initialize without it.
- Fixed incorrect version strings being reported by yabridge when building from
  a tarball that has been extracted inside of an unrelated git repository. This
  could happen when building the `yabridge` AUR package with certain AUR
  helpers.
- Fixed the log message for the cached `IPlugView::canResize()` VST3 function
  calls implemented in yabridge 3.2.0.

## [3.3.0] - 2020-06-03

### Added

- Added a [compatibility
  option](https://github.com/robbert-vdh/yabridge#compatibility-options) to
  redirect the Wine plugin host's STDOUT and STDERR output streams directly to a
  file. Enabling this allows _ujam_ plugins and other plugins made with the
  Gorilla Engine, such as the _LoopCloud_ plugins, to function correctly. Those
  plugins crash with a seemingly unrelated error message when their output is
  redirected to a pipe.
- Added a small warning during initialization when `RLIMIT_RTTIME` is set to
  some small value. This happens when using PipeWire with rtkit, and it can
  cause crashes when loading plugins.

### Changed

- Added a timed cache for the `IPlugView::canResize()` VST3 function so the
  result will be remembered during an active resize. This makes resizing VST3
  plugin editor windows more responsive.
- Added another cache for when the host asks a VST3 plugin whether it supports
  processing 32-bit or 64-bit floating point audio. Some hosts, like **Bitwig
  Studio**, call this function at the start of every processing cycle even
  though the value won't ever change. Caching this can significantly reduce the
  overhead of bridging VST3 plugins under those hosts.
- Redesigned the VST3 audio socket handling to be able to reuse the process data
  objects on both sides. This greatly reduces the overhead of our VST3 bridging
  by getting rid of all potential memory allocations during audio processing.
- VST2 audio processing also received the same optimizations. In a few places
  yabridge would still reallocate heap data during every audio processing cycle.
  We now make sure to always reuse all buffers and heap data used in the audio
  processing process.
- Considerably optimized parts of yabridge's communication infrastructure by
  preventing unnecessary memory operations. As it turned out, the underlying
  binary serialization library used by yabridge would always reinitialize the
  type-safe unions yabridge uses to differentiate between single and double
  precision floating point audio buffers in both VST2 and VST3 plugins, undoing
  all of our efforts at reusing objects and preventing memory allocations in the
  process. A fix for this issue has also been upstreamed to the library.
- VST3 output audio buffers are now no longer zeroed out at the start of every
  audio processing cycle. We've been doing this for VST3 plugins since the
  introduction of VST3 bridging in yabridge 3.0.0, but we never did this for
  VST2 plugins. Since not doing this has never caused any issues with VST2
  plugins, it should also be safe to also skip this for VST3 plugins. This
  further reduces the overhead of VST3 audio processing.
- Optimized VST3 audio processing for instruments by preallocating small vectors
  for event and parameter change queues.
- VST2 MIDI event handling also received the same small vector optimization to
  get rid of any last potential allocations during audio processing.
- This small vector optimization has also been applied across yabridge's entire
  communication and event handling architecture, meaning that most plugin
  function calls and callbacks should no longer produce any allocations for both
  VST2 and VST3 plugins.
- Changed the way mutual recursion in VST3 plugins on the plugin side works to
  counter any potential GUI related timing issues with VST3 plugins when using
  multiple instances of a plugin.
- Changed the way realtime scheduling is used on the Wine side to be less
  aggressive, potentially reducing CPU usage when plugins are idle.
- The deserialization part of yabridge's communication is now slightly faster by
  skipping some unnecessary checks.
- Log messages about VST3 query interfaces are now only printed when
  `YABRIDGE_DEBUG_LEVEL` is set to 2 or higher, up from 1.

### Fixed

- Fixed a longstanding thread safety issue when hosting a lot of VST2 plugins in
  a plugin group. This could cause plugins to crash or freeze when initializing
  a new instance of a VST2 plugin in a plugin group while another VST2 plugin in
  that same group is currently processing audio.
- Fixed yabridge's Wine processes inheriting file descriptors in some
  situations. This could cause **Ardour** and **Mixbus** to hang when reopening
  the DAW after a crash. The watchdog timer added in yabridge 3.2.0 addressed
  this issue partially, but it should now be completely fixed. This may also
  prevent rare issues where the **JACK** server would hang after the host
  crashes.
- Fixed _DMG_ VST3 plugins freezing in **REAPER** when the plugin resizes itself
  while the host passes channel context information to the plugin.
- Also fixed _DMG_ VST3 plugins freezing in **REAPER** when restoring multiple
  instances of the plugin at once while the FX window is open and the GUI is
  visible.
- Fixed the _PG-8X_ VST2 plugin freezing in **REAPER** when loading the plugin.
- Fixed _Voxengo_ VST2 plugins freezing in **Renoise** when loading a project or
  when otherwise restoring plugin state.
- Fixed logging traces in the VST2 audio processing functions and the VST3 query
  interfaces causing allocations even when `YABRIDGE_DEBUG_LEVEL` is not set to 2.
- Fixed building on Wine 6.8 after some internal changes to Wine's `windows.h`
  implementation.

### yabridgectl

- Improved the warning yabridgectl shows when it cannot run `yabridge-host.exe`
  as part of the post-installation setup checks.
- Fixed the reported number of new or updated plugins when yabridgectl manages
  both a 32-bit and a 64-bit version of the same VST3 plugin.
- Fixed text wrapping being broken after a dependency update earlier this year.

## [3.2.0] - 2021-05-03

### Added

- During VST2 audio processing, yabridge will now prefetch the current transport
  information and process level before sending the audio buffers over to the
  Windows VST2 plugin. This lets us cache this information on the Wine side
  during the audio processing call, which significantly reduces the overhead of
  bridging VST2 plugins by avoiding one or more otherwise unavoidable back and
  forth function calls between yabridge's native plugin and the Wine plugin
  host. While beneficial to every VST2 plugin, this considerably reduces the
  overhead of bridging _MeldaProduction_ VST2 plugins, and it has an even
  greater impact on plugins like _SWAM Cello_ that request this information
  repeatedly over the course of a single audio processing cycle. Previously
  yabridge had a `cache_time_info` compatibility option to mitigate the
  performance hit for those plugins, but this new caching behaviour supercedes
  that option.
- We now always force the CPU's flush-to-zero flag to be set when processing
  audio. Most plugins will already do this by themselves, but plugins like _Kush
  Audio REDDI_ and _Expressive E Noisy_ that don't will otherwise suffer from
  extreme DSP usage increases when processing almost silent audio.
- Added a new [compatibility
  option](https://github.com/robbert-vdh/yabridge#compatibility-options) to hide
  the name of the DAW you're using. This can be useful with plugins that have
  undesirable or broken DAW-specific behaviour. See the [known
  issues](https://github.com/robbert-vdh/yabridge#runtime-dependencies-and-known-issues)
  section of the readme for more information on when this may be useful.
- Yabridge now uses a watchdog timer to prevent rare instances where Wine
  processes would be left running after the native host has crashed or when it
  got forcefully terminated. By design yabridge would always try to gracefully
  shut down its Wine processes when native host has crashed and the sockets
  become unavailable, but this did not always happen if the crash occurred
  before the bridged plugin has finished initializing because of the way Unix
  Domain Sockets work. In that specific situation the `yabridge-host.exe`
  process would be left running indefinitely, and depending on your DAW that
  might have also prevented you from actually restarting your DAW without
  running `wineserver -k` first. To prevent any more dangling processes,
  yabridge's Wine plugin hosts now have a watchdog timer that periodically
  checks whether the original process that spawned the bridges is still running.
  If it detects that the process is no longer alive, yabridge will close the
  sockets and shut down the bridged plugin to prevent any more dangling
  processes from sticking around.

### Changed

- Most common VST2 functions that don't have any arguments are now handled
  explicilty. Yabridge could always automatically support most VST2 functions by
  simply inspecting the function arguments and handling those accordingly. This
  works practically everywhere, but _Plugsound Free_ by UVI would sometimes pass
  unreadable function arguments to functions that weren't supposed to have any
  arguments, causing yabridge to crash. Explicitly handling those functions
  should prevent similar situations from happening in the future.
- Yabridge will now try to bypass VST3 connection proxies if possible. Instead
  of connecting two VST3 plugin objects directly, **Ardour** and **Mixbus**
  place a connection proxy between the two plugin objects so that they can only
  interact indirectly through the DAW. In the past yabridge has always honored
  this by proxying the host's connection proxy, but this causes difficult
  situations with plugins that actively communicate over these proxies from the
  GUI thread, like the _FabFilter_ plugins. Whenever possible, yabridge will now
  try to bypass the connection proxies and connect the two objects directly
  instead, only falling back to proxying the proxies when that's not possible.
- Compile times have been slightly lowered by compiling most of the Wine plugin
  host into static libraries first.
- When building the package from source, the targetted Wine version now gets
  printed at configure-time. This can make it a bit easier to diagnose
  Wine-related compilation issues.

### Removed

- The `cache_time_info` compatibility option has been removed since it's now
  obsolete.
- Removed a message that would show up when loading a VST3 plugin in Ardour,
  warning about potential crashes due to Ardour not supporting multiple input
  and output busses. These crashes have been resolved since yabridge 3.1.0.

### Fixed

- Fixed rare X11 errors that could occur when closing a plugin's editor. In
  certain circumstances, closing a plugin editor would trigger an X11 error and
  crash the Wine plugin host, and with that likely the entire DAW. This happened
  because Wine would try to destroy the window after it had already been
  destroyed. This could happen in Renoise and to a lesser degree in REAPER with
  plugins that take a while to close their editors, such as the _iZotope Rx_
  plugins. We now explicitly reparent the window to back the root window first
  before deferring the window closing. This should fix the issue, while still
  keeping editor closing nice and snappy.
- Plugin group host processes now shut down by themselves if they don't get a
  request to host any plugins within five seconds. This can happen when the DAW
  gets killed right after starting the group host process but before the native
  yabridge plugin requests the group host process to host a plugin for them.
  Before this change, this would result in a `yabridge-group.exe` process
  staying around indefinitely.
- Prevented latency introducing VST3 from freezing **Ardour** and **Mixbus**
  when loading the plugin. This stops _Neural DSP Darkglass_ from freezing when
  used under those DAWs.
- Fixed _FabFilter_ VST3 plugins freezing in **Ardour** and **Mixbus** when
  trying to duplicate existing instances of the plugin after the editor GUI has
  been opened.
- Fixed VST3 plugins freezing in **Ardour** and **Mixbus** when the plugin tries
  to automate a parameter while loading a preset.
- Fixed _Voxengo_ VST3 plugins freezing in **Ardour** and **Mixbus** when
  loading a project or when duplicating the plugin instances.
- Fixed potential X11 errors resulting in assertion failures and crashes in
  **Ardour** and **Mixbus** when those hosts hide (unmap) a plugin's editor
  window.
- Fixed saving and loading plugin state for VST3 _iZotope Rx_ plugins in
  **Bitwig Studio**.
- Fixed a regression from yabridge 3.1.0 where **REAPER** would freeze when opening
  a VST3 plugin context menu.
- Fixed a potential freezing issue in **REAPER** that could happen when a VST3
  plugin resizes itself while sending parameter changes to the host when
  REAPER's 'disable saving full plug-in state' option has not been disabled.
- Fixed another potential freeze when loading a VST3 plugin preset while the
  editor is open when the plugin tries to resize itself based on that new
  preset.
- Fixed a potential assertion failure when loading VST3 presets. This would
  depend on the compiler settings and the version of `libstdc++` used to built
  yabridge with.
- Fixed _PSPaudioware InifniStrip_ failing to initialize. The plugin expects the
  host to always be using Microsoft COM, and it doesn't try to initialize it by
  itself. InfiniStrip loads as expected now.
- Fixed _Native Instruments' FM7_ crashing when processing MIDI. In order to fix
  this, MIDI events are now deallocated later then when they normally would have
  to be.
- Fixed extreme DSP usage increases in _Kush Audio REDDI_ and _Expressive E
  Noisy_ due to denormals.
- Fixed the VST3 version of _W. A. Production ImPerfect_ crashing during audio
  setup.
- Fixed _UVI Plugsound Free_ crashing during initialization.
- Fixed the Wine version detection when using a custom `WINELOADER`.
- Fixed incorrect logging output for cached VST3 function calls.
- Because of the new VST2 transport information prefetching, the excessive DSP
  usage in _SWAM Cello_ has now been fixed without requiring any manual
  compatibility options.

## [3.1.0] - 2021-04-15

### Added

- Added support for using 32-bit Windows VST3 plugins in 64-bit Linux VST3
  hosts. This had previously been disabled because of a hard to track down
  corruption issue.
- Added an
  [option](https://github.com/robbert-vdh/yabridge#compatibility-options) to
  prefer the 32-bit version of a VST3 plugin over the 64-bit version if both are
  installed. This likely won't be necessary, but because of the way VST3 bundles
  work there's no clean way to separate these. So when both are installed, the
  64-bit version gets used by default.

### Fixed

- Worked around a regression in Wine 6.5 that would prevent yabridge from
  shutting down ([wine bug
  #50869](https://bugs.winehq.org/show_bug.cgi?id=50869)). With Wine 6.5
  terminating a Wine process no longer terminates its threads, which would cause
  yabridge's plugin and host components to wait for each other to shut down.
- Fixed preset/state loading in both the VST2 and VST3 versions of _Algonaut
  Atlas 2.0_ by loading and saving plugin state from the main GUI thread.
- Added a workaround for a bug present in every current _Bluecat Audio_ VST3
  plugin. Those plugins would otherwise crash yabridge because they didn't
  directly expose a core VST3 interface through their query interface.
- Fixed a multithreading related memory error in the VST3 audio processor socket
  management system.

### yabridgectl

- Added an indexing blacklist, accessible through `yabridgectl blacklist`. You
  most likely won't ever have to use this, but this lets you skip over files and
  directories in yabridgectl's indexing process.
- Minor spelling fixes.

### Packaging notes

- The Meson wrap dependencies for `bitsery`, `function2` and `tomlplusplus` are
  now defined using `dependency()` with a subproject fallback instead of using
  `subproject()` directly. This should make it easier to package.
- The VST3 SDK Meson wrap dependency and the patches in
  `tools/patch-vst3-sdk.sh` are now based on version 3.7.2 of the SDK.
- The VST3 SDK Meson wrap now uses a tag (`v3.7.2_build_28-patched`) instead of
  a commit hash.

## [3.0.2] - 2021-03-07

### Fixed

- Fix bus information queries being performed for the wrong bus index. This
  fixes VST3 sidechaining in _Renoise_, and prevents a number of VST3 plugins
  with a sidechain input from causing _Ardour_ and _Mixbus_ to freeze or crash.

## [3.0.1] - 2021-02-26

### Changed

- Wine 6.2 introduced a
  [regression](https://bugs.winehq.org/show_bug.cgi?id=50670) that would cause
  compile errors because some parts of Wine's headers were no longer valid C++.
  Since we do not need the affecting functionality, yabridge now includes a
  small workaround to make sure that the affected code never gets compiled. This
  has been fixed for Wine 6.3.

### Fixed

- Added support for a new ReaSurround related VST2.4 extension that **REAPER**
  recently started using. This would otherwise cause certain plugins to crash
  under REAPER.
- Fixed a regression from yabridge 3.0.0 where log output would no longer
  include timestamps.

### yabridgectl

- Changed the wording and colors in `yabridgectl status` for plugins that have
  not yet been setup to look less dramatic and hopefully cause less confusion.
- Aside from the installation status, `yabridgectl status` now also shows a
  plugin's type and architecture. This is color coded to make it easier to
  visually parse the output.
- Plugin paths printed during `yabridgectl status` and
  `yabridgectl sync --verbose` are now always shown relative to the plugin
  directory instead of the same path prefix being repeated for every plugin.

## [3.0.0] - 2021-02-14

### Added

- Yabridge 3.0 introduces the first ever true Wine VST3 bridge, allowing you to
  use Windows VST3 plugins in Linux VST3 hosts with full VST 3.7.1
  compatibility. Simply tell yabridgectl to look for plugins in
  `$HOME/.wine/drive_c/Program Files/Common Files/VST3`, run `yabridgectl sync`,
  and your VST3 compatible DAW will pick up the new plugins in
  `~/.vst3/yabridge` automatically. Even though this feature has been tested
  extensively with a variety of VST3 plugins and hosts, there's still a
  substantial part of the VST 3.7.1 specification that isn't used by any of the
  hosts or plugins we could get our hands on, so please let me know if you run
  into any weird behaviour! There's a list in the readme with all of the tested
  hosts and their current VST3 compatibility status.
- Added an
  [option](https://github.com/robbert-vdh/yabridge#compatibility-options) to use
  Wine's XEmbed implementation instead of yabridge's normal window embedding
  method. This can help reduce flickering when dragging the window around with
  certain window managers. Some plugins will have redrawing issues when using
  XEmbed or the editor might not show up at all, so your mileage may very much
  vary.
- Added a [compatibilty
  option](https://github.com/robbert-vdh/yabridge#compatibility-options) to
  forcefully enable drag-and-drop support under _REAPER_. REAPER's FX window
  reports that it supports drag-and-drop itself, which makes it impossible to
  drag files onto a plugin editor embedded there. This option strips the
  drag-and-drop support from the FX window, thus allowing you to drag files onto
  plugin editors again.
- Added a frame rate
  [option](https://github.com/robbert-vdh/yabridge#compatibility-options) to
  change the rate at which events are being handled. This usually also controls
  the refresh rate of a plugin's editor GUI. The default 60 updates per second
  may be too high if your computer's cannot keep up, or if you're using a host
  that never closes the editor such as _Ardour_.
- Added a [compatibility
  option](https://github.com/robbert-vdh/yabridge#compatibility-options) to
  disable HiDPI scaling for VST3 plugins. At the moment Wine does not have
  proper fractional HiDPI support, so some plugins may not scale their
  interfaces correctly when the host tells those plugins to scale their GUIs. In
  some cases setting the font DPI in `winecfg`'s graphics tab to 192 will also
  cause the GUIs to scale correctly at 200%.
- Added the `with-vst3` compile time option to control whether yabridge should
  be built with VST3 support. This is enabled by default.

### Changed

- `libyabridge.so` is now called `libyabridge-vst2.so`. If you're using
  yabridgectl then nothing changes here. **To avoid any potential confusion in
  the future, please remove the old `libyabridge.so` file before upgrading.**
- The release archives uploaded on GitHub are now repackaged to include
  yabridgectl for your convenience.
- Window closing is now deferred. This means that when closing the editor
  window, the host no longer has to wait for Wine to fully close the window.
  Most hosts already do something similar themselves, so this may not always
  make a difference in responsiveness.
- Slightly increased responsiveness when resizing plugin GUIs by preventing
  unnecessary blitting. This also reduces flickering with plugins that don't do
  double buffering.
- VST2 editor idle events are now handled slightly differently. This should
  result in even more responsive GUIs for VST2 plugins.
- Win32 and X11 events in the Wine plugin host are now handled with lower
  scheduling priority than other tasks. This might help get rid of potential DSP
  latency spikes when having the editor open while the plugin is doing expensive
  GUI operations.
- Opening and closing plugin editors is now also no longer done with realtime
  priority. This should get rid of any latency spikes during those operations,
  as this could otherwise steal resources away from the threads that are
  processing audio.
- The way realtime priorities assigned has been overhauled:

  - Realtime scheduling on the plugin side is now a more granular. Instead of
    setting everything to use `SCHED_FIFO`, only the spawned threads will be
    configured to use realtime scheduling. This prevents changing the scheduling
    policy of your host's GUI thread if your host instantiates plugins from its
    GUI thread like _REAPER_ does.
  - Relaying messages printed by the plugin and Wine is now done without
    realtime priority, as this could in theory cause issues with plugins that
    produce a steady stream of fixmes or other output.
  - The realtime scheduling priorities of all audio threads in the Wine plugin
    host are now periodically synchronized with those of the host's audio
    threads.

- When using `yabridge.toml` config files, the matched section or glob pattern
  is now also printed next to the path to the file to make it a bit easier to
  see where settings are being set from.
- The architecture document has been updated for the VST3 support and it has
  been rewritten to talk more about the more interesting bits of yabridge's
  implementation.
- Part of the build process has been changed to account for [this Wine
  bug](https://bugs.winehq.org/show_bug.cgi?id=49138). Building with Wine 5.7
  and 5.8 required a change for `yabridge-host.exe` to continue working, but
  that change now also breaks builds using Wine 6.0 and up. The build process
  now detects which version of Wine is used to build with, and it will then
  apply the change conditionally based on that to be able to support building
  with both older and newer versions of Wine. This does mean that when you
  switch to an older Wine version, you might need to run
  `meson setup build --reconfigure` before rebuilding to make sure that these
  changes take effect.
- `yabridge-host.exe` will no longer remove the socket directories if they're
  outside of a temporary directory. This could otherwise cause a very unpleasant
  surprise if someone were to pass random arguments to it when for instance
  trying to write a wrapper around `yabridge-host.exe`.
- When `YABRIDGE_DEBUG_LEVEL` is set to 2 or higher and a plugin asks the host
  for the current position in the song, yabridge will now also print the current
  tempo to help debugging host bugs.

### Fixed

- VST2 plugin editor resizing in **REAPER** would not cause the FX window to be
  resized like it would in every other host. This has now been fixed.
- The function for suspending and resuming audio, `effMainsChanged()`, is now
  always executed from the GUI thread. This fixes **EZdrummer** not producing
  any sound because the plugin makes the incorrect assumption that
  `effMainsChanged()` is always called from the GUI thread.
- Event handling is now temporarily disabled while plugins are in a partially
  initialized state. The VST2 versions of **T-RackS 5** would have a chance to
  hang indefinitely if the event loop was being run before those plugins were
  fully initialized because of a race condition within those plugins. This issue
  was only noticeable when using plugin groups.
- Fixed a potential issue where an interaction between _Bitwig Studio_ and
  yabridge's input focus grabbing method could cause delayed mouse events when
  clicking on a plugin's GUI in Bitwig. This issue has not been reported for
  yabridge 2.2.1 and below, but it could in theory also affect older versions of
  yabridge.

### yabridgectl

- Updated for the changes in yabridge 3.0. Yabridgectl now allows you to set up
  yabridge for VST3 plugins. Since `libyabridge.so` got renamed to
  `libyabridge-vst2.so` in this version, it's advised to carefully remove the
  old `libyabridge.so` and `yabridgectl` files before upgrading to avoid
  confusing situations.
- Added the `yabridgectl set --path-auto` option to revert back to automatically
  locating yabridge's files after manually setting a path with
  `yabridgectl set --path=<...>`.
- Added the `yabridgectl set --no-verify={true,false}` option to permanently
  disable post-installation setup checks. You can still directly pass the
  `--no-verify` argument to `yabridgectl sync` to disable these checks for only
  a single invocation.

## [2.2.1] - 2020-12-12

### Fixed

- Fixed some plugins, notably the _Spitfire Audio_ plugins, from causing a
  deadlock when using plugin groups in _REAPER_. Even though this did not seem
  to cause any issues in other hosts, the race condition that caused this issue
  could also occur elsewhere.

## [2.2.0] - 2020-12-11

### Added

- Added an option to cache the time and tempo info returned by the host for the
  current processing cycle. This would normally not be needed since plugins
  should ask the host for this information only once per audio callback, but a
  bug in _SWAM Cello_ causes this to happen repeatedly for every sample,
  resutling in very bad performance. See the [compatibility
  options](https://github.com/robbert-vdh/yabridge#compatibility-options)
  section of the readme for more information on how to enable this.

### Changed

- When `YABRIDGE_DEBUG_LEVEL` is set to 2 or higher and a plugin asks the host
  for the current position in the song, yabridge will now print that position in
  quarter notes and samples as part of the debug output.
- `YABRIDGE_DEBUG_LEVEL` 2 will now also cause all audio processing callbacks to
  be logged. This makes recognizing misbheaving plugins a bit easier.
- Symbols in all `libyabridge.so` and all Winelib `.so` files are now hidden by
  default.

### Fixed

- Fixed an issue where in certain situations Wine processes were left running
  after the host got forcefully terminated before it got a chance to tell the
  plugin to shut down. This could happen when using Kontakt in Bitwig, as Bitwig
  sets a limit on the amount of time a plugin is allowed to spend closing when
  you close Bitwig, and Kontakt can take a while to shut down.
- Fixed a potential crash or freeze when removing a lot of plugins from a plugin
  group at exactly the same time.

## [2.1.0] - 2020-11-20

### Added

- Added a separate
  [yabridgectl](https://aur.archlinux.org/packages/yabridgectl/) AUR package for
  Arch and Manjaro. The original idea was that yabridgectl would not require a
  lot of changes and that a single
  [yabridgectl-git](https://aur.archlinux.org/packages/yabridgectl-git/) package
  would be sufficient, but sometimes changes to yabridgectl will be incompatible
  with the current release so it's nicer to also have a separate regular
  package.

### Changed

- Yabridge will now always search for `yabridge-host.exe` in
  `~/.local/share/yabridge` even if that directory is not in the search path.
  This should make setup easier, since you no longer have to modify any
  environment variables when installing yabridge to the default location.
  Because of this, the symlink-based installation method does not have a lot of
  advantages over the copy-based method anymore other than the fact that you
  can't forget to rerun `yabridgectl sync` after an upgrade, so most references
  to it have been removed from the readme.

### Fixed

- Fixed an issue where _Renoise_ would show an error message when trying to load
  a plugin in the mixer.

## [2.0.2] - 2020-11-14

### Fixed

- Added a workaround for a bug in _Ardour 6.3_ which would cause several plugins
  including MT Power Drumkit to crash when opening the editor.
- Fixed linking error in debug build related to the parallel STL.

## [2.0.1] - 2020-11-08

### Fixed

- Fixed a regression where `yabridge-host.exe` would not exit on its own after
  the host crashes or gets terminated without being able to properly close all
  plugins.

## [2.0.0] - 2020-11-08

### Added

- The way communication works in yabridge has been completely redesigned to be
  fully concurrent and to use additional threads as necessary. This was needed
  to allow yabridge to handle nested and mutually recursive function calls as
  well as several other edge cases a synchronous non-concurrent implementation
  would fail. What this boils down to is that yabridge became even faster, more
  responsive, and can now handle many scenarios that would previously require
  workarounds. The most noticeable effects of these changes are as follows:

  - The `hack_reaper_update_display` workaround for _REAPER_ and _Renoise_ to
    prevent certain plugins from freezing is no longer needed and has been
    removed.
  - Opening and scanning plugins becomes much faster in several VST hosts
    because more work can be done simultaneously.
  - Certain plugins, such as Kontakt, no longer interrupt audio playback in
    Bitwig while their editor was being opened.
  - Any loading issues in Bitwig Studio 3.3 beta 1 are no longer present.
  - Hosting a yabridged plugin inside of the VST2 version of Carla now works as
    expected.
  - And probably many more improvements.

  Aside from these more noticeable changes, this has also made it possible to
  remove a lot of older checks and behaviour that existed solely to work around
  the limitations introduced by the old event handling system. I have been
  testing this extensively to make sure that these changes don't not introduce
  any regressions, but please let me know if this did break anything for you.

### Changed

- The way the Wine process handles threading has also been completely reworked
  as part of the communication rework.
- GUI updates for plugins that don't use hardware acceleration are now run at 60
  Hz instead of 30 Hz. This was kept at 30 updates per second because that
  seemed to be a typical rate for Windows VST hosts and because function calls
  could not be processed while the GUI was being updated, but since that
  limitation now no longer exists we can safely bump this up.
- Sockets are now created in `$XDG_RUNTIME_DIR` (which is `/run/user/<user_id>`
  on most systems) instead of `/tmp` to avoid polluting `/tmp`.

### Removed

- The now obsolete `hack_reaper_update_display` option has been removed.
- The previously deprecated `use-bitbridge` and `use-winedbg` compilation
  options have been removed. Please use `with-bitbridge` and `with-winedbg`
  instead.

### Fixed

- Fixed a very long standing issue with plugins groups where unloading a plugin
  could cause a crash. Now you can host over a hundred plugins in a single
  process without any issues.
- Fixed another edge case with plugin groups when simultaneously opening
  multiple plugins within the same group. The fallover behaviour that would
  cause all of those plugins to eventually connect to a single group host
  process would sometimes not work correctly because the plugins were being
  terminated prematurely.
- Fixed the implementation of the accumulative `process()` function. As far as
  I'm aware no VST hosts made in the last few decades even use this, but it just
  feels wrong to have an incorrect implementation as part of yabridge.

## [1.7.1] - 2020-10-23

### Fixed

- Fixed a regression where the `editor_double_embed` option would cause X11
  errors and crash yabridge.
- Fixed a regression where certain fake dropdown menus such as those used in the
  Tokyo Dawn Records plugins would close immediately when hovering over them.
- Fixed an issue where plugins hosted within a plugin group would not shut down
  properly in certain situations. This would cause the VST host to hang when
  removing such a plugin.

### yabridgectl

- When running `yabridgectl sync`, existing .so files will no longer be
  recreated unless necessary. This prevents hosts from rescanning all plugins
  after setting up a single new plugin through yabridgectl. Running
  `yabridgectl sync` after updating yabridge will still recreate all existing
  .so files as usual.
- Added a `--force` option to `yabridgectl sync` to always recreate all existing
  .so files like in previous versions.
- Fixed a regression from yabridgectl 1.6.1 that prevented you from removing
  directories that no longer exist using `yabridgectl rm`.

## [1.7.0] - 2020-10-13

### Changed

- The way keyboard input works has been completely rewritten to be more reliable
  in certain hosts and to provide a more integrated experience. Hovering over
  the plugin's editor while the window provided by the host is active will now
  immediately grab keyboard focus, and yabridge will return input focus to the
  host's window when moving the mouse outside of the plugin's editor when the
  window is still active. This should fix some instances where keyboard input
  was not working in hosts with more complex editor windows like _REAPER_ and
  _Ardour_, and it also allows things like the comment field in REAPER's FX
  window to still function.

  A consequence of this change is that pressing Space in Bitwig Studio 3.2 will
  now play or pause playback as intended, but this does mean that it can be
  impossible to type the space character in text boxes inside of a plugin editor
  window. Please let me know if this causes any issues for you.

- Both unrecognized and invalid options are now printed on started to make
  debugging `yabridge.toml` files easier.

- Added a note to the message stating that libSwell GUI support has been
  disabled to clarify that this is expected behaviour when using REAPER. The
  message now also contains a suggestion to enable the
  `hack_reaper_update_display` option when it is not already enabled.

### Fixed

- Added a workaround for reparenting issues with the plugin editor GUI on a
  [specific i3 setup](https://github.com/robbert-vdh/yabridge/issues/40).

### Documentation

- The documentation on `yabridge.toml` files and the available options has been
  rewritten in an effort to make it easier to comprehend.

## [1.6.1] - 2020-09-28

### Fixed

- Fixed a potential crash that could happen if the host would unload a plugin
  immediately after its initialization. This issue affected the plugin scanning
  in _REAPER_.
- Fixed parsing order of `yabridge.toml`. Sections were not always read from top
  to bottom like they should be, which could cause incorrect and unexpected
  setting overrides.
- Fixed an initialization error when using plugin groups for plugins that are
  installed outside of a Wine prefix.

### yabridgectl

- Relative paths now work when adding plugin directories or when setting the
  path to yabridge's files.
- Also search `/usr/local/lib` for `libyabridge.so` when no manual path has been
  specified. Note that manually copying yabridge's files to `/usr` is still not
  recommended.

## [1.6.0] - 2020-09-17

### Added

- Added support for double precision audio processing. This is not very widely
  used, but some plugins running under REAPER make use of this. Without this
  those plugins would cause REAPER's audio engine to crash.

### Fixed

- Increased the limit for the maximum number of audio channels. This could cause
  issues in Renoise when using a lot of output channels.

## [1.5.0] - 2020-08-21

### Added

- Added an option to work around timing issues in _REAPER_ and _Renoise_ where
  the hosts can freeze when plugins call a certain function while the host
  doesn't expect it, see
  [#29](https://github.com/robbert-vdh/yabridge/issues/29) and
  [#32](https://github.com/robbert-vdh/yabridge/issues/32). The
  [readme](https://github.com/robbert-vdh/yabridge#runtime-dependencies-and-known-issues)
  contains instructions on how to enable this.

### Changed

- Don't print calls to `effIdle()` when `YABRIDGE_DEBUG_LEVEL` is set to 1.

### Fixed

- Fix Waves plugins from freezing the plugin process by preventing them from
  causing an infinite message loop.

## [1.4.1] - 2020-07-27

### yabridgectl

- Fixed regression caused by
  [alexcrichton/toml-rs#256](https://github.com/alexcrichton/toml-rs/issues/256)
  where the configuration file failed to parse after running `yabridgectl sync`.
  If you have already run `yabridgectl sync` using yabridgectl 1.4.0, then
  you'll have to manually remove the `[last_known_config]` section from
  `~/.config/yabridgectl/config.toml`.
- Fixed issue with overwriting broken symlinks during `yabridgectl sync`.

## [1.4.0] - 2020-07-26

### Added

- Added an alternative editor hosting mode that adds yet another layer of
  embedding. Right now the only known plugins that may need this are
  _PSPaudioware_ plugins with expandable GUIs such as E27. The behaviour can be
  enabled on a per-plugin basis in the plugin configuration. See the
  [readme](https://github.com/robbert-vdh/yabridge#compatibility-options)
  for more details.

### Changed

- Both parts of yabridge will now run with realtime priority if available. This
  can significantly reduce overall latency and spikes. Wine itself will still
  run with a normal scheduling policy by default, since running wineserver with
  realtime priority can actually increase the audio processing latency although
  it does reduce the amount of latency spikes even further. You can verify that
  yabridge is running with realtime priority by looking for the `realtime:` line
  in the initialization message. I have not found any downsides to this approach
  in my testing, but please let me know if this does end up causing any issues.

### Fixed

- Fixed rare plugin location detection issue on Debian based distros related to
  the plugin and host detection fix in yabridge 1.2.0.

### yabridgectl

- Added a check to `yabridgectl sync` that verifies that the currently installed
  versions of Wine and yabridge are compatible. This check will only be repeated
  after updating either Wine or yabridge.

- Added a `--no-verify` option to `yabridgectl sync` to skip the
  post-installation setup checks. This option will skip both the login shell
  search path check for the copy-based installation method as well as the new
  Wine compatibility check.

## [1.3.0] - 2020-07-17

### Added

- By somewhat popular demand yabridge now comes with yabridgectl, a utility that
  can automatically set up and manage yabridge for you. It also performs some
  basic checks to ensure that everything has been set up correctly so you can
  get up and running faster. Yabridgectl can be downloaded separately from the
  GitHub releases page and its use is completely optional, so you don't have to
  use it if you don't want to. Check out the
  [readme](https://github.com/robbert-vdh/yabridge/tree/master/tools/yabridgectl)
  for more information on how it works.

### Deprecated

- The `use-bitbridge` and `use-winedbg` options have been deprecated in favour
  of the new `with-bitbridge` and `with-winedbg` options. The old options will
  continue to work until they are removed in yabridge 2.0.0.

## [1.2.1] - 2020-06-20

### Changed

- When building from source, only statically link Boost when the
  `with-static-boost` option is enabled.
- The `use-bitbridge` and `use-winedbg` options have been replaced by
  `with-bitbridge` and `with-winedbg` for consistency's sake. The old options
  will be marked as deprecated in the next minor release.

### Fixed

- Fixed memory error that would cause crashing on playback with some buffer
  sizes in Mixbus6.
- Opening a plugin would override the Wine prefix for all subsequent plugins
  opened from within the same process. This prevented the use of multiple Wine
  prefixes in hosts that do not sandbox their plugins, such as Ardour.
- Manual Wine prefix overides through the `WINEPREFIX` environment were not
  reflected in the output shown on startup.
- Fixed plugin group socket name generation. This would have prevented plugin
  groups with the same name from being used simultaneously in different Wine
  prefixes.
- Distinguish between active processes and zombies when checking whether a group
  host process is still running during initialization.

## [1.2.0] - 2020-05-29

### Added

- Added the ability to host multiple plugins within a single Wine process
  through _plugin groups_. A plugin group is a user-defined set of plugins that
  will be hosted together in the same Wine process. This allows multiple
  instances of plugins to share data and communicate with each other. Examples
  of plugins that can benefit from this are FabFilter Pro-Q 3, MMultiAnalyzer,
  and the iZotope mixing plugins. See the readme for instructions on how to set
  this up.

### Changed

- Changed architecture to use one fewer socket.
- GUI events are now always handled on a steady timer rather than being
  interleaved as part of the event loop. This change was made to unify the event
  handling logic for individually hosted plugins and plugin groups. It should
  not have any noticeable effects, but please let me know if this does cause
  unwanted behavior.

### Fixed

- Steal keyboard focus when clicking on the plugin editor window to account for
  the new keyboard focus behavior in _Bitwig Studio 3.2_.
- Fixed large amount of empty lines in the log file when the Wine process closes
  unexpectedly.
- Made the plugin and host detection slightly more robust.

## [1.1.4] - 2020-05-12

### Fixed

- Fixed a static linking issue with the 32-bit build for Ubuntu 18.04.

## [1.1.3] - 2020-05-12

### Fixed

- Added a workaround for the compilation issues under Wine 5.7 and above as
  caused by [Wine bug #49138](https://bugs.winehq.org/show_bug.cgi?id=49138).
- Added a workaround for plugins that improperly defer part of their
  initialization process without telling the host. This fixes startup behavior
  for the Roland Cloud plugins.
- Added a workaround for a rare race condition in certain plugins caused by
  incorrect assumptions in plugin's editor handling. Fixes the editor for
  Superior Drummer 3 and the Roland Cloud synths in Bitwig Studio.
- Fixed potential issue with plugins not returning their editor size.

## [1.1.2] - 2020-05-09

### Fixed

- Fixed an issue where plugin removal could cause Ardour and Mixbus to crash.

## [1.1.1] - 2020-05-09

### Changed

- Changed installation recommendations to only install using symlinks with hosts
  that support individually sandboxed plugins.
- Respect `YABRIDGE_DEBUG_FILE` when printing initialization errors.

### Fixed

- Stop waiting for the Wine VST host process on startup if the process has
  crashed or if Wine was not able to start.

## [1.1.0] - 2020-05-07

### Added

- Added support for plugins that send MIDI events back to the host. This allows
  plugins such as Cthulhu and Scaler to output notes and CC for another plugin
  to work with.
- Added support for querying and setting detailed information about speaker
  configurations for use in advanced surround setups. This indirectly allows
  yabridge to work under _Renoise_.
- Added automated development builds for yabridge, available by clicking on the
  'Automated builds' badge in the project readme.

### Changed

- Changed the plugin detection mechanism to support yet another way of
  symlinking plugins. Now you can use a symlink to a copy of `libyabridge.so`
  that's installed for a plugin in another directory. This is not recommended
  though.
- Changed Wine prefix detection to be relative to the plugin's `.dll` file,
  rather than the loaded `.so` file.
- Increased the maximum number of audio channels from 32 to 256.
- Clarified the error that appears when we're unable to load the `.dll`.
- Yabridge will now print the used version of Wine during startup. This can be
  useful for diagnosing startup problems.

### Fixed

- Fixed plugins failing to load on certain versions of _Ubuntu_ because of
  paths starting with two forward slashes.
- Redirect the output from the Wine host process earlier in the startup process.
  Otherwise errors printed during startup won't be visible, making it very hard
  to diagnose problems.

## [1.0.0] - 2020-05-03

### Added

- This changelog file to track keep track of changes since yabridge's 1.0
  release.
