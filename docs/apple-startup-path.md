# Apple startup path conversion

`apple/StartupPath.h` normalizes host `/` separators in the executable token before the Mac/iOS frontends call `Player::init`. INI Start values, program-picker results, and Mac startup overrides all use this boundary. Existing DOS backslashes, inline slash options and the separate Args field are preserved. Empty Start remains empty for disk boot.

The shared core and Windows frontend are unchanged. The iOS repository uses the identical helper and restores its earlier core-side SuperDepth patch to upstream. No BIOS or ROM is required.

`start_path_smoke` executes nested EXE, COM and BAT files through the Apple helper and unmodified DOS core, verifies sibling data access and slash arguments, and repeats with the save overlay enabled. The fixtures are original synthetic programs; no third-party game data is bundled.

Validation after refactoring (2026-11-05): Mac Universal build includes arm64 and x86_64; Mac CTest 5/5, iOS repository native CTest 8/8. The Mac app started `aa3558f/DEPTH.EXE` and displayed the SuperDepth title after 650 frames. The iPhone 17 simulator app built with Xcode 27.2 advanced 2,100 guest frames into gameplay (guestQuit=0, 15 visible colors, generated PCM peak 2457). This is early-game verification, not a full-game or physical-device test. No store submission was changed.
