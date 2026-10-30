# European FadeLow static research — two Scorched Planet 3dfx editions

Status (2026-10-30): **EN/ES/IT FadeLow prototype user-validated in gameplay**; the owner reports "le patch fonctionne". **FR/DE failed the owner's gameplay test (reported 2026-10-30), with the exact failure symptom still unspecified.** This report does not imply all missions, cinematics, or both wrappers have been tested.

This analysis used the two owner-supplied original executables, without modifying them, and compares the palette-fade mechanism with the previously validated US FadeLow 4-byte patch. No original or patched commercial executable is stored in this repository.

## Verified identities and patch offsets

| Edition | Original EXE SHA-256 | Size (bytes) | Fade routine VA | 4-byte threshold VA | File offset | Exact 4-byte test-copy SHA-256 |
| --- | --- | ---: | --- | --- | --- | --- |
| EN/ES/IT (InstallKey 1.06) | `9ab6442553d055a14273ee0e3c5a912986662dc0a3cc8512ac716879b5068e5f` | 835584 | `0x004493A0` | `0x004493AD` | `0x487AD` | `dec3977ff83758f3a65bc5fe96a6bafaa064bbf34ef93ee9f35593fb65e173bc` |
| FR/DE (InstallKey 1.06) | `eff38cedf2408566060b3c7cf39be59c54d23e67b0a4c7cb0725051aa87e1b72` | 836608 | `0x00449660` | `0x0044966D` | `0x48A6D` | `5373c7b24389556b7466932695e7e2d1076531105f9bb879749e783a64898d90` |
| US (InstallKey 1.08, already gameplay-validated) | `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a` | 836608 | `0x004495C0` | `0x004495CD` | `0x489CD` | `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45` |

Both supplied European files are PE32 i386, ImageBase `0x00400000`, Windows GUI subsystem. Their complete SHA-256 hashes agree with the previously uploaded edition inventories under `data/cd-inventories/`. The InstallKey version labels are from the CD `autorun.inf`, not proven game EXE version resources.

## Static verification

1. In both European files, x86 instruction `cmp dword ptr [esp+0x404], 0x38D1B717` is found at the matching fade-routine prologue. Its immediate `17 B7 D1 38` encodes IEEE754 float32 approximately 0.0001, same as US.
2. The instruction neighborhood around the immediate is identical between both European editions: `81 BC 24 04 04 00 00 | 17 B7 D1 38 | 53 56 57 0F 8E 1A 01 00 00`. In each binary, this exact 20-byte sequence occurs once.
3. The same fade path contains the palette data update, countdown subtraction and return-state check, whose terminal threshold is `0x3A83126F`.
4. Disassembly with `objdump -d -M intel` finds **35 direct calls** to the respective fade routine and **12 busy-wait backedges** (`jne -22`) that re-call both the edition-specific timer routine and fade routine. Example European timers are `0x00435F10` (EN/ES/IT) and `0x004361A0` (FR/DE).
5. Creating user-only test copies by replacing **only four bytes** of the threshold literal with `AC C5 27 37` (float32 approximately 0.00001, matching the US FadeLow candidate) preserves the full file size and all other bytes. Generated output hashes are recorded above. After copy-generation, each output hash and its four altered byte positions were checked independently.

**Inference, not gameplay proof:** this is very strong static evidence that the same subthreshold-time loss mechanism can affect European fades. Whether it creates the observed real-world Go freeze on a particular Windows/Wine setup remains unverified without a game run.

## Release gate and testing

- The Windows x64 Rust GUI now permits FadeLow for **US and EN/ES/IT only**, protected by exact original/patched hashes, verified per-edition backup and restore. **FR/DE remains disabled**, pending a game test.
- Trial binaries are separate private output files, supplied only to the original owner via this conversation. **Do not add original or modified commercial EXEs to GitHub, CI, or a public release**.
- Test using an isolated copy of each installed edition, keeping its original `SPVOODOO.EXE` untouched. Launch the alternate-name trial EXE via Lutris/Wine or Windows, repeat the previous Go/mission fade scenario and verify menu, mission transitions, playback, input and audio. Revert simply by selecting the original EXE again.
- Owner validation received for EN/ES/IT on 2026-10-30: the privately supplied four-byte test copy works in the game. That edition is eligible for Rust production patch/restore, but the new Rust GUI/installer itself still requires a separate end-to-end test. FR/DE is not eligible until its own gameplay test passes.

No modification to `main`; research and proposed changes are kept in draft PR #5.

legluondunet assisted by ChatGPT (OpenAI).

## Configuration learned from successful EN/ES/IT test (2026-10-30)

The owner extracted the CD, copied `VOODOO` game files into `SPLANET` while excluding `glide.dll`, and created `splanet.ini`. Required configuration in the tested environment:

- `[scorch]` `video=<absolute path to local splanet\\video>`
- `language=4` (**Italian**) in the owner's example
- `HighQualityVideo=1`

We independently inspected the original European PE32 language-resource pointer table and x86 lookup indexed from `base + 4 * language`: `1=english.rez`, `2=french.rez`, `3=german.rez`, `4=italian.rez`, `5=spanish.rez`; index zero is null. For EN/ES/IT, only 1, 4 and 5 are actually present on the CD. For FR/DE, only 2 and 3 are present. Avoid offering missing language resources in the GUI.

The owner-supplied INI also contains `keymap`, `gamma`, `joystick`, `settings`, and `name`. Whether all these are required for first launch is **not established**. The installer supplies the already established fallback `settings` and generic player name when absent; when they exist, it leaves them untouched. `keymap`, `gamma`, and `joystick` are not fabricated or reset. The user-specific absolute path, name and keymap are deliberately not copied into this public repository.

The installation workflow explicitly omits obsolete original 3dfx hardware drivers `glide.dll`, `sst1init.dll`, and `fxmemmap.vxd`, rather than shipping or installing historic drivers. An external modern Glide implementation remains necessary.

**Confidence boundary:** the owner's original EN/ES/IT installation was tested in-game. The fully automated new installer, GUI language selection and patch/restore integration remain subject to CI and owner tests.

## Negative FR/DE gameplay result (2026-10-30)

The owner reports: **« l'executable patché ne fonctionne pas pour le FR-DE »**. This is a negative result for the privately supplied FR/DE four-byte FadeLow prototype, not evidence that the original unmodified FR/DE executable or manual installation also fails. The precise symptom (launch failure, crash, black screen, Go freeze, missing videos, etc.) has not yet been supplied.

Reinspection of the locally supplied original and generated candidate confirms:
- Original SHA-256: `eff38cedf2408566060b3c7cf39be59c54d23e67b0a4c7cb0725051aa87e1b72`.
- Candidate SHA-256: `5373c7b24389556b7466932695e7e2d1076531105f9bb879749e783a64898d90`.
- Four changed bytes only, offset `0x48A6D..0x48A70`: `17 B7 D1 38` to `AC C5 27 37`; file size is unchanged, 836608 bytes.
- Both original European fade functions have comparable machine-code structure, but that does not guarantee equivalent game/runtime behavior.
- FR/DE CD resources include `SPLANET/FRENCH.REZ` and `SPLANET/GERMAN.REZ`, while EN/ES/IT includes `ENGLISH.REZ`, `ITALIAN.REZ`, `SPANISH.REZ`. The corresponding `language` configuration must select only files present in the edition.

**Disposition:** maintain the FR/DE safety gate; do not ship or enable the FR/DE patch. Determine whether the *unmodified* original FR/DE EXE runs in the same test installation, then record the failure stage and any error text before proposing a new binary change. Neither the cause nor a fix is established yet.
