# European FadeLow static research — two Scorched Planet 3dfx editions

Status: **exact-original static analysis confirmed; proposed binary fixes are EXPERIMENTAL, not user-validated in gameplay**. 2026-10-30.

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

- The *shipping* Windows x64 Rust GUI must **not enable FadeLow for either European executable yet**. Only the previously user-validated US exact hashes are patchable.
- Trial binaries are separate private output files, supplied only to the original owner via this conversation. **Do not add original or modified commercial EXEs to GitHub, CI, or a public release**.
- Test using an isolated copy of each installed edition, keeping its original `SPVOODOO.EXE` untouched. Launch the alternate-name trial EXE via Lutris/Wine or Windows, repeat the previous Go/mission fade scenario and verify menu, mission transitions, playback, input and audio. Revert simply by selecting the original EXE again.
- Only after the owner's positive gameplay validation should the project consider adding these two exact-hash variants to the Rust production patch/restore paths. Existing backup, overwrite protection, version checks and idempotence must be preserved. Until then the GUI remains deliberately nonpatching for European builds.

No modification to `main`; research and proposed changes are kept in draft PR #5.

legluondunet assisted by ChatGPT (OpenAI).
