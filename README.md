# Fliqlo for RenkuOS

**English** | [日本語](README.ja.md) | [한국어](README.ko.md)

A flip clock screen saver for Haiku and RenkuOS, modelled on
[Fliqlo](https://fliqlo.com/): two dark cards on black, hours and minutes in
tall condensed digits, and the top half of a card flips down whenever its
value changes. Runs on 32-bit x86 (x86_gcc2), x86_64 and arm64.

![Fliqlo full screen on RenkuOS arm64](docs/screenshot.png)

## Install with pkgman

| Haiku | Add the repository |
| --- | --- |
| 32-bit x86 (x86_gcc2) | `pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2` |
| x86_64 | `pkgman add-repo https://pkgman.rainygirl.com/x86_64` |
| arm64 | `pkgman add-repo http://pkgman.rainygirl.com/arm64` |

```sh
pkgman install fliqlo
```

If `pkgman add-repo` fails with `Operation not supported`, the network kit of
that image has no TLS; use `http://` instead of `https://`.

On arm64 the package also brings `libscreensaver.so` and the **ScreenSaver**
preferences: RENKU arm64 images are minimum builds and leave both out.

## Use

1. Open **Deskbar -> Preferences -> ScreenSaver**.
2. Pick **Fliqlo** in the list. The preview shows the clock.
3. Set the options on the right:
   - **24-hour clock** -- off shows `4 50` with AM/PM in the corner, on shows
     `16 50`.
   - **Size** -- how much of the screen the cards take.
   - **Brightness** -- dims the cards and digits.
4. Use **Test** to see it full screen; move the mouse or press a key to leave.

![The Fliqlo options in ScreenSaver preferences](docs/preferences.png)
![A card flipping from 56 to 57](docs/flip.png)

Fliqlo's digits are Helvetica Condensed Bold, which cannot be shipped. The
package installs TeX Gyre Heros Condensed Bold instead, a free Helvetica clone
(GUST Font License). Without it the clock falls back to Noto Sans Display Bold.

## Build

On Haiku: `make install` builds the add-on and copies it and the font into
`~/config/non-packaged`. On a 32-bit x86 hybrid use the default `g++` (gcc2):
screen_blanker there only loads gcc2 add-ons.

On a workstation with Docker, `tools/cross-build.sh all` cross-builds
`dist/x86/` (gcc2), `dist/x86_64/` and `dist/arm64/`; the pkgman-repo recipe
packages those.

## Not affiliated

This is an independent reimplementation for Haiku. It is not made by, endorsed
by or connected to the makers of Fliqlo; no Fliqlo code or artwork is used.

## AI disclosure

Fliqlo for RenkuOS was developed with the assistance of AI coding tools
(Anthropic's Claude).

## License

MIT for the code. `fonts/texgyreheroscn-bold.otf` (TeX Gyre Heros, by
B. Jackowski and J. M. Nowacki) is under the GUST Font License
(`fonts/GUST-FONT-LICENSE.txt`).
