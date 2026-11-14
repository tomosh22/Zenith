# SEC design: tests

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It lists every test SEC's tickets write, with its file, the case it asserts and the gate step that runs it, and the acceptance items SEC owns. Owners' tests that SEC's matrices cite are listed in the owners' designs and are not repeated here. Every name is unique programme-wide (ruling TEST-NAME-UNIQUE): SEC's names carry `Ed25519`, `UpdateKeyRules`, `Sec` or `Pwsh_Sec`.

## Scope of this file

| Topic | Heading |
|---|---|
| targets and the steps that run them | "Test targets and gate steps" |
| the primitive | "Ed25519 primitive tests" |
| the integration suite | "Integration suite tests" |
| tooling | "PowerShell tests", "CMake script-mode tests" |
| the protected step's fixtures | "Gate-step fixture tests" |
| stage acceptance | "Acceptance items" |
| what a reviewer probes | "Review focus for SEC tickets" |

## Test targets and gate steps

| Target or file | Location | Presets | Gate step |
|---|---|---|---|
| `ZenithPlatform_Tests` (SEC's files) | `Zenith2/Engine/ZenithPlatform/Tests/Zenith_Ed25519_Tests.cpp`, `Zenith_UpdateKeyRules_Tests.cpp` | `win`, `win-asan`, `linux-tsan`, Android test APK | `Test`; `Sanitizers` |
| `Zenith_Fuzz_Ed25519Verify` | `Zenith2/Engine/ZenithPlatform/Tests/Fuzz/Ed25519Verify.cpp`, dictionary `Ed25519Verify.dict` | `win-asan` | `Sanitizers -Preset win-asan` (smoke) |
| `Zenith_Integration_Security` | `Zenith2/Tests/Security/` (`zenith_add_integration_tests(Security DEPENDS ZenithCore ZenithPlatform ZenithCompression ZenithCompression_Zlib ZenithCompression_Zstd ZenithAssets ZenithPackage ZenithSave ZenithScript)`, all `ZENITH_ALL`, so the suite exists in `win`, `win-asan` and `linux-tsan`) | `win`, `win-asan`, `linux-tsan`, `win-shipping-test` | `Test`; `Sanitizers` |
| PowerShell assert script | `Zenith2/Tools/Security/Tests/Test-Zenith2Security.ps1`, registered with `zenith_register_pwsh_test` in SEC's block of `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | `win` | `Test` (label `z2-pwsh`) |
| CMake script-mode tests | `Zenith2/CMake/Tests/Security/*.cmake`, registered with `zenith_register_script_test` | every test preset, except `CMake_Sec_UpdateKeyResolution`, which runs pwsh and is therefore registered by SEC's registry block only when `ZENITH_PRESET` is `win`, the one preset where SHIP registers tests that run pwsh | `Test` (label `z2-cmake`) |
| `Test-SecurityConformance.ps1` | `Zenith2/Tools/Gates/Tests/` (protected, SEC-P1) | host `pwsh` | the `Coverage` step's script run |
| `Test-Security.ps1` | `Zenith2/Tools/Gates/Tests/Security/` (protected, SEC-P9), over fixtures under `Zenith2/Tools/Gates/Tests/Security/Fixtures/` | host `pwsh` | PROC's `Run-GateSelfTests.ps1`, which runs every `Tests/*/Test-*.ps1`, and the `Coverage` step's script run |

## Ed25519 primitive tests

| Test | Asserts |
|---|---|
| `Test_Ed25519_Rfc8032Vectors` | TEST 1, TEST 2 and TEST 3 of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Published test keys" verify with their public keys |
| `Test_Ed25519_RejectsMutations` | for each RFC vector: each of the 512 signature bits flipped alone (512 cases), each of the 256 key bits flipped alone (256 cases) and each of the 8 bits of the first, middle and last message byte flipped alone (TEST 1's empty message has none; a one-byte message counts its byte once) is rejected; S replaced by S + L (a little-endian add of L's bytes, written in the test) and by L itself is rejected; an all-zero signature is rejected; TEST 1's signature with TEST 2's key is rejected |
| `Test_Ed25519_SizesRefused` | key spans of 0, 31 and 33 bytes, signature spans of 0, 63 and 65 bytes, and a message span whose size is `ulED25519_MAX_MESSAGE_BYTES + 1` (a span over a 1-byte buffer with that size, never read, because the size check comes first) each return false |
| `Test_Ed25519_ZeroKeyRefused` | TEST 1's signature and message verified against 32 zero bytes return false |
| `Test_Ed25519_NoAllocation` | 1,000 verifications leave the test heap's statistics unchanged |
| `Test_UpdateKeyRules_PublishedKeysDetected` | `static_assert(Zenith_IsPublishedTestUpdateKey(…))` for each of the three keys and `static_assert(!Zenith_IsPublishedTestUpdateKey(…))` for a key differing from TEST 1's in its last byte, `static_assert(Zenith_IsZeroUpdateKey(…))` for 32 zero bytes; the test case body checks `uZENITH_PUBLISHED_TEST_KEY_COUNT == 3` |

`Zenith_Fuzz_Ed25519Verify` passes the input's first 64 bytes (zero-padded) as a signature and the rest as the message to `Zenith_Ed25519Verify` with TEST 1's public key and asserts only that the call returns; its committed text seed corpus is empty, and its fixture-set seeds are TEST 1's signature followed by the empty message and one mutation per `Test_Ed25519_RejectsMutations` class.

## Integration suite tests

All run headless on every host preset of the suite; each fixture is built in the test from the documented layouts, so nothing binary is committed (ruling Q55 (b)).

### Path-matrix composition tests

`Zenith2/Tests/Security/Zenith_SecPath_Tests.cpp`: the tests of `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Composed traversal tests", each asserting what that table states, plus:

| Test | Asserts |
|---|---|
| `Test_SecPath_EveryCorpusSeedThroughResolve` | every seed file of PLAT's `Corpus/VirtualPath/` read as bytes either fails `Zenith_ParseVirtualPath` with the seed table's error or, when accepted, resolves against a temporary package root holding links to outside it without opening a file outside the root |
| `Test_SecEd25519_DifferentialAgainstTestSigner` | 1,000 messages of lengths 0 to 4,096 bytes, generated from `Zenith_RandomSource` with a fixed seed, signed by PACK's header-only RFC 8032 signer `Zenith_PackageTestSigner.h` with TEST 1's and TEST 3's seeds, verify through `Zenith_Ed25519Verify`; each signature with one random bit flipped does not |

### Update tamper tests

`Zenith2/Tests/Security/Zenith_SecUpdate_Tests.cpp`: the seventeen tests of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Tamper suite", each asserting the rejection with its rule tag, the log line `patch rejected: <version>: <rule>` or PACK's equivalent, and that the previously mounted content is still the content served for a probe path.

### Shipping behaviour tests

`Zenith2/Tests/Security/Zenith_SecShipping_Tests.cpp`, built in the `win-shipping-test` preset (`Zenith_IsShippingBuild()` true) and, with the opposite expectation, in `win`:

| Test | Asserts in ShippingTest | Asserts in win |
|---|---|---|
| `Test_SecShipping_ListenerRefusedForEveryService` | `Zenith_DevListener::Create` with service names `dap`, `profiler` and `livelink` returns `DEV_SERVICE_ERROR_NOT_IN_SHIPPING`; `Zenith_DevClient::ConnectLoopback` returns the same | a listener opens on an ephemeral loopback port and refuses a wrong token |
| `Test_SecShipping_DevelopmentInputsRefused` | `AddSource(MOUNT_KIND_LOOSE, …)` returns `FILE_ERROR_MOUNT_NOT_ALLOWED`; both command-line host-file open functions return `FILE_ERROR_MOUNT_NOT_ALLOWED` | the loose source mounts |
| `Test_SecScript_BytecodeOnlyFromPackageAndPatch` | as `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Composed traversal tests" | the loose-mount module loads |
| `Test_SecShipping_UserNamespaceDevtoolsRefused` | `user:/devtools/x`, DEVT's `user:/devtools/assets/dump.json`, CINE's `user:/devtools/captures/x` and ADVGFX's `user:/devtools/vt/dump.json` each fail with `PATH_ERROR_USER_NAMESPACE` in `ShippingTest` (row SEC1, answered by PLAT's R10 patch) | the same paths parse |

## PowerShell tests

`Zenith2/Tools/Security/Tests/Test-Zenith2Security.ps1`, a plain assert script like `Tools/Test_T0Harness_*`, over hand-written fixture outputs under `Zenith2/Tools/Security/Tests/Fixtures/` (text only):

| Test | Asserts |
|---|---|
| `Pwsh_Sec_FuzzTriage_DedupAndSeverity` | a fixture `campaign.json` with six findings (two sharing a key, one unreproduced) yields four body pairs (six findings, two sharing a key, one unreproduced), the smaller input kept for the shared key, and the severities of "Severity mapping" |
| `Pwsh_Sec_FuzzTriage_HexRoundTrip` | a 130-byte input written as `.hex` has three lines (64, 64, 2 bytes), lower case, LF, and decodes to the same bytes and SHA-1 |
| `Pwsh_Sec_FuzzTriage_BodiesWellFormed` | each fix body has the ten sections in order, a `- [ ]` Definition of Done with a failure case, a `Decisions:` line citing Z2-D-045, and no `## Gates` heading; each recording body carries `needs-human` |
| `Pwsh_Sec_KeyCeremony_RoundTrip` | `New-Zenith2UpdateKey.ps1` with a test password in an environment variable writes an encrypted PEM into a temporary directory; SHIP's `Get-Zenith2UpdateSigningKey` decrypts it with a profile pointing at it and returns a 32-byte seed whose public key (checked with SHIP's `Ed25519PublicKeyCheck.java`) equals the tool's printed key; a second run into the same directory exits 5 and leaves the file unchanged; an unset variable exits 3 |
| `Pwsh_Sec_InstallLayout_WritePrincipals` | `Test-Zenith2InstallLayout.ps1` over recorded ACL objects: an install whose `.zenith2-install` grants `Users` write while the executable does not fails L1; equal ACLs pass; a FAT32 volume reports L2 as hash-only |
| `Pwsh_Sec_ParserInventory_CommittedFileValid` | the committed `parser_inventory.json`, `path_matrix.json` and `update_keys.json` parse and match their schemas through the readers of the protected `Zenith2SecurityGates.psm1`, which the script imports (the full conformance is the protected step's) |

## CMake script-mode tests

| Test | File | Asserts |
|---|---|---|
| `CMake_Sec_UpdateKeyResolution` | `Zenith2/CMake/Tests/Security/UpdateKeyResolution.cmake` | `zenith_security_resolve_update_key` over fixture key files written by the test: an active key's `publicKeyHex` is written to `Zenith_UpdateKey.Shipping.generated.inc` and TEST 1's to `Zenith_UpdateKey.Development.generated.inc`, both files written whatever the configuration; no active key yields 64 zeros and the warning; `ZENITH_UPDATE_KEY_IN_DEVELOPMENT=RELEASE` writes the active key to both; an unchanged key leaves both files' timestamps unchanged; two active keys, a published test key's hex, 64 zeros and a bad release line each fail with their messages from the CMake half; a record whose base64 disagrees with its hex fails with the PowerShell check's first line; after a call, the directory property `CMAKE_CONFIGURE_DEPENDS` holds the absolute paths of `update_keys.json`, `Test-Zenith2UpdateKeys.ps1` and `shipping_forbidden_strings.json`; a fixture pwsh path that does not exist fails with the `pwsh 7 is required` message |
| `CMake_Sec_PublishedKeysAgree` | `Zenith2/CMake/Tests/Security/PublishedKeysAgree.cmake` | the `hex` entries whose `id` ends `-public-key` in the committed `Zenith2/Tools/Gates/shipping_forbidden_strings.json` equal, in order, the initialisers of `aauZENITH_PUBLISHED_TEST_KEYS` in `Zenith2/Engine/ZenithPlatform/Zenith_UpdateKeyRules.h` (read as text), and their number equals `uZENITH_PUBLISHED_TEST_KEY_COUNT`; pure CMake, every test preset |
| `CMake_Sec_ToolFuzzTargetDeclarations` | `Zenith2/CMake/Tests/Security/ToolFuzzTargets.cmake` (written by SHIP's ticket that implements the helper, row SEC6 (g), from this specification) | `zenith_tool_fuzz_target` called in a non-sanitizer configure records `<Stem>\|<Tool>\|<Sources>` and creates no target; `zenith_cook_fuzz_target(MetaDocument SOURCES Internal/X.cpp DEPENDS ZenithCore)` records `MetaDocument\|ZenithCook\|Internal/X.cpp`; an unknown tool, a missing source, a main TU in `SOURCES` and a `DEPENDS` absent from the preset each fail with their messages (the last in a configure that sets `ZENITH_SANITIZER`); in that configure the smoke's `ENVIRONMENT` carries the step's suppression path |

## Gate-step fixture tests

`Test-SecurityConformance.ps1` asserts the fixtures of `Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Fixtures": `SecConformance_Pass`, `SecConformance_SC01` to `SecConformance_SC16` and `SecConformance_SC03b`, `SecConformance_SC03c` (each exactly one finding of its check), `SecConformance_KnownFindingPasses`, `SecConformance_ConfigureFailureExits2`, `SecConformance_CommittedRegistriesPass`. They are protected and land with SEC-P1.

`Zenith2/Tools/Gates/Tests/Security/Test-Security.ps1` (protected, SEC-P9) asserts the scan logic of `Zenith2/Tools/Gates/Security/`, over recorded tool outputs and text fixtures:

| Test | Asserts |
|---|---|
| `Pwsh_Sec_ShippingExposure_SymbolMatch` | recorded `llvm-pdbutil` and `llvm-nm` listings: a listing naming no entry passes; one naming each of an `exact` entry, a `prefix` entry with a longer name, and a C symbol fails naming it; a decorated name differing only in parameters, and one differing only in template arguments (the negative control), each match the normalised entry |
| `Pwsh_Sec_ShippingExposure_StringScan` | a fixture image with `ZENITH2-TOKEN ` placed across a 64 MiB chunk boundary (the test writes a sparse 64 MiB + 8 file in a temporary directory) is found; the UTF-16LE form is found; the UTF-16LE form of a 128-character `text` entry placed across the boundary is found (the negative control of the overlap); a `hex` entry is found; a clean file passes |
| `Pwsh_Sec_ShippingExposure_ImportScan` | recorded `llvm-readobj --coff-imports` output importing `bind` from `WS2_32.dll`, ordinal 13 from `ws2_32.dll`, ordinal 2 alone (`bind` by ordinal only, the negative control), and `AcceptEx` from `MSWSOCK.dll` each fail; one importing only `connect` passes; recorded `llvm-readelf --dyn-syms` with an undefined `accept4` fails |
| `Pwsh_Sec_ShippingExposure_PositiveControlMissing` | `-PositiveControl` against outputs lacking `Zenith_DevListenerImpl::Open` exits 1 with `positive control missing` |
| `Pwsh_Sec_ShippingExposure_ListSchemaRefused` | a symbol list with a duplicate `symbol`, a bad `match` and a string entry with both `text` and `hex` each exit 2 naming the field |
| `Pwsh_Sec_UpdateKeys_Rules` | `Test-Zenith2UpdateKeys.ps1` over fixture key files: a valid file exits 0; a record whose base64 and hex disagree, a digest mismatch, 64 zero digits, each published test key, a reused key, two active keys, a revocation without reason and records out of order each exit 1 with their messages; records of one game ordered `1.9` then `1.10` pass and the reverse order fails (the release line compared as integers); a key equal to a `-public-key` entry of a fixture strings list fails; the script imports no module (asserted on its text) |


## Acceptance items

| Item | Stage | Run by | Evidence |
|---|---|---|---|
| the fuzz campaign at the tier durations, every finding VERIFIED or ACCEPTED | 2, 3, 4, 5, 6 | a person, through DEVT's runner | `campaign.json`, the summary, the evidence record (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Campaign evidence") |
| the HWASan replay of the tier 1 and tier 2 regression tests on the S24 | 3, 4, 5, 6 | a person, through the test APK | the device run record |
| the Shipping exposure audit of every game's packaged images, with positive controls | 6 | the release verification | `zenith2.shipping-exposure` reports |
| the tamper suite and the path composition tests green in `Run-Committed.ps1 -Acceptance` | 6 | the acceptance run | the JUnit report |
| the distribution-layout audit of every supported layout | 6 | a person | `Test-Zenith2InstallLayout.ps1` reports in the qualification record |
| the key ceremony record of every shipped (game, release line) | 6 | a person | the committed `update_keys.json` record and SHIP's W10 |

## Review focus for SEC tickets

- A parser change: does every new count, length or offset reach a check before it sizes an allocation or indexes a buffer, and is the check tested at the limit and at limit + 1?
- A fuzz target: does the harness keep state across inputs, touch the file system, or assert anything but an invariant?
- The verifier: is any field beyond V2's read before V3 passes; is the signature range exactly `[0, total − 64)`?
- The audit lists: does every new development entry point have a symbol entry and a `ShippingTest` case?
- The key files: could a Shipping configuration ever obtain TEST 1's key?
- A committed Ed25519 seed or private key: did its public key join `aauZENITH_PUBLISHED_TEST_KEYS` and `shipping_forbidden_strings.json` in the same diff (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Published test keys")?
- The protected scan logic: does any SEC ticket's diff touch `Zenith2/Tools/Gates/**`? It must not; that is PROC's `needs-human` work.
