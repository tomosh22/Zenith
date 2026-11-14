# SEC design: authenticated update manifests

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It specifies the verification rules every update manifest passes before any byte it covers is mounted, the Ed25519 verification primitive and the selection of its implementation, where the public key comes from and how it reaches the Player, the key lifecycle with its ceremony, its replacement by a new release line and the compromise response, replay and downgrade protection, the Play-delivered pack rule, and the tamper suite. The SPEC baseline is `Zenith2/Docs/Architecture.md` → "Security architecture" ([Z2-N-ARCH-955], [Z2-N-ARCH-956], [Z2-N-ARCH-957], [Z2-N-ARCH-961], [Z2-N-ARCH-966]). The manifest layout and its checks U0 to U11 are PACK's (`Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest", "Update manifest checks"); the verifier and the patch mount are PLAT's (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification"); key custody, the release profile and the signing hand-off are SHIP's (`Zenith2/Docs/Design/SHIP/Release.md` → "Key custody", "Update manifest signing hand-off"); the signer is PACK's (`Zenith2/Docs/Design/PACK/Tooling.md` → "Ed25519 signer"). This file adds the rules those designs execute and the pieces no other area owns.

## Scope of this file

| Topic | Heading |
|---|---|
| the trust chain in one table | "What is signed and what is checked" |
| the ordered rules the verifier implements | "Verification rules" |
| the primitive and its implementation selection | "Ed25519 verification primitive", "Ed25519 implementation selection" |
| where the key comes from | "Public key provenance" |
| generating, retiring, replacing and revoking keys | "Key lifecycle" |
| older manifests | "Replay and downgrade" |
| Play asset packs | "Play-delivered packs" |
| the tests that prove tampering fails | "Tamper suite" |

## What is signed and what is checked

| Link | Bound by | Checked |
|---|---|---|
| the private key | encrypted PKCS#8 at rest under SHIP's custody; used only through `Get-Zenith2UpdateSigningKey` | by SHIP's profile checks at signing time |
| the manifest bytes `[0, total − 64)` | the pure Ed25519 signature (RFC 8032, no context, no pre-hash) in the last 64 bytes | at every start and before every install, by `Zenith_Ed25519Verify` against the compiled-in key |
| each listed file | its size and SHA-256 in the signed file table | at install (B6) and, at each subsequent start, re-hashed when its identity stamp changed ([Z2-N-ARCH-955], [Z2-N-ARCH-966]) |
| the target | release line, version, target build ID (patch) or all-zero build ID and name (DLC), engine cook version, base content hash | at every start |
| the public key | 32 bytes compiled into the game's composition table from `Zenith2/Tools/Security/update_keys.json` | at configure ("Key resolution at configure"), at compile time ("Composition-table key") and at release (SHIP's W10) |

## Verification rules

### Rule order

These are SEC's verification rules; PLAT's `Zenith_DecodeUpdateManifest`, `Zenith_CheckUpdateManifestTarget` and `Zenith_VerifyUpdateManifestFiles` (rows PK2, PK14, PK27 of the cross-area notes) and PACK's applier and Windows DLC mounting execute them in this order, stopping at the first failure:

| # | Rule | U-checks | Reads |
|---|---|---|---|
| V1 | read the manifest whole with a cap of the format's maximum size (`96 + 64 × 65,536 + 1,048,576 + 64` bytes); a larger file is refused before it is read | — | the file size |
| V2 | structural checks at fixed offsets only: at least 225 bytes, magic, format version 1, kind 1 or 2, reserved fields zero, `total bytes` equal to the input length, `96 + 64F + S + 64 = total` in overflow-checked 64-bit arithmetic, F and S within their limits | U1, U2 | header fields at offsets 0 to 95 |
| V3 | the Ed25519 signature over `[0, total − 64)` verifies against the compiled-in key through `Zenith_Ed25519Verify`; no field beyond V2's is interpreted before this rule passes | U3 | the whole manifest |
| V4 | strings valid UTF-8 and in bounds, paths valid single components, sorted, unique, flags consistent | U7 | the file and string tables |
| V5 | release line, version and directory name, target build ID, base content hash | U4, U5, U8, U11 | header |
| V6 | engine cook version | U6 | header |
| V7 | the bundle holds exactly the manifest and the listed regular files (applier and DLC mounting) | U0 | the directory listing, after V3 |
| V8 | the version is newer than every committed one and the install state's floor (applier) | U10 | the install tree |
| V9 | every listed file's size and SHA-256, or its unchanged identity stamp under the conditions C1 to C4 of `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Stamp set confirmed" | U9 | the files |

- V1 to V6 run at every start for every committed patch candidate and every Windows DLC; V9 runs at every start with the stamp lookup.
- The public key is the 32 bytes of the composition table; it is never read from a file, a configuration layer, an environment variable or the command line.
- `Zenith_Ed25519Verify` is the only signature check; no other code compares or interprets the signature bytes.

### Rules per manifest kind

| Rule | PATCH (`update.zman`) | DLC (`dlc.zman`) |
|---|---|---|
| V5 target build | equals the running build's 16-byte build ID | all zero |
| V5 name and directory | the directory name equals `MAJOR.MINOR.PATCH`, major and minor equal the release line | the name equals the directory name `DLC/<name>` |
| V5 base content hash | equals the base package's content hash | 0 |
| V8 newer | greater than every committed version and the install state's floor | revision greater than or equal to the installed revision |
| where it runs | PLAT's patch mount at startup step 5; PACK's applier at startup step 4 | PACK's applier; PACK's Windows DLC mounting at the close of startup step 11 and at each subsequent prologue |

### Rejection and logging

- Any failed rule rejects the whole update: no archive of it is mounted, the previous content stays mounted, and nothing it lists is moved into place ([Z2-N-ARCH-961]).
- The log line names the version or DLC, the rule and the U-check, never a byte of the file: `patch rejected: <version>: V3 (U3 signature)` (PLAT's `vfs.patches_rejected` counts it); PACK's applier logs its `Zenith_PackageError` with the same rule tag.
- A rejection is a `Zenith_Result` error, never an assert ([Z2-N-CONTRACT-472]).

## Ed25519 verification primitive

### Zenith_Ed25519 header

The primitive is SEC's code inside ZenithPlatform, under `Internal/Security/` beside PLAT's verifier, so no third-party Ed25519 header leaves `Internal/` ([Z2-N-ARCH-122]):

```cpp
// Zenith2/Engine/ZenithPlatform/Internal/Security/Zenith_Ed25519.h

inline constexpr uint32 uED25519_PUBLIC_KEY_BYTES = 32;
inline constexpr uint32 uED25519_SIGNATURE_BYTES = 64;
inline constexpr uint64 ulED25519_MAX_MESSAGE_BYTES = 4294967296ull;   // 4 GiB; a manifest is at most 5,243,040 bytes

/// @contract function Zenith_Ed25519Verify
/// Ownership: Borrows the three spans for the call; allocates nothing.
/// Thread affinity: ANY_THREAD. Reentrant; holds no state between calls.
/// Invalidation: None.
/// Units: Byte counts.
/// Errors: Returns false for a wrong span size, an over-long message, an all-zero key, a non-canonical S (S >= L), and
///   any signature the selected implementation rejects; never asserts on its inputs.
/// Version: since Z2 Stage 6. ZenithPlatform-internal; callers are PLAT's update-manifest verifier and its tests.
/// @endcontract
bool Zenith_Ed25519Verify(Zenith_ByteSpan xMessage, Zenith_ByteSpan xSignature, Zenith_ByteSpan xPublicKey);
```

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_UpdateKeyRules.h (public: the generated composition TU includes it)

/// @contract value Zenith_UpdateKeyRules
/// Ownership: Compile-time constants and consteval predicates only; nothing is emitted into an image.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 6. A new published test key is appended; none is removed.
/// @endcontract
inline constexpr uint32 uZENITH_UPDATE_KEY_BYTES = 32;
inline constexpr uint32 uZENITH_PUBLISHED_TEST_KEY_COUNT = 3;
inline constexpr uint8 aauZENITH_PUBLISHED_TEST_KEYS[uZENITH_PUBLISHED_TEST_KEY_COUNT][uZENITH_UPDATE_KEY_BYTES] = { /* "Published test keys" */ };

consteval bool Zenith_IsPublishedTestUpdateKey(const uint8 (&auKey)[uZENITH_UPDATE_KEY_BYTES]);
consteval bool Zenith_IsZeroUpdateKey(const uint8 (&auKey)[uZENITH_UPDATE_KEY_BYTES]);
```

The two predicates are `consteval`, so the test-key bytes are used only in constant expressions and never reach an image through them; the forbidden-string scan proves it (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Initial string entries").

### Primitive algorithm

1. Return false when `xPublicKey.GetSize() != 32`, `xSignature.GetSize() != 64` or `xMessage.GetSize() > ulED25519_MAX_MESSAGE_BYTES`.
2. Return false when the 32 key bytes are all zero (the fail-closed key of "Fail-closed defaults"). The comparison is an OR over the 32 bytes.
3. Return false when S, the signature's bytes 32 to 63 read as a little-endian 256-bit integer, is greater than or equal to L = 2^252 + 27742317777372353535851937790883648493, whose little-endian bytes are `edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010`; the comparison runs from byte 63 down to byte 32 and stops at the first difference.
4. Call the implementation the selection ruling admits ("Ed25519 implementation selection"): Monocypher's `crypto_ed25519_check(signature, public_key, message, message_size)` from `optional/monocypher-ed25519.h`, accepted when it returns 0; or libsodium's `crypto_sign_verify_detached(signature, message, message_size, public_key)`, accepted when it returns 0. The call keeps the library's own parameter types (Monocypher's `const uint8_t*` and `size_t`; libsodium's `const unsigned char*` and `unsigned long long` length), converted from the spans inside `Internal/Security/`. A Zenith-owned implementation, if the ruling chooses one, is specified by the ruling's design patch.
5. Return the result. Verification takes public inputs only, so it is not required to be constant time; its cost is one double-scalar multiplication plus SHA-512 over `R ‖ A ‖ M` (linear in the message).

Step 3 makes the canonical-S rule hold whichever implementation is admitted; step 2 makes a Shipping build without a release key refuse every update even if the implementation accepted a degenerate key.

### Published test keys

`aauZENITH_PUBLISHED_TEST_KEYS` holds the three public keys of RFC 8032 § 7.1 (Ed25519), copied from the RFC; a ticket's reviewer compares each against the RFC (review focus):

| Vector | Seed | Public key | Message | Signature |
|---|---|---|---|---|
| TEST 1 | `9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60` | `d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a` | empty | `e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b` |
| TEST 2 | `4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb` | `3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c` | `72` | `92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00` |
| TEST 3 | `c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7` | `fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025` | `af82` | `6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a` |

TEST 1's seed, key and signature equal the values PACK and SHIP already use (`Zenith2/Docs/Design/PACK/Tooling.md` → "Ed25519 signer"; `Zenith2/Docs/Design/SHIP/Release.md` → "Update manifest signing hand-off"). The seeds and signatures are test data only and appear in no runtime header.

- **Which vector a test signs with.** SEC's composed tests sign with TEST 1 and use TEST 2 as the wrong key; PACK's applier and integration tests sign with TEST 2 and use TEST 3 as the wrong key (`Zenith2/Docs/Design/PACK/Tests.md`), and `Z2T-PLAT-092`'s fuzz target verifies against TEST 2's public key. Every one of them passes the key to the verifier, the applier or the mount explicitly, so the conventions never meet; the compiled-in development default (TEST 1, "Fail-closed defaults") is used only by a test that verifies through a composed Player without passing a key, and no test does that today.
- **A committed seed is a published key.** A diff that commits an Ed25519 seed or private key anywhere in the repository (a test, a fixture, a document) appends its public key to `aauZENITH_PUBLISHED_TEST_KEYS` (raising `uZENITH_PUBLISHED_TEST_KEY_COUNT`) and to `shipping_forbidden_strings.json` (through the PROC ticket that maintains the list, hand-off SEC-P6) in the same change, so no key whose private half is public can reach a Shipping key record or a Shipping image.
- **One list of published keys.** The list is the `hex` entries of `Zenith2/Tools/Gates/shipping_forbidden_strings.json` whose `id` ends `-public-key`. The configure function and `Test-Zenith2UpdateKeys.ps1` read their refusal list from it rather than holding a copy, and the script-mode test `CMake_Sec_PublishedKeysAgree` reads that file and the text of `Zenith_UpdateKeyRules.h` and fails unless the C++ array holds exactly the same keys in the same order; so a key published in one place is refused everywhere.
- **The human check of the vectors.** The values above were recomputed independently in review, which proves them consistent with each other, not that RFC 8032 § 7.1 prints them. SEC's A5 ticket batch carries one `needs-human` ticket in the Stage 6 sprint, before the API-only slice, in which a person compares the three seeds, public keys, messages and signatures against the text of RFC 8032 § 7.1 and records the comparison in the ticket's result.

## Ed25519 implementation selection

The Stage 1 security-architecture ruling admits the implementation (`Zenith2/Docs/Architecture.md` → "Dependencies admitted by open selections"); this is its selection, run as Z2-D-084 requires: an ordinary harness ticket, then the `needs-human` ruling that appends its entry to `Zenith2/Docs/DecisionLog.md` in its own diff (Z2-D-097).

### Candidates

| Candidate | Pinned version | Manifest name | Integration | Entry point |
|---|---|---|---|---|
| Monocypher | 4.0.2 | `monocypher` | `zenith-wrapper` over `src/monocypher.c` and `src/optional/monocypher-ed25519.c` | `crypto_ed25519_check` |
| libsodium | 1.0.20 | `libsodium` | `zenith-wrapper` over the `crypto_sign/ed25519` and `crypto_hash/sha512` sources and their dependencies | `crypto_sign_verify_detached` |
| a Zenith-owned verifier | — | — | ZenithPlatform `Internal/Security/` | `Zenith_Ed25519Verify` itself |

### Criteria

| # | Criterion | Measured by |
|---|---|---|
| K1 | a licence the inventory can ship (CC0, BSD-2-Clause, ISC or equivalent) | the manifest's SPDX field |
| K2 | an independent published security audit of the library's version line | the audit report's reference and date, recorded in the ruling (recorded, not deciding) |
| K3 | builds with exceptions and RTTI off and the third-party flag rows, warnings as errors in its wrapper ([Z2-N-ARCH-121]) | the harness build on `win`, `win-asan`, `linux-tsan`, `android-arm64` |
| K4 | accepts RFC 8032 TEST 1 to TEST 3 and rejects every case of `Test_Ed25519_RejectsMutations` | the harness's vector run |
| K5 | no global state and no initialisation call: libsodium meets K5 only if the ruling records that `crypto_sign_verify_detached` and every function it reaches neither require `sodium_init` nor read state it sets | source inspection recorded in the ruling |
| K6 | code added to the Shipping Player (Windows and Android) | the linker map attribution of SHIP's size report |
| K7 | runs clean under ASan, TSan with UBSan, and HWASan | the harness under the sanitizer presets and on the S24 |
| K8 | median verify time of a 4 KiB message on the reference desktop and the S24, over 10 runs of 10,000 verifications | the harness |

### Acceptance rule

The first candidate in the order Monocypher, libsodium that meets K1, K3, K4, K5 and K7 is admitted, with K2, K6 and K8 recorded but not deciding: K6 and K8 because both candidates verify in well under a millisecond and add tens of kilobytes, and K2 because neither may have an audit of the exact pinned line, and a recorded audit of the library's line (with its date and how far the pinned version is from the audited one) is what `Zenith2/Docs/Architecture.md` → "Dependencies admitted by open selections" means by an audited library. So the Zenith-owned verifier is reachable only when both candidates fail K1, K3, K4, K5 or K7, and then the ruling's diff carries its design. The admitted entry's `owner` becomes `ZenithPlatform` and its `usage` `runtime` in the same diff ([Z2-N-ARCH-121]); the other candidate's entry is removed.

### Selection harness

`Zenith2/Benchmarks/Ed25519/` (`zenith_add_benchmark(Ed25519 THIRD_PARTY monocypher libsodium)`, both entries `owner: Zenith_Bench_Ed25519`, `usage: tools`). SHIP pins the two candidates: a `needs-human` Stage 1 SHIP ticket adds `monocypher` 4.0.2 and `libsodium` 1.0.20 to `Zenith2/ThirdParty/manifest.json` as harness candidates (shape `zenith-wrapper`, owner `Zenith_Bench_Ed25519`, `usage: tools`; `Zenith2/Docs/Design/SHIP/Manifest.md` → "Selection-candidate entries"), and the harness ticket is blocked by it (row SEC15); after the ruling, SHIP's pin ticket re-owns the admitted entry and removes the other, as "Acceptance rule" states:

- `Internal/Zenith_Bench_Ed25519Main.cpp` runs, per candidate: the three RFC vectors; the mutation set of `Test_Ed25519_RejectsMutations`; 10 runs of 10,000 verifications of a 4 KiB message signed with TEST 1's seed; and reports `Zenith2/out/<checkout-hash>/bench/Ed25519.json` (`zenith2.bench-ed25519` version 1: `{ "candidates": [ { "name", "version", "vectorsPassed", "mutationsRejected", "medianMicroseconds", "p95Microseconds" } ] }`).
- `--smoke` runs one verification per candidate and the vector set, as the registered CTest smoke in the `Test` step.
- The ruling runs it on the reference desktop and the S24, commits `Zenith2/Docs/Research/Security/Ed25519Selection.md` with the measurements and each criterion's outcome, and records the ruling.

## Public key provenance

### update_keys.json

`Zenith2/Tools/Security/update_keys.json` is the committed public record of every update key (public halves only):

```json
{
    "schema": "zenith2.update-keys",
    "schemaVersion": 1,
    "keys": [
        {
            "game": "OpenWorldSlice",
            "releaseLine": "1.0",
            "publicKey": "<base64 of 32 bytes>",
            "publicKeyHex": "<64 lower-case hex digits: the same 32 bytes>",
            "publicKeySha256": "<64 lower-case hex digits>",
            "status": "active",
            "created": "2027-03-01",
            "revokedReason": null
        }
    ]
}
```

`publicKeyHex` exists because CMake can read strings but cannot decode base64 or hash raw bytes; `publicKey` (base64) is the form SHIP's release profile and W10 compare. Each rule is checked where it can be expressed: the configure function checks the string rules, and the PowerShell key check (`Zenith2/Tools/Gates/Security/Test-Zenith2UpdateKeys.ps1`, protected, hand-off SEC-P9, using `[Convert]::FromBase64String` and `[System.Security.Cryptography.SHA256]`) checks every rule; the configure runs the PowerShell check too ("Key resolution at configure"), and SecurityConformance check SC14 runs it on every gate run:

| Rule | Checked by | Refusal message |
|---|---|---|
| `game` names a directory `Zenith2/Games/<game>/` | configure, PowerShell | `update keys: <game>: unknown game` |
| `releaseLine` is `MAJOR.MINOR`, each 0 to 65,535 without a leading zero | configure, PowerShell | `update keys: <game>: bad release line <value>` |
| `publicKeyHex` is 64 lower-case hex digits, not 64 zeros, and not a published key of the list "Published test keys" defines (read from `shipping_forbidden_strings.json`) | configure, PowerShell | `update keys: <game> <line>: bad public key` |
| `publicKey` decodes to exactly 32 bytes equal to the bytes of `publicKeyHex` | PowerShell | `update keys: <game> <line>: hex and base64 disagree` |
| `publicKeySha256` is the SHA-256 of the 32 bytes | PowerShell | `update keys: <game> <line>: digest mismatch` |
| `status` is `active`, `retired` or `revoked`; at most one `active` record per (`game`, `releaseLine`); no two records share a `publicKeyHex` | configure, PowerShell | `update keys: <game> <line>: two active keys`, `update keys: <game> <line>: key reused` |
| `revokedReason` is a non-empty string exactly when `status` is `revoked` | configure, PowerShell | `update keys: <game> <line>: revocation without reason` |
| `created` is an ISO date | configure, PowerShell | `update keys: <game> <line>: bad date` |
| records sorted by `game` (byte order), then `releaseLine` compared as the pair (major, minor) of integers (so `1.9` precedes `1.10`), then `created` | configure, PowerShell | `update keys: records out of order at <game> <line>` |

The file is committed at Stage 2 with `"keys": []`, the schema's valid empty record set, together with the other registries, so SecurityConformance check SC14 and every game's configure are green from their first run: until a release line's key ceremony commits its record, every Shipping build carries the zero key ("Fail-closed defaults") and every development build the published TEST 1 key, and real keys arrive only with the release line they sign (Stage 6). The file joins the Z2 categories' `reviewPaths` (hand-off SEC-P8), so a change to it requires a revision-bound review; SHIP's W10 (the release profile's key occurs exactly once in the image) catches a record that disagrees with the operator's key.

### Key resolution at configure

`zenith_security_resolve_update_key(<Game> <ReleaseLine> <OutShippingInc> <OutDevelopmentInc>)` in `Zenith2/CMake/ZenithSecurity.cmake`, called by `zenith_add_game` for every game (row SEC5 (a)):

1. Register `${ZENITH_ROOT}/Tools/Security/update_keys.json`, `${ZENITH_ROOT}/Tools/Gates/Security/Test-Zenith2UpdateKeys.ps1` and `${ZENITH_ROOT}/Tools/Gates/shipping_forbidden_strings.json` with `set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS …)`, so a key ceremony, a revocation, a new published key or a change to the check re-runs the configure (Ninja's automatic re-run, Z2-D-001) and the `.inc` files never go stale. Read `update_keys.json` and the published keys of `shipping_forbidden_strings.json` with `file(READ)` and `string(JSON)`; check every record by the rules the table marks "configure"; any failure is `message(FATAL_ERROR …)` with the rule's message.
2. Run `${ZENITH_PWSH} -NoProfile -File ${ZENITH_ROOT}/Tools/Gates/Security/Test-Zenith2UpdateKeys.ps1 -Path ${ZENITH_ROOT}/Tools/Security/update_keys.json -StringList ${ZENITH_ROOT}/Tools/Gates/shipping_forbidden_strings.json` through `execute_process(… TIMEOUT 60 RESULT_VARIABLE … OUTPUT_VARIABLE … OUTPUT_STRIP_TRAILING_WHITESPACE)`, every path absolute so the result never depends on the working directory, once per configure (not per game; the result is cached in the global property `ZENITH_UPDATE_KEYS_CHECKED` keyed by the three files' `file(SHA256)`); a non-zero exit or a timeout is `message(FATAL_ERROR …)` with the script's first output line. `ZENITH_PWSH` is the pinned pwsh that `zenith_register_pwsh_test` runs; when it is absent the configure fails with `Zenith2: update keys: pwsh 7 is required to configure a game`, because the function runs only inside `zenith_add_game` for a preset whose target OS is in the game's `platforms`, and every such preset is configured on a Windows host where pwsh 7 is a setup prerequisite (`Setup-Zenith2.ps1`); the check is never skipped for a game. The script is self-contained: it imports no module (neither `Zenith2/Tools/Gates/Common/`, which needs the gate environment and `git`, nor `Zenith2SecurityGates.psm1`, whose `Read-Zenith2UpdateKeys` invokes this script rather than re-implementing it), so a configure outside a gate run behaves exactly as one inside it.
3. Find the `active` record for (`<Game>`, `<ReleaseLine>`); its `publicKeyHex` is the Shipping key; with none, the Shipping key is 64 zero digits and the configure prints `Zenith2: <Game> <ReleaseLine>: no active update key; Shipping builds refuse every patch and DLC` as a `WARNING`.
4. The development key is RFC 8032 TEST 1's public key, unless the cache variable `ZENITH_UPDATE_KEY_IN_DEVELOPMENT` is `RELEASE`, which selects the Shipping key for `Debug` and `Release` too (to test a release-signed patch on a development build).
5. Write both keys, unconditionally and in every preset, as two files under `${ZENITH_OUT}/Games/<Game>/<configure-preset>/`: `Zenith_UpdateKey.Shipping.generated.inc` and `Zenith_UpdateKey.Development.generated.inc`, each holding one brace-enclosed initialiser of 32 `0x..` bytes and nothing else, written with `file(WRITE)` only when the content differs (so an unchanged key rebuilds nothing). No `file(GENERATE)` content varies by configuration.
6. Set both output variables to the two files' paths for MOD's `zenith_add_game`, which adds `target_compile_definitions(<Game> PRIVATE "ZENITH_UPDATE_KEY_INC=\"$<IF:$<CONFIG:Shipping,ShippingTest>,Zenith_UpdateKey.Shipping.generated.inc,Zenith_UpdateKey.Development.generated.inc>\"")` and the directory to the game target's private include path (row SEC5 (a)).

The define names a file to include and selects no code: it is not a [Z2-N-ARCH-189] configuration define, it is defined in every configuration, and the generated TU includes it unconditionally with `#include ZENITH_UPDATE_KEY_INC`, so there is no preprocessor conditional and no layout change (rule F11).

### Composition-table key

<!-- assumption: pending MOD -->
SEC assumes (row SEC5 (a)) that MOD's generated composition table (`Zenith2/Docs/Architecture/CompositionRoot.md` → "The generated composition table") gains `uint8 m_auUpdatePublicKey[32]`, initialised from `inline constexpr uint8 kauZenithUpdatePublicKey[32] =` followed by `#include ZENITH_UPDATE_KEY_INC` and `;` in the generated TU (one TU per configure preset, identical in every configuration), and that the generated TU includes `ZenithPlatform/Zenith_UpdateKeyRules.h` and states `static_assert(!Zenith_IsShippingBuild() || !Zenith_IsPublishedTestUpdateKey(kauZenithUpdatePublicKey), "Zenith2: a Shipping build carries a published test update key");`, and that the composition root passes the 32 bytes to `Zenith_VirtualFileSystemDesc::m_xUpdatePublicKey`, `Zenith_UpdateApplierDesc::m_xUpdatePublicKey` and PACK's DLC mounting. The key is therefore per game and per configuration, selected by the compile definition rather than by generated content, compiled once into the game's own TU, never into a shared engine library.
<!-- /assumption -->

### Fail-closed defaults

| Build | Key | Effect |
|---|---|---|
| Shipping with an active record | the record's key | patches and DLC signed by its private half verify |
| Shipping without an active record | 32 zero bytes | step 2 of "Primitive algorithm" refuses every manifest; patches are rejected, DLC stays unmounted, the base package runs |
| Shipping with a published test key | impossible | the configure refuses the record (steps 1 and 2; SC14) and the static assertion fails the compile |
| `Debug`, `Release`, sanitizer configurations | RFC 8032 TEST 1's public key | patches signed in process by tests, and by PACK's tooling with a test profile, verify; residual R8 |

## Key lifecycle

### Key states

| State | Event | Next | Action |
|---|---|---|---|
| (none) | key ceremony completed | GENERATED | the encrypted key and public record exist on the ceremony media |
| GENERATED | public record committed with `status` `active` | ACTIVE | builds of (game, release line) compile it into Shipping |
| ACTIVE | the release line's last release is out of support | RETIRED | `status` `retired`; the private key is kept offline for 1 year, then destroyed |
| ACTIVE, RETIRED | compromise suspected or confirmed | REVOKED | "Compromise response" |

One key serves one (game, release line) for the line's whole life: there is no key change inside a line ("Key replacement").

### Key ceremony

Run by a person (a `needs-human` ticket per game and release line), on an offline Windows machine whose `Setup-Zenith2.ps1 -Check` passes:

1. Generate a password of at least 24 characters in a password manager and place it in the environment variable `ZENITH2_UPDATE_KEY_PASSWORD` of the session.
2. Run `pwsh -NoProfile -File Zenith2/Tools/Security/New-Zenith2UpdateKey.ps1 -Game <Game> -ReleaseLine <MAJOR.MINOR> -PasswordEnv ZENITH2_UPDATE_KEY_PASSWORD -Out <directory on removable media>`. It runs "Ed25519KeyCeremony tool" and writes `update-ed25519.p8.pem` (encrypted) and `update-ed25519.pub.json` (`{ game, releaseLine, publicKey, publicKeyHex, publicKeySha256, created }`), refusing an existing file.
3. Prove the key is usable: point a release profile's `updateManifest` at the file and run SHIP's `Get-Zenith2UpdateSigningKey`, which decrypts it and has the pinned JDK prove the seed matches `publicKey`; it must succeed.
4. Copy the encrypted file to a second removable medium; store the two media in two separate locations; store the password in two separate password-manager vaults.
5. Commit the public record to `update_keys.json` with `status` `active` (the ticket's diff; review is required by SEC-P8).
6. Configure the release machine's profile (`encryptedPrivateKeyFile`, `publicKey`, `publicKeySha256`) per SHIP.
7. Clear the environment variable and close the session; nothing in plaintext was ever written.

### Ed25519KeyCeremony tool

`Zenith2/Tools/Security/Ed25519KeyCeremony.java`, run by the pinned JDK's source launcher (never compiled by CMake, never part of a target), invoked only by `New-Zenith2UpdateKey.ps1`:

```text
<java> -Xshare:off Zenith2/Tools/Security/Ed25519KeyCeremony.java <password env name> <output PEM path>
	stdout: one line of JSON {"publicKey":"<base64>","publicKeyHex":"<64 hex>","publicKeySha256":"<64 hex>"}
	exit:   0 written · 2 usage · 3 password variable absent or shorter than 16 characters · 4 cryptographic failure · 5 output exists
```

1. Read the password from the named environment variable into a `char[]`; exit 3 when absent or shorter than 16 characters.
2. `KeyPairGenerator.getInstance("Ed25519")` initialised with `SecureRandom.getInstanceStrong()`; generate a key pair.
3. The PKCS#8 encoding of the private key (`getEncoded()`, the RFC 8410 `CurvePrivateKey` form SHIP's decryption expects) is encrypted under PBES2: a 16-byte salt and a 16-byte IV from the same `SecureRandom`, `PBEParameterSpec(salt, 600000, new IvParameterSpec(iv))`, the algorithm `PBEWithHmacSHA256AndAES_256` (PBKDF2-HMAC-SHA256, AES-256-CBC), and wrapped as `EncryptedPrivateKeyInfo`.
4. Write its DER as PEM (`-----BEGIN ENCRYPTED PRIVATE KEY-----`, 64-character base64 lines, LF) to the output path with `StandardOpenOption.CREATE_NEW`; exit 5 when the file exists.
5. The public key is the last 32 bytes of the 44-byte SubjectPublicKeyInfo (`getPublic().getEncoded()`, prefix `302a300506032b6570032100`); print its base64, its lower-case hex and its SHA-256.
6. Zero the password array and the PKCS#8 byte array before exit; any exception is exit 4 with one line on standard error.

`Pwsh_Sec_KeyCeremony_RoundTrip` proves steps 2 to 5 produce a file SHIP's decryption accepts ("Zenith2/Docs/Design/SEC/Tests.md" lists it under "PowerShell tests").

### Key replacement

A key is never rotated inside a release line. A patch targets one build, which verifies against the key compiled into it, and a DLC is valid for every build of its line with one signature, so a new key inside a line would leave older builds unable to verify new patches or any re-signed DLC. Every key replacement, planned or after a compromise, therefore starts a new release line: a new `MAJOR.MINOR`, a new ceremony, a new `update_keys.json` record, a new base build, and each DLC that should reach the new line re-signed in a new `dlc.zman` whose release-line field is the new line. The old line keeps its key until its record is retired or revoked; its players move to the new line by installing the new base build.

### Compromise response

1. Set the record's `status` to `revoked` with the reason, in a reviewed diff; no further signature is made with the key.
2. Run a ceremony for a new key and release a new base build of a new release line ("Key replacement"), so updated players verify only the new key.
3. Re-sign every DLC that should reach players of the new line, as DLC of the new line, with the new key.
4. State in the release notes that older builds must be updated; builds already in players' hands still carry the compromised key and accept patches it signs for their build ID (residual R6).
5. Audit the signing machine and the profile's custody per SHIP before the next signature.

## Replay and downgrade

- A patch manifest is bound to one build ID (V5), so a manifest for another build is refused.
- An older patch of the running line is refused by V8 (U10) against the committed directories and the install state's floor; deleting the install state resets the floor to the committed directories, and the install record still classifies the downgrade and reports it (residual R7).
- A DLC manifest is valid for every build of its release line by design (all-zero build ID); a DLC revision below the installed one is refused.
- `Test_SecUpdate_ReplayedOlderPatchRefused` and PACK's applier tests are the evidence.

## Play-delivered packs

Fast-follow and on-demand packs are signed by Play's app-signing chain and verified against the SHA-256 the base `package.zpkm` records for them before they mount, once, with the stamp skip of PACK's "Verified cache" item 3 ([Z2-N-ARCH-956]); a mismatch leaves the pack unmounted and reports it to the game. SEC adds no rule beyond the stamp-set confirmation, which applies to pack archives as it applies to patch files. Install-time packs ship inside the APK splits and are boundary B1.

## Tamper suite

`Zenith2/Tests/Security/Zenith_SecUpdate_Tests.cpp` (suite `Zenith_Integration_Security`) builds a valid patch bundle and a valid DLC in a temporary install tree through PACK's memory and host install file systems, signs them in process with RFC 8032 TEST 1's seed through `Zenith_PackageTestSigner.h`, and verifies each tampered variant against TEST 1's public key through the real verifier, applier and mounts; each case asserts the rejection, the rule tag in the log, and that the previous content stays mounted:

| Test | Variant | Expected rule |
|---|---|---|
| `Test_SecUpdate_FlippedHeaderByteRefused` | one bit of byte 40 (version patch) flipped | V3 |
| `Test_SecUpdate_FlippedFileTableByteRefused` | one bit of a listed SHA-256 flipped | V3 |
| `Test_SecUpdate_FlippedStringByteRefused` | one bit of a file name flipped | V3 |
| `Test_SecUpdate_FlippedSignatureByteRefused` | one bit of R flipped; one bit of S flipped | V3 |
| `Test_SecUpdate_WrongKeyRefused` | signed with TEST 2's seed | V3 |
| `Test_SecUpdate_NonCanonicalSignatureRefused` | S replaced by S + L | V3 (step 3 of "Primitive algorithm") |
| `Test_SecUpdate_ZeroKeyRefusesEverything` | a valid manifest verified with the zero key | V3 |
| `Test_SecUpdate_TruncatedRefused` | the last byte removed | V2 |
| `Test_SecUpdate_AppendedByteRefused` | one byte appended after the signature | V2 |
| `Test_SecUpdate_WrongBuildRefused` | target build ID of another build, correctly signed | V5 |
| `Test_SecUpdate_WrongReleaseLineRefused` | release line 1.1 on a 1.0 build, correctly signed | V5 |
| `Test_SecUpdate_ReplayedOlderPatchRefused` | version 1.0.1 after 1.0.2 is committed | V8 |
| `Test_SecUpdate_ArchiveBytesChangedRefused` | one byte of a listed archive changed, manifest intact | V9 |
| `Test_SecUpdate_ArchivesSwappedRefused` | two listed archives' contents exchanged | V9 |
| `Test_SecUpdate_UnlistedFileRefused` | an extra file in the bundle | V7 |
| `Test_SecUpdate_StampFieldChangeRehashes` | after a verified install, for each of the five stamp fields a record differing in that field alone (one record byte flipped with PACK's test encoder) beside a changed file: the file is re-hashed and refused | V9 |
| `Test_SecUpdate_DlcOtherNameRefused` | a correctly signed `dlc.zman` copied into another `DLC/<name>/` | V5 |
