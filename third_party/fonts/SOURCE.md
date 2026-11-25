# Bundled fonts

HeadsUp draws its nameplates in one of these unless you pick a font installed in Windows, in Montserrat SemiBold by
default, whose plain round letters are closest to the game's own name lettering. Barlow Condensed, plain and
narrow, is the badges' font, so their numbers stay readable at a few pixels tall. Atkinson Hyperlegible and
Atkinson Hyperlegible Next are the Braille Institute's fonts for readers with low vision. They are loaded into the
plugin's own process only, with `AddFontMemResourceEx`, and nothing is installed on your system.

| Font | Family GDI sees | From | License |
|---|---|---|---|
| `Montserrat-SemiBold.ttf` | Montserrat SemiBold | JulietaUla/Montserrat `fonts/ttf` (version 9.000) | `OFL-Montserrat.txt` |
| `MarcellusSC-Regular.ttf` | Marcellus SC | google/fonts `ofl/marcellussc` | `OFL-Marcellus-SC.txt` |
| `Cinzel-Regular.ttf` | Cinzel | NDISCOVER/Cinzel `fonts/ttf` | `OFL-Cinzel.txt` |
| `CormorantSC-SemiBold.ttf` | Cormorant SC | google/fonts `ofl/cormorantsc` | `OFL-Cormorant-SC.txt` |
| `AtkinsonHyperlegible-Regular.ttf` | Atkinson Hyperlegible | googlefonts/atkinson-hyperlegible `fonts/ttf` (version 1.006) | `OFL-Atkinson-Hyperlegible.txt` |
| `AtkinsonHyperlegibleNext-Regular.ttf` | Atkinson Hyperlegible Next | googlefonts/atkinson-hyperlegible-next `fonts/ttf` (version 2.001) | `OFL-Atkinson-Hyperlegible-Next.txt` |
| `BarlowCondensed-Bold.ttf` | Barlow Condensed | google/fonts `ofl/barlowcondensed` | `OFL-Barlow-Condensed.txt` |

The two Atkinson fonts were downloaded from
https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/1cb311624b2ddf88e9e37873999d165a8cd28b46/fonts/ttf/AtkinsonHyperlegible-Regular.ttf
and
https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible-next/7925f50f649b3813257faf2f4c0b381011f434f1/fonts/ttf/AtkinsonHyperlegibleNext-Regular.ttf,
each with the `OFL.txt` beside it in its repository.

All seven are under the SIL Open Font License 1.1, which asks for its notice to ship with the fonts, so the licenses
above are part of the repository and CREDITS.md names them.
