# NET requirement catalogue: Stage 7 networked multiplayer

## Purpose

The deferred Stage 7 requirements of the NET area (Z2-D-073), continuing the numbering of `Zenith2/Docs/Requirements/NET.md`, whose "Coverage of inventory items" maps them to Z2-I-NET-07 … Z2-I-NET-14. Each is classified deferred with stage S7 and names its open decision where one governs it; the design is `Zenith2/Docs/Design/NET/Stage7.md`.

## Requirements

### Z2-R-NET-108 Transport selection ruled

- **Statement:** D-SEL-NETTRANSPORT must be ruled from a harness under `Zenith2/Benchmarks/NetTransport/` measuring every candidate against the transport conformance suite and the S24 CPU per 1,000 messages, by the acceptance rule of "Transport selection".
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7), Z2-D-084, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the harness smoke in the `Test` gate step; the ruling's report under `Zenith2/Docs/Research/NetTransport/` and DecisionLog entry
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Transport selection"

### Z2-R-NET-109 Transport backend contract

- **Statement:** `Zenith_NetTransportBackend` must provide the three channels and the members of "Transport layer contract" with its thread affinity and errors, and the selected library must sit behind it inside `ZenithNet/Internal`.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetTransportConformance_ReliableOrderedUnderProfiles`, `Test_NetTransportConformance_UnreliableSequencedDropsOld` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Transport layer contract"

### Z2-R-NET-110 Stateless challenge handshake

- **Statement:** Wire format version 2 must connect through REQUEST, CHALLENGE (an HMAC cookie over address, nonce and epoch), RESPONSE and ACCEPT, allocating no slot before a valid cookie and never answering with more bytes than received.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SESSION-AUTH (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHandshakeV2_ChallengeStateless`, `Test_NetHandshakeV2_ResponseNeverLargerThanRequest` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Connection handshake version 2"

### Z2-R-NET-111 Handshake rate limits

- **Statement:** A server must drop requests above 10 per second per address and 1,000 per second in total, and ignore an address for 30 s after 8 failed cookies in 10 s.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SESSION-AUTH (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHandshakeV2_RateLimits`, `Test_NetHandshakeV2_TotalRateAndBanWindow` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Connection handshake version 2"

### Z2-R-NET-112 Channels and reliability

- **Statement:** The default reliability layer must implement sequence, ack and ack-bitfield acknowledgement, the RTT and retransmission rules, a 256-message reliable window and ordered delivery as "Channels and reliability" states.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetTransportConformance_ReliableOrderedUnderProfiles`, `Test_NetTransportConformance_RttEstimate` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Channels and reliability"

### Z2-R-NET-113 Fragmentation and reassembly

- **Statement:** Messages above one datagram must be fragmented into at most 60 fragments and reassembled with at most 4 groups per peer, unreliable groups dropped after 1 s.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetTransportConformance_FragmentReassembly`, `Test_NetTransportConformance_UnreliableGroupTimeout` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Fragmentation and reassembly"

### Z2-R-NET-114 Congestion and bandwidth control

- **Statement:** Each peer's sends must stay within its token-bucket budget, halved on measured loss or RTT growth and recovered as "Congestion and bandwidth control" states, without affecting simulation.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetTransportConformance_BandwidthBudget` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Congestion and bandwidth control"

### Z2-R-NET-115 Session lifecycle with reconnect

- **Statement:** Stage 7 sessions must follow the server, peer and client states of "Session lifecycle" and the tables of "Stage 7 state machines", including `Net.Connect` reconnection, the reconnection reaching the world as a recorded `RECONNECTING` session event.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7), D-NET-SESSION-AUTH (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSession_Stage7StatesFollowTables`, `Test_NetSession_ReconnectAfterDisconnect` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Session lifecycle"; `Zenith2/Docs/Design/NET/Stage7.md` → "Stage 7 state machines"

### Z2-R-NET-116 Peer slots and join in progress

- **Statement:** A server must hold up to 64 peer slots and give a joining peer the string table and every relevant entity's CREATE before its first snapshot.
- **Covers:** Z2-I-NET-07, Z2-I-NET-08
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_JoinInProgressFullState` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Peer slots and join in progress"

### Z2-R-NET-117 Clock synchronisation

- **Statement:** The client must estimate the server tick, keep its command lead at the target of "Clock synchronisation" by accumulator nudges of at most 2 ms per frame, never change tick duration, and publish each recomputed offset as a recorded `OFFSET_CHANGED` session event (value 6, the offset at byte 4 and the first client tick at byte 20) from which every simulation read of the tick mapping is taken.
- **Covers:** Z2-I-NET-07, Z2-I-NET-03
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetClock_LeadConvergesAndNudges`, `Test_NetPrediction_ReplayReproducesCorrections` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Clock synchronisation"

### Z2-R-NET-118 Session security ruled

- **Statement:** D-NET-SESSION-AUTH must be ruled by the criteria and acceptance rule of "Session security" (an analysis ruling, no per-packet cost criterion), and the chosen credential and AEAD protection must be implemented through a pinned, audited library; the challenge cookie uses HMAC-SHA-256 over CORE's SHA-256 whichever candidate wins.
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SESSION-AUTH (open, Stage 7), Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the ruling's DecisionLog entry; `Test_NetSecurity_UnauthenticatedRefusedInShipping` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Session security"

### Z2-R-NET-119 Shipping sessions authenticated

- **Statement:** A Stage 7 Shipping build must accept a non-loopback session only when authenticated and encrypted, lifting the Stage 3 Shipping guard in the same diff as the ruling's implementation.
- **Covers:** Z2-I-NET-07, Z2-I-NET-12
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SESSION-AUTH (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSecurity_UnauthenticatedRefusedInShipping` (`Test` gate step, `win-shipping-test`)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Session security"

### Z2-R-NET-120 Transport and session error codes

- **Statement:** Codes 32–42 of `Zenith_NetErrorCode` must carry the meanings of "Transport and session error codes".
- **Covers:** Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-SEL-NETTRANSPORT (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` gate step; `Test_NetSession_Stage7StatesFollowTables` (each failure cell asserts its error code) (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Transport and session error codes"

### Z2-R-NET-121 Per-peer replication model

- **Statement:** `Net.Replicate` must build, per peer and send tick, a delta snapshot of that peer's relevant entities ordered by priority within its budget, in phase 11 writing only the capture resource.
- **Covers:** Z2-I-NET-08
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_PriorityOrderDeterministic` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Replication model"

### Z2-R-NET-122 Create before state

- **Statement:** No snapshot may name an entity whose CREATE the peer has not acknowledged, and relevance changes must send CREATE and DESTROY with their reasons on the reliable channel.
- **Covers:** Z2-I-NET-08, Z2-I-NET-10
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_CreateBeforeState` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Entity scopes and relevance hand-off"; `Zenith2/Docs/Design/NET/Stage7.md` → "Entity create and destroy"

### Z2-R-NET-123 Full property replication

- **Statement:** Stage 7 replication must cover every replicated property of every component through the generic codecs, detecting changes from row change versions against the acknowledged baseline.
- **Covers:** Z2-I-NET-08, Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_DeltaRoundTripProperty` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Property replication"

### Z2-R-NET-124 Delta compression against baselines

- **Statement:** Each entity record must be encoded against the newest acknowledged snapshot containing it, from a 32-snapshot ring per peer within `net.server.baselineBytesPerPeer`.
- **Covers:** Z2-I-NET-08, Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_DeltaRoundTripProperty`, `Test_NetReplication_BaselineRingAndCap` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Delta compression against acknowledged baselines"

### Z2-R-NET-125 Priority and bandwidth allocation

- **Statement:** Per-peer priority accumulators must order entities deterministically for given state, budget and acknowledgements, as "Priority and bandwidth allocation" states.
- **Covers:** Z2-I-NET-08
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_PriorityOrderDeterministic` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Priority and bandwidth allocation"

### Z2-R-NET-126 Owner-client proposals

- **Statement:** OWNER_CLIENT components must be proposed by their owning client, validated on the server by the registered validator, applied by phase-1 commands, rejected values corrected, and recorded as server publications.
- **Covers:** Z2-I-NET-08, Z2-I-NET-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_OwnerProposalsValidated` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Owner-client authority"

### Z2-R-NET-127 Replicated events

- **Statement:** `Net.Broadcast` must send a registered event type's fields reliably to all, the owner or one peer, and the client must emit it in phase 1 from its recorded publication.
- **Covers:** Z2-I-NET-08
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_BroadcastTargets` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Replicated events"

### Z2-R-NET-128 Per-peer player slots

- **Statement:** Peer p's commands must drive player slot p on the server, with PLAT's per-slot action state and GAME's per-peer player entities (Z2-R-GAME-137).
- **Covers:** Z2-I-NET-08, Z2-I-NET-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_PerPeerPlayerSlots` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Player slots and per-peer input"

### Z2-R-NET-129 Replication across world transitions

- **Statement:** A server world transition must keep the session and peer slots, reload clients through TRANSITION and TRANSITION_READY, and resume replication with CREATEs.
- **Covers:** Z2-I-NET-08, Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReplication_WorldTransitionKeepsSession` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Replication with world transitions"

### Z2-R-NET-130 Prediction model ruled

- **Statement:** D-NET-PREDICTION-MODEL must be ruled from a harness measuring the three candidates on the `PredictionCorridor` scenario (a WorkloadCharacters scene authored by the Stage 7 prediction harness ticket, run by `Zenith2/Tests/NetPrediction/`) and the S24, by the acceptance rule of "Prediction model selection".
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-PREDICTION-MODEL (open, Stage 7), Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the harness smoke in the `Test` gate step; the ruling's report and DecisionLog entry
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Prediction model selection"

### Z2-R-NET-131 Predicted entities

- **Statement:** A client must predict exactly its locally owned entities carrying `Zenith.Net.Predicted` through the prediction sub-schedule, systems opting out through `NotPredicted`.
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-PREDICTION-MODEL (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPrediction_OnlyOwnedPredictedEntities` (owned entities without `Zenith.Net.Predicted` are interpolated; a `NotPredicted()` system is not re-run), `Test_NetPrediction_CorridorNoMispredictionAtFixedLatency` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Predicted entities"

### Z2-R-NET-132 Command acknowledgement

- **Statement:** Every Stage 7 snapshot must tell each peer its last applied command sequence and tick, and the client must discard acknowledged commands from its 64-tick ring.
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-PREDICTION-MODEL (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPrediction_CorrectionReplaysToServerState` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Command acknowledgement"

### Z2-R-NET-133 Reconciliation

- **Statement:** When the server state of a predicted entity differs from the prediction by more than the declared precision, compared in the decoded domain, the client must adopt the server state at T and re-run the prediction sub-schedule for the buffered ticks inside phase 1, counting the misprediction.
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-PREDICTION-MODEL (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPrediction_CorrectionReplaysToServerState` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Reconciliation algorithm"

### Z2-R-NET-134 Correction smoothing

- **Statement:** The visual error offset must decay as "Correction smoothing" states in presentation only, never entering simulation state.
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-PREDICTION-MODEL (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPrediction_SmoothingIsPresentationOnly` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Correction smoothing"

### Z2-R-NET-135 Remote entity interpolation

- **Statement:** Remote proxies must be presented interpolated between buffered snapshots at the interpolation delay, extrapolated at most 250 ms.
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInterpolation_RemoteProxyBetweenSnapshots` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Remote entity interpolation"

### Z2-R-NET-136 Prediction limits and rewind queries

- **Statement:** Prediction must stall beyond `MaxPredictedTicks` and fall back to interpolation beyond its history cap, and `Net.RewoundRaycast` must test against a 32-tick history copy at the view tick of the peer's recorded `PEER_VIEW` publication, never the live world.
- **Covers:** Z2-I-NET-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-PREDICTION-MODEL (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetPrediction_StallsBeyondLimit`, `Test_NetReplication_RewindQueryReplays` (replay equality and no read of the live world) (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Prediction limits"

### Z2-R-NET-137 Relevance grid

- **Statement:** The server must rebuild a uniform XZ grid of `net.interest.cellSizeM` cells each send tick in O(n).
- **Covers:** Z2-I-NET-10
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInterest_RadiusAndHysteresis` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Relevance grid"

### Z2-R-NET-138 Relevancy sets per peer

- **Statement:** An entity must be relevant to a peer under the four rules of "Relevancy sets per peer", at most `net.interest.maxRelevantPerPeer` keeping the nearest.
- **Covers:** Z2-I-NET-10
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInterest_AlwaysRelevantAndOwned`, `Test_NetInterest_CapacityKeepsNearest` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Relevancy sets per peer"

### Z2-R-NET-139 Relevance hysteresis

- **Statement:** Relevance must begin at R and end beyond 1.2 R after 2 s, with at most 64 CREATEs per send tick per peer.
- **Covers:** Z2-I-NET-10
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInterest_RadiusAndHysteresis` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Hysteresis and priority"

### Z2-R-NET-140 Streaming around every peer

- **Statement:** A server must keep streamed cells resident around every connected peer's controlled entity through STREAM's multiple viewers (row NT16).
- **Covers:** Z2-I-NET-10
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetInterest_StreamingViewersPerPeer` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Streaming integration"

### Z2-R-NET-141 Wire format version 2

- **Statement:** Stage 7 must use wire format version 2 (`ZNP2`) with reliability fields, message framing and the AEAD tag, a version 1 and a version 2 peer must reject each other, and a client must publish each received message in pieces of at most 1,020 bytes behind the publication prefix, reassembled in the phase-1 intake, with no change to the sink's limit.
- **Covers:** Z2-I-NET-11, Z2-I-NET-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SESSION-AUTH (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetWireV2_VersionsRejectEachOther`, `Test_NetHandshakeV2_ChallengeStateless`, `Test_NetPublication_Stage7PiecesReassemble` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Wire format version 2"; `Zenith2/Docs/Design/NET/Stage7.md` → "Protocol versioning"; `Zenith2/Docs/Design/NET/Stage7.md` → "Message publication in pieces"

### Z2-R-NET-142 Bit writer and reader

- **Statement:** The bit stream must pack least significant bit first into little-endian words and never read past its buffer.
- **Covers:** Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetBits_ReaderNeverOverreads` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Bit writer and reader"

### Z2-R-NET-143 Property codecs

- **Statement:** Every value type must have the full and delta codecs of "Property codecs", each with golden vectors and a property-based round trip; an undeclared position property uses the Stage 3 range of ±8,388.607 m.
- **Covers:** Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetCodecs_GoldenVectors`, `Test_NetCodecs_PropertyRoundTrip` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Property codecs"

### Z2-R-NET-144 Replication precision declarations

- **Statement:** A replicated property may declare precision, range and bits in its schema or C++ registration (row NT19), and a declaration change must change the table hash.
- **Covers:** Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_NetCodecs_PrecisionDeclarationChangesTableHash`, `Test_NetCodecs_PropertyRoundTrip` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Replication precision declarations"

### Z2-R-NET-145 Reference encoding

- **Statement:** String IDs and assets must be sent as session-table indices added reliably before first use, entities as network IDs, an unresolved reference resolving to null and counted.
- **Covers:** Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetReference_StringAndAssetTablesAddBeforeUse`, `Test_NetReference_UnresolvedEntityIsNullAndCounted` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Reference encoding"

### Z2-R-NET-146 Serialization limits and fuzzing

- **Statement:** Every limit of "Serialization limits" must be tested at the limit and limit + 1, and every Stage 7 parser must have a fuzz target.
- **Covers:** Z2-I-NET-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSerialization_LimitsAtAndBeyond` (`Test` gate step); `Zenith_Fuzz_NetBitStream`, `Zenith_Fuzz_NetMessageV2` (`Sanitizers -Preset win-asan`)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Serialization limits"

### Z2-R-NET-147 Dedicated server build target

- **Statement:** `zenith_add_game` must create `<Name>Server` for a game whose `net.dedicatedServer.enabled` is true, linking no RHI backend, forcing headless and defaulting `net.role` to server (row NT20).
- **Covers:** Z2-I-NET-12
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SERVER-PLATFORM (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServerBuild_LinksNoRhiBackend` (CMake fixtures, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Server build target"

### Z2-R-NET-148 Server platforms ruled

- **Statement:** D-NET-SERVER-PLATFORM must be ruled by the criteria of "Server platforms", a Linux server needing a decision amending Z2-D-010.
- **Covers:** Z2-I-NET-12
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SERVER-PLATFORM (open, Stage 7), Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** the ruling's DecisionLog entry
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Server platforms"

### Z2-R-NET-149 Server runtime differences

- **Statement:** A dedicated server must run with the differences of "Server runtime differences": no device, no local player, headless pacing, per-minute summary lines.
- **Covers:** Z2-I-NET-12, Z2-I-NET-05
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SERVER-PLATFORM (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServerBuild_BootsAndServes` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Server runtime differences"

### Z2-R-NET-150 Server operation

- **Statement:** A dedicated server must take its configuration from the command line and game layer, shut down on the platform termination signal with DISCONNECT to every peer and exit 0, and expose no remote administration in Shipping.
- **Covers:** Z2-I-NET-12
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, D-NET-SERVER-PLATFORM (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_NetServerBuild_BootsAndServes` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Server operation"

### Z2-R-NET-151 Simulated port

- **Statement:** `Zenith_NetSimulatedPort` must apply latency, jitter, loss, duplication, reordering, bandwidth and Gilbert–Elliott bursts per direction and refuse creation in Shipping.
- **Covers:** Z2-I-NET-13
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSimulation_ProfilesWithinTolerance`, `Test_NetSimulation_RefusedInShipping` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Simulated port"

### Z2-R-NET-152 Simulation profiles

- **Statement:** The five profiles of "Simulation profiles" must be selectable by `net.sim.profile` and by the `net.sim` console command, with per-field overrides.
- **Covers:** Z2-I-NET-13
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSimulation_ProfilesWithinTolerance` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Simulation profiles"

### Z2-R-NET-153 Deterministic network simulation

- **Statement:** The simulator must draw its decisions from a seeded `Zenith_RandomSource` in a fixed order per datagram, so a manual clock makes it a pure function of the test.
- **Covers:** Z2-I-NET-13
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetSimulation_DeterministicWithSeed` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Simulation determinism"

### Z2-R-NET-154 Per-peer statistics

- **Statement:** `Zenith_NetPeerStatistics` must report the fields of "Per-peer statistics" per peer, read only outside simulation.
- **Covers:** Z2-I-NET-14
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_PeerStatisticsPanel` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Per-peer statistics"

### Z2-R-NET-155 Network panel and console

- **Statement:** The `Net` panel and the Stage 7 console commands must exist in development builds as "Network panel and console" lists.
- **Covers:** Z2-I-NET-14
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_PeerStatisticsPanel` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Network panel and console"

### Z2-R-NET-156 Packet capture

- **Statement:** `net.capture` must write the `.znetcap` format through DEVT's development-file writer (row NT24), at most 64 MiB per file, decodable by `ZenithDevTool net-dump`.
- **Covers:** Z2-I-NET-14
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_PacketCaptureFormat` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Packet capture"

### Z2-R-NET-157 Replication debug views and dump

- **Statement:** Development builds must draw the relevance, priority and correction views and add the per-peer `net` frame-dump content of "Network frame dump".
- **Covers:** Z2-I-NET-14
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetDiagnostics_DebugViewsAndDump` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/NET/Stage7.md` → "Replication debug views"; `Zenith2/Docs/Design/NET/Stage7.md` → "Network frame dump"

### Z2-R-NET-158 Stage 7 NET budget rows allocated

- **Statement:** The `needs-human` Stage 7 per-system budget ticket for NET must allocate the pending rows N6–N9 from measured runs.
- **Covers:** Z2-I-NET-07, Z2-I-NET-08, Z2-I-NET-09, Z2-I-NET-10
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-073, Z2-D-012, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the ruling's Budgets.md rows and DecisionLog entry, checked by the `Coverage` gate step in acceptance mode `-Stage 7`
- **Design:** `Zenith2/Docs/Design/NET.md` → "Pending row needs"
