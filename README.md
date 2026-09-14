
### DISCLAIMER
This project *was* made primarily using ChatGPT, since writing this kind of thing is outside of my skillset, however I *do* work on UI elements and give genuine feedback during the process, like the AutoPlay, Shuffle, AutoMix, and Repeat pixmaps, and padding around items. I also explicitly *despise" AI Image Generation, instead opting for using the toolkit itself, and creating assets by hand when something genuinely needs a raster/SVG image.  

# Marmelade

An unofficial Apple Music client that uses the Motif Widget Toolkit, built primarily for GNU/Linux and intended to be platform-agnostic.

Current development version: `0.2.0-DEV-22.3`

Marmelade keeps the visible application native Xt/Motif. A small local Node.js bridge controls a dedicated Chromium Apple Music session and exposes playback, library, queue, artwork, and account state to the C frontend over a loopback-only HTTP protocol.

## Current features

- Native Xt/Motif interface with MaXX scheme support.
- Listen Now, Recently Played, Songs, Albums, Artists, Playlists, Radio, and search.
- Native list and album-grid views.
- Full-color album artwork rendered as native X11 pixmaps.
- Persistent Apple Music login through a dedicated Chromium profile.
- Play/pause, previous/next, seek, volume, Shuffle, Repeat, AutoMix, and Autoplay controls.
- Up Next queue with Autoplay integration.
- Resizable sidebar with iTunes-style pinned album art.
- Damage-limited custom list rendering and lazy loading for list/artwork views.
- Locally-documented bridge protocol so the browser backend can be replaced without rewriting the Motif frontend.

## Platform status

Linux is the main development platform, and currently works perfectly.
*BSD currently is untested.
macOS is currently untested, likely will not work, as I am not aware of anyone actively using Motif apps on the platform.
Windows is unsupported, and likely will never be supported, as it has no Motif implementation that I'm aware of, and has been going downhill since 2012 at *least*.

HaikuOS will likely get its' own frontend down the line, since it lacks a good GUI music-streaming application.

GNU/Linux is the primary tested platform. The native side is written against C99/POSIX, Xt, Motif, X11, and Xpm rather than a Linux-specific GUI stack. Other UNIX-like systems are an intended target, but BSD compatibility has not yet been verified.

## Requirements for building:

To build the native frontend, you will need

- A C99-compliant compiler 
- Motif development headers and libraries
- X11 and Xt development headers and libraries
- Xpm
- pkg-config` is recommended but not required



To use the Apple Music bridge, you will need

- Node.js 20 or newer
- Chromium or a compatible Chromium-based browser
- An Apple Music subscription

Node.js is a hard runtime requirement when Marmelade is managing its own bridge. If Node.js cannot be found, the application reports as such, and doesn't proceed further.

## Building

```sh
make
make run
```

Run the bridge by itself:

```sh
make bridge
```

Run the bridge and client checks:

```sh
make check
```

Useful runtime overrides:

```sh
MOTIF_APPLE_MUSIC_MANAGE_BRIDGE=0 ./motif-apple-music
MOTIF_APPLE_MUSIC_BRIDGE_URL=http://127.0.0.1:17876 ./motif-apple-music
MOTIF_APPLE_MUSIC_NODE=/path/to/node ./motif-apple-music
MOTIF_APPLE_MUSIC_CHROMIUM=/path/to/chromium ./motif-apple-music
```

## About the login process

The bridge runs Chromium headlessly by default. Use `File -> Show Apple Music Login...` to restart the Chromium process in the foreground and sign in through the Apple Music web UI. 

The profile is stored in

```text
~/.local/share/motif-apple-music/chromium
```

Later launches reuse that session, so you don't have to log in every time.

## The architecture

The frontend communicates with the local bridge on `127.0.0.1:17876` by default. The bridge drives Apple Music through the authenticated web page's MusicKit instance, with DOM control matching used only as a fallback. Artwork is decoded by Chromium and transferred to the native client as packed RGB24 data.

See [`docs/protocol.md`](docs/protocol.md) for the current bridge contract.

## Development versioning

Feature milestones use whole DEV numbers, for example `0.2.0-DEV-23`. Small fixes and presentation-only revisions stay on the same milestone with a fractional suffix, for example `0.2.0-DEV-22.3`.

## Status

This is development software and is not affiliated with or endorsed by Apple Inc.
