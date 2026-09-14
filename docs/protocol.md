# Bridge protocol v8

The bridge listens only on loopback by default. Control and metadata responses are JSON; the artwork route returns packed RGB24 bytes. Every response includes a suitable HTTP status code. The visible Motif application is the only intended client.

Transport and data requests are executed through the Apple Music page's own
authenticated MusicKit instance. DOM control matching exists only as a fallback.

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/v1/status` | Bridge readiness and authorization state |
| GET | `/v1/library/tracks` | Tracks for the active playlist, album, song, or library route |
| GET | `/v1/library/{songs,albums,artists,playlists}` | An iTunes-style library source view |
| GET | `/v1/library/recent` | Mixed recently played albums, songs, and playlists |
| GET | `/v1/listen-now` | Personalized Apple Music recommendations |
| GET | `/v1/radio` | Apple Music radio stations |
| GET | `/v1/library/{albums,artists,playlists}/{id}` | Drill into the selected library object |

Track detail items include `trackNumber`, `discNumber`, and `discCount`. The
Motif client uses them for numbered children and only renders disc headings for
actual multi-disc releases.
| GET | `/v1/search?q=...` | Catalog/library search |
| GET | `/v1/player/state` | Current track, position, volume, playback, shuffle, repeat, AutoMix, and Autoplay |
| POST | `/v1/player/play` | Start or resume playback |
| POST | `/v1/player/pause` | Pause playback |
| POST | `/v1/player/next` | Advance the queue |
| POST | `/v1/player/previous` | Return to the prior queue item |
| POST | `/v1/player/volume` | Set volume using `{ "value": 0.0..1.0 }` |
| POST | `/v1/player/queue` | Queue and play an exact `{ "kind", "id" }` item |
| POST | `/v1/player/seek` | Seek to an absolute playback time |
| POST | `/v1/player/shuffle` | Set shuffle using `{ "enabled": true|false }` |
| POST | `/v1/player/repeat` | Set repeat using `{ "mode": "off"|"all"|"one" }` |
| POST | `/v1/player/automix` | Set AutoMix using `{ "enabled": true|false }` |
| POST | `/v1/player/autoplay` | Set Autoplay using `{ "enabled": true|false }` |
| GET | `/v1/player/queue` | Current queue, including MusicKit autoplay additions |
| GET | `/v1/player/artwork.rgb` | Current track artwork as packed 24-bit RGB bytes |

`/v1/player/artwork.rgb` accepts `size` and an optional encoded `url`. The
sidebar and Now Playing Artwork view use the current item; Grid supplies each
album's artwork URL. The response length is exactly `size * size * 3` bytes, with RGB bytes in row-major order.
| POST | `/v1/browser/show` | Bring the persistent Apple Music login/player window forward |
| POST | `/v1/shutdown` | Stop the bridge and its Chromium playback engine |
| GET | `/v1/diagnostics` | Report accessible web-player controls for selector debugging |

## Browser states

- `starting_browser`: Chromium has not exposed the Apple Music page yet.
- `needs_login`: the persistent browser profile is not signed into Apple Music.
- `ready`: the authenticated Apple Music web player is controllable.
- `chromium_not_found`: set `MOTIF_APPLE_MUSIC_CHROMIUM` to the executable.

The frontend must treat unrecognized fields as optional and unrecognized
authorization states as unavailable. This lets the bridge grow without tightly
coupling it to a particular Motif build.
