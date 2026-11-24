# Experimental RGB888 lighting A/B

```ini
[Matrox]
RGB888Lighting=0
Dither=0
```

Default 0 preserves the legacy native RGB565 /255 rounding and optional 4x4 ordered quantization. RGB888Lighting=1 shifts RGB565 channels to eight bits (no bit replication), multiplies by the existing per-channel 0..255 coefficients, truncates /256, then quantizes to RGB565. If Dither=1 and the game requests dither, apply the measured 2x2 phase before quantization. No inversion of game/API requests is attempted: the observed inverted polarity of the original MSI95 1.4 under 86Box is not sufficient to change this behavior. Dither=0 always disables ordered quantization. Coefficient interpolation, HUD bypass, texture sampling, alpha and keying unchanged. The experiment can darken colors, even at nominal full light without dither; it is not an asserted improvement or original hardware proof. 3dfx unaffected. F12 _lighting.bin header field at offset 20 identifies arithmetic: 0 legacy, 1 experimental. Older captures remain readable; the analyzer selects the correct formula and no-dither reference. Earlier sourceaudit spatial tables remain legacy 4x4 coordinate bins, not actual 2x2 ranks for experimental captures. Compare same scene with the same options except RGB888Lighting; restart game after changes. Use [Debug] Log=1 and F12 if investigating differences.
