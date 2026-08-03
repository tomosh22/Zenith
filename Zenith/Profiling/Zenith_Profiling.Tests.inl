#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// Boot-capture tests.
//
// Included at the bottom of Zenith_Profiling.cpp so the file-static routing
// helpers (RouteFullRingEvent / DrainRingInto / FlushExitingRingLocked /
// SignedTickDeltaToMs) and the BootCapture internals are directly reachable —
// that is the whole reason those helpers are static + global-free.
//
// EVERY test that mutates state does so on a LOCAL instance. The one test that
// touches the live engine profiler (BootCaptureLiveEngineState) is strictly
// read-only: the batch these tests run inside IS the suspended window, and
// perturbing the live capture would corrupt the run's own boot report.
//
// Both BootCapture (~130 KB of zone aggregates) and ThreadBuffer (an 8K ring of
// 40-byte events, ~330 KB) are far too large for the stack, so the rig below
// heap-allocates them.
// ============================================================================

#if ZENITH_PROFILING_ENABLED

namespace
{
	// A profiler + one registered ring + a configured capture, wired exactly the way
	// RegisterThread wires the real thing. Owns everything it allocates.
	struct BootTestRig
	{
		Zenith_Profiling* m_pxProfiling = nullptr;
		Zenith_Profiling::ThreadBuffer* m_pxBuffer = nullptr;

		explicit BootTestRig(const u_int uMaxRawEvents = 64, const bool bRetainRaw = true)
		{
			m_pxProfiling = new Zenith_Profiling();          // ctor allocates the control block
			m_pxProfiling->m_pxBootCapture = new Zenith_Profiling::BootCapture();
			m_pxProfiling->m_pxBootCapture->Configure(uMaxRawEvents, bRetainRaw, 1000);

			m_pxBuffer = new Zenith_Profiling::ThreadBuffer();
			m_pxBuffer->m_uThreadID = 0;
			m_pxBuffer->m_pxControl = Control();
			Control()->m_apxThreadBuffers[0].store(m_pxBuffer, std::memory_order_release);

			SetState(Zenith_Profiling::BOOT_CAPTURE_ACTIVE);
		}

		~BootTestRig()
		{
			if (Control() != nullptr) Control()->m_apxThreadBuffers[0].store(nullptr, std::memory_order_release);
			delete m_pxBuffer;
			delete m_pxProfiling;   // frees its control block AND its capture
		}

		BootTestRig(const BootTestRig&) = delete;
		BootTestRig& operator=(const BootTestRig&) = delete;

		Zenith_Profiling::BootControlBlock* Control() const { return m_pxProfiling->GetControlBlock(); }
		Zenith_Profiling::BootCapture& Capture() const { return *m_pxProfiling->GetBootCapture(); }
		void SetState(const u_int uState) const { Control()->m_uState.store(uState, std::memory_order_release); }

		// Fills the ring to EXACTLY capacity, so the very next completed scope takes
		// the ring-full routing branch.
		void FillRing(const Zenith_ProfileZoneID uZoneID, const u_int64 uBaseTicks) const
		{
			for (u_int u = 0; u < Zenith_Profiling::uRING_CAPACITY; ++u)
			{
				m_pxBuffer->m_axEvents[u] = Zenith_Profiling::Event(uBaseTicks + u, uBaseTicks + u + 1, uZoneID, 0);
			}
			m_pxBuffer->m_uWriteLocal = Zenith_Profiling::uRING_CAPACITY;
			m_pxBuffer->m_uWriteCursor.store(Zenith_Profiling::uRING_CAPACITY, std::memory_order_release);
			m_pxBuffer->m_uReadCursor.store(0, std::memory_order_release);
		}

		u_int64 Drops() const { return m_pxBuffer->m_uDroppedEvents.load(std::memory_order_relaxed); }
	};

	// Reads a whole tmpfile back so a report can be asserted on by substring.
	void ReadWholeFile(FILE* pxFile, Zenith_Vector<char>& xOut)
	{
		fflush(pxFile);
		fseek(pxFile, 0, SEEK_END);
		const long lSize = ftell(pxFile);
		fseek(pxFile, 0, SEEK_SET);
		for (long l = 0; l < lSize; ++l) xOut.PushBack('\0');
		xOut.PushBack('\0');
		if (lSize > 0) { const size_t uRead = fread(xOut.GetDataPointer(), 1, static_cast<size_t>(lSize), pxFile); (void)uRead; }
	}
}

// Aggregates roll up per zone: count, total, min and max, across any number of
// events, independent of the raw list.
ZENITH_TEST(Profiling, BootCaptureAggregate)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(1024, true, 0);

	pxCapture->Ingest(Zenith_Profiling::Event(10, 30, 1, 0), 0);   // 20 ticks
	pxCapture->Ingest(Zenith_Profiling::Event(40, 45, 1, 0), 0);   //  5 ticks
	pxCapture->Ingest(Zenith_Profiling::Event(50, 60, 2, 0), 0);   // 10 ticks

	ZENITH_ASSERT_EQ(pxCapture->m_uTotalEvents, static_cast<u_int64>(3), "every ingested event must be counted");
	ZENITH_ASSERT_EQ(pxCapture->m_axZoneAggs[1].m_uCount, static_cast<u_int64>(2), "zone 1 fired twice");
	ZENITH_ASSERT_EQ(pxCapture->m_axZoneAggs[1].m_uTotalTicks, static_cast<u_int64>(25), "zone 1 total must be 20+5");
	ZENITH_ASSERT_EQ(pxCapture->m_axZoneAggs[1].m_uMinTicks, static_cast<u_int64>(5), "zone 1 min must be the shorter event");
	ZENITH_ASSERT_EQ(pxCapture->m_axZoneAggs[1].m_uMaxTicks, static_cast<u_int64>(20), "zone 1 max must be the longer event");
	ZENITH_ASSERT_EQ(pxCapture->m_axZoneAggs[2].m_uCount, static_cast<u_int64>(1), "zone 2 fired once");

	delete pxCapture;
}

// The raw cap is injectable and truncation is counted — but aggregates keep
// counting past it, so the per-zone table stays complete when the timeline is not.
ZENITH_TEST(Profiling, BootCaptureRawCap)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(4, true, 0);

	for (u_int u = 0; u < 10; ++u)
	{
		pxCapture->Ingest(Zenith_Profiling::Event(u, u + 1, 3, 0), 0);
	}

	ZENITH_ASSERT_EQ(pxCapture->m_xRawEvents.GetSize(), 4u, "raw retention must stop at the injected cap");
	ZENITH_ASSERT_EQ(pxCapture->m_uTruncatedRawEvents, static_cast<u_int64>(6), "everything past the cap must be counted as truncated");
	ZENITH_ASSERT_EQ(pxCapture->m_axZoneAggs[3].m_uCount, static_cast<u_int64>(10), "aggregates must keep counting past the raw cap");

	// Aggregates-only mode (the Android / non-tools default) keeps nothing raw.
	Zenith_Profiling::BootCapture* pxAggOnly = new Zenith_Profiling::BootCapture();
	pxAggOnly->Configure(1024, false, 0);
	pxAggOnly->Ingest(Zenith_Profiling::Event(0, 5, 3, 0), 0);
	ZENITH_ASSERT_EQ(pxAggOnly->m_xRawEvents.GetSize(), 0u, "aggregates-only mode must retain no raw events");
	ZENITH_ASSERT_EQ(pxAggOnly->m_uTruncatedRawEvents, static_cast<u_int64>(0), "aggregates-only mode is not truncation");
	ZENITH_ASSERT_EQ(pxAggOnly->m_axZoneAggs[3].m_uCount, static_cast<u_int64>(1), "aggregates-only mode still aggregates");

	delete pxAggOnly;
	delete pxCapture;
}

// Markers append in order; milestones are first-wins by name; Seal stamps the
// cutoff and leaves the raw list sorted by begin tick.
ZENITH_TEST(Profiling, BootCaptureMarkersAndFinalize)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(1024, true, 100);

	pxCapture->AddMarker("PhaseBegin", 110);
	pxCapture->AddMarker("PhaseEnd", 150);
	pxCapture->SetMilestone("FirstFrameCompleted", 900);
	pxCapture->SetMilestone("FirstFrameCompleted", 999);   // must NOT overwrite

	ZENITH_ASSERT_EQ(pxCapture->m_uMarkerCount, 2u, "both markers must be recorded");
	ZENITH_ASSERT_EQ(pxCapture->m_uMilestoneCount, 1u, "a repeated milestone must not add a second entry");
	ZENITH_ASSERT_NOT_NULL(pxCapture->FindMilestone("FirstFrameCompleted"), "milestone must be findable by name");
	ZENITH_ASSERT_EQ(pxCapture->FindMilestone("FirstFrameCompleted")->m_uTicks, static_cast<u_int64>(900), "milestones are first-wins");
	ZENITH_ASSERT_NULL(pxCapture->FindMilestone("NeverRecorded"), "an unrecorded milestone must not resolve");

	// Deliberately out of order — Seal sorts.
	pxCapture->Ingest(Zenith_Profiling::Event(300, 310, 1, 0), 0);
	pxCapture->Ingest(Zenith_Profiling::Event(200, 210, 1, 0), 0);
	pxCapture->Seal(1000);

	ZENITH_ASSERT_TRUE(pxCapture->m_bSealed, "Seal must mark the capture sealed");
	ZENITH_ASSERT_EQ(pxCapture->m_uCutoffTicks, static_cast<u_int64>(1000), "Seal must stamp the cutoff it was given");
	ZENITH_ASSERT_EQ(pxCapture->m_xRawEvents.Get(0).m_xEvent.m_uBeginTicks, static_cast<u_int64>(200), "Seal must sort the raw list by begin tick");
	ZENITH_ASSERT_EQ(pxCapture->m_xRawEvents.Get(1).m_xEvent.m_uBeginTicks, static_cast<u_int64>(300), "Seal must sort the raw list by begin tick");

	delete pxCapture;
}

// Post-seal, a boot-era straggler still reaches the capture — through the LATE
// list, because the raw list is frozen and sorted at Seal — and a frame drain
// diverts pre-boundary events the same way.
ZENITH_TEST(Profiling, BootCaptureLateRouting)
{
	BootTestRig xRig;
	xRig.Control()->m_uCutoffTicks.store(5000, std::memory_order_release);
	xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_SEALED);
	xRig.Capture().Seal(5000);

	// (a) ring-full routing of a scope that BEGAN inside boot and only just closed.
	xRig.FillRing(1, 100);
	RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(4000, 6000, 1, 0));
	ZENITH_ASSERT_EQ(xRig.Capture().m_uLateEvents, static_cast<u_int64>(1), "a pre-cutoff straggler must be captured late, not dropped");
	ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(0), "a pre-cutoff straggler must not count as a drop");

	// (b) a post-seal FRAME drain diverting pre-boundary events into the same list.
	xRig.m_pxBuffer->m_axEvents[0] = Zenith_Profiling::Event(4500, 4600, 1, 0);
	xRig.m_pxBuffer->m_uWriteCursor.store(1, std::memory_order_release);
	xRig.m_pxBuffer->m_uReadCursor.store(0, std::memory_order_release);

	Zenith_Profiling::Snapshot xSnapshot;
	DrainTarget xTarget;
	xTarget.m_pxSnapshot = &xSnapshot;
	xTarget.m_pxBoot = &xRig.Capture();
	xTarget.m_bBootIsLate = true;
	xTarget.m_uBootBoundaryTicks = 5000;
	DrainRingInto(*xRig.m_pxBuffer, 0, xTarget);

	ZENITH_ASSERT_EQ(xRig.Capture().m_uLateEvents, static_cast<u_int64>(2), "a pre-boundary frame-drain event must divert into the capture");
	ZENITH_ASSERT_NOT_NULL(xSnapshot.m_xThreadEvents.TryGet(0u), "diverting must not remove the event from the frame snapshot");

	// Publishing swaps pending into the immutable published list, sorted by begin.
	xRig.Capture().PublishLate();
	ZENITH_ASSERT_EQ(xRig.Capture().m_xLatePending.GetSize(), 0u, "publish must drain the pending list");
	ZENITH_ASSERT_EQ(xRig.Capture().m_xLatePublished.GetSize(), 2u, "publish must move both late events across");
	ZENITH_ASSERT_EQ(xRig.Capture().m_xLatePublished.Get(0).m_xEvent.m_uBeginTicks, static_cast<u_int64>(4000), "publish must re-sort by begin tick");
}

// The immutable first-frame boundary is what stops boot attribution running
// forever: an ordinary cross-frame scope at frame N begins AFTER it, so it is
// never boot-routed — it only lands in the frame snapshot.
ZENITH_TEST(Profiling, BootCrossFrameScopeNotLate)
{
	BootTestRig xRig;
	xRig.Control()->m_uCutoffTicks.store(5000, std::memory_order_release);
	xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_SEALED);
	xRig.Capture().Seal(5000);

	// Begins at 9000: long after the boundary, but spans the frame edge.
	xRig.m_pxBuffer->m_axEvents[0] = Zenith_Profiling::Event(9000, 9500, 1, 0);
	xRig.m_pxBuffer->m_uWriteCursor.store(1, std::memory_order_release);
	xRig.m_pxBuffer->m_uReadCursor.store(0, std::memory_order_release);

	Zenith_Profiling::Snapshot xSnapshot;
	DrainTarget xTarget;
	xTarget.m_pxSnapshot = &xSnapshot;
	xTarget.m_pxBoot = &xRig.Capture();
	xTarget.m_bBootIsLate = true;
	xTarget.m_uBootBoundaryTicks = 6000;   // first production frame began here
	DrainRingInto(*xRig.m_pxBuffer, 0, xTarget);

	ZENITH_ASSERT_EQ(xRig.Capture().m_uLateEvents, static_cast<u_int64>(0), "a post-boundary cross-frame scope must NEVER be boot-attributed");
	ZENITH_ASSERT_EQ(xRig.Capture().m_uTotalEvents, static_cast<u_int64>(0), "a post-boundary scope must not reach the boot aggregates either");
	const Zenith_Vector<Zenith_Profiling::Event>* pxEvents = xSnapshot.m_xThreadEvents.TryGet(0u);
	ZENITH_ASSERT_NOT_NULL(pxEvents, "the event still belongs to the frame snapshot");
	ZENITH_ASSERT_EQ(pxEvents->GetSize(), 1u, "the event still belongs to the frame snapshot");
}

// Late storage is bounded; the overflow is counted, never grown into.
ZENITH_TEST(Profiling, BootLateStorageCap)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(8, true, 0);   // tiny RAW cap: the LATE cap is independent

	const u_int uOverflow = 5;
	for (u_int u = 0; u < Zenith_Profiling::BootCapture::uMAX_LATE_EVENTS + uOverflow; ++u)
	{
		pxCapture->IngestLate(Zenith_Profiling::Event(u, u + 1, 1, 0), 0);
	}

	ZENITH_ASSERT_EQ(pxCapture->m_xLatePending.GetSize(), Zenith_Profiling::BootCapture::uMAX_LATE_EVENTS, "late storage must stop at its cap");
	ZENITH_ASSERT_EQ(pxCapture->m_uLateDropped, static_cast<u_int64>(uOverflow), "late arrivals past the cap must be counted, not stored");
	ZENITH_ASSERT_EQ(pxCapture->m_uLateEvents, static_cast<u_int64>(Zenith_Profiling::BootCapture::uMAX_LATE_EVENTS + uOverflow),
		"the late COUNT must include the ones storage could not keep");

	delete pxCapture;
}

// Drop deltas accumulate over non-suspended intervals only: whatever a suspended
// window dropped is re-baselined away rather than folded in.
ZENITH_TEST(Profiling, BootCaptureDropAccounting)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(64, true, 0);

	pxCapture->FoldDropDelta(0, 5);                          // 5 dropped while ACTIVE
	ZENITH_ASSERT_EQ(pxCapture->GetTotalDrops(), static_cast<u_int64>(5), "an active-interval delta must fold in");

	pxCapture->RebaselineDrops(0, 12);                       // Resume after a suspended window
	pxCapture->FoldDropDelta(0, 20);                         // 8 more while ACTIVE
	ZENITH_ASSERT_EQ(pxCapture->GetTotalDrops(), static_cast<u_int64>(13), "the suspended window's 7 drops must be excluded");

	pxCapture->FoldDropDelta(0, 20);                         // folding is idempotent
	ZENITH_ASSERT_EQ(pxCapture->GetTotalDrops(), static_cast<u_int64>(13), "re-folding the same counter must not double-count");

	// Out-of-range thread ids are ignored rather than indexing off the end.
	pxCapture->FoldDropDelta(Zenith_Profiling::uMAX_PROFILE_THREADS, 1000);
	ZENITH_ASSERT_EQ(pxCapture->GetTotalDrops(), static_cast<u_int64>(13), "an out-of-range thread id must be ignored");

	delete pxCapture;
}

// The whole point of the state machine: an exactly-full ring behaves differently
// in each state, and every case is decided under the lock without dereferencing
// anything that could have been freed.
ZENITH_TEST(Profiling, BootRouteStateMachine)
{
	// ACTIVE: drain every ring into the capture, then publish the pending event.
	{
		BootTestRig xRig(Zenith_Profiling::uRING_CAPACITY * 2, true);
		xRig.FillRing(1, 100);
		RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(9000, 9100, 1, 0));

		ZENITH_ASSERT_EQ(xRig.Capture().m_uTotalEvents, static_cast<u_int64>(Zenith_Profiling::uRING_CAPACITY),
			"ACTIVE routing must drain the whole ring into the capture");
		ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(0), "ACTIVE routing must not drop the pending event");
		ZENITH_ASSERT_EQ(xRig.m_pxBuffer->m_uWriteCursor.load(std::memory_order_acquire),
			static_cast<u_int64>(Zenith_Profiling::uRING_CAPACITY) + 1, "the pending event must be published after the drain");
	}

	// SUSPENDED: ProfilingOverflow's contract — no consumer, so the excess drops.
	{
		BootTestRig xRig;
		xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_SUSPENDED);
		xRig.FillRing(1, 100);
		RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(9000, 9100, 1, 0));

		ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(1), "SUSPENDED must drop and count");
		ZENITH_ASSERT_EQ(xRig.Capture().m_uTotalEvents, static_cast<u_int64>(0), "SUSPENDED must not drain into the capture");
	}

	// SEALED, boot-era event: captured late.
	{
		BootTestRig xRig;
		xRig.Control()->m_uCutoffTicks.store(5000, std::memory_order_release);
		xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_SEALED);
		xRig.FillRing(1, 100);
		RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(4999, 5200, 1, 0));

		ZENITH_ASSERT_EQ(xRig.Capture().m_uLateEvents, static_cast<u_int64>(1), "SEALED + begin<cutoff must capture");
		ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(0), "SEALED + begin<cutoff must not drop");
	}

	// SEALED, frame-era event: ordinary back-pressure, exactly as before boot capture.
	{
		BootTestRig xRig;
		xRig.Control()->m_uCutoffTicks.store(5000, std::memory_order_release);
		xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_SEALED);
		xRig.FillRing(1, 100);
		RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(5000, 5200, 1, 0));

		ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(1), "SEALED + begin>=cutoff must drop and count");
		ZENITH_ASSERT_EQ(xRig.Capture().m_uLateEvents, static_cast<u_int64>(0), "SEALED + begin>=cutoff must not capture");
	}

	// DISABLED (--skip-boot-capture): plain drop from process start.
	{
		BootTestRig xRig;
		xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_DISABLED);
		xRig.FillRing(1, 100);
		RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(9000, 9100, 1, 0));

		ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(1), "DISABLED must drop and count");
		ZENITH_ASSERT_EQ(xRig.Capture().m_uTotalEvents, static_cast<u_int64>(0), "DISABLED must never route");
	}

	// DEAD: the profiler back-pointer is already null, so nothing is dereferenced.
	{
		BootTestRig xRig;
		xRig.Control()->m_pxProfiling = nullptr;
		xRig.SetState(Zenith_Profiling::BOOT_CAPTURE_DEAD);
		xRig.FillRing(1, 100);
		RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(9000, 9100, 1, 0));

		ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(1), "DEAD must drop and count without touching the profiler");
	}
}

// A ring that is STILL full after the drain (its slot was claimed by someone else
// in the meantime) must drop rather than write past the cursors. This is the
// branch that depends on RELOADING the cursors after the drain instead of
// trusting the pre-drain values.
ZENITH_TEST(Profiling, BootRouteCompetingDrain)
{
	BootTestRig xRig;
	xRig.FillRing(1, 100);

	// Unpublish the ring so the routing drain walks the table and frees nothing.
	xRig.Control()->m_apxThreadBuffers[0].store(nullptr, std::memory_order_release);
	RouteFullRingEvent(*xRig.m_pxBuffer, Zenith_Profiling::Event(9000, 9100, 1, 0));

	ZENITH_ASSERT_EQ(xRig.Drops(), static_cast<u_int64>(1), "a still-full ring after the drain must drop, not overwrite");
	ZENITH_ASSERT_EQ(xRig.m_pxBuffer->m_uWriteCursor.load(std::memory_order_acquire),
		static_cast<u_int64>(Zenith_Profiling::uRING_CAPACITY), "the write cursor must not advance when the event was dropped");
	ZENITH_ASSERT_EQ(xRig.Capture().m_uTotalEvents, static_cast<u_int64>(0), "nothing was drainable, so nothing was captured");
}

// An exiting thread's ring is flushed before it is freed — the pre-boot-capture
// code discarded it, losing the only record of that thread's boot work.
ZENITH_TEST(Profiling, BootUnregisterFlush)
{
	BootTestRig xRig;
	xRig.m_pxBuffer->m_axEvents[0] = Zenith_Profiling::Event(100, 200, 1, 0);
	xRig.m_pxBuffer->m_axEvents[1] = Zenith_Profiling::Event(200, 250, 1, 0);
	xRig.m_pxBuffer->m_uWriteCursor.store(2, std::memory_order_release);
	xRig.m_pxBuffer->m_uReadCursor.store(0, std::memory_order_release);
	xRig.m_pxBuffer->m_uDroppedEvents.store(7, std::memory_order_relaxed);

	FlushExitingRingLocked(*xRig.Control(), &xRig.Capture(), *xRig.m_pxBuffer, 0);

	ZENITH_ASSERT_EQ(xRig.Capture().m_uTotalEvents, static_cast<u_int64>(2), "an exiting ring's events must be flushed into the capture");
	ZENITH_ASSERT_EQ(xRig.Capture().GetTotalDrops(), static_cast<u_int64>(7), "an exiting ring's drop delta must be folded into the accounting");

	// SUSPENDED discards instead, matching the no-consumer contract.
	BootTestRig xSuspended;
	xSuspended.SetState(Zenith_Profiling::BOOT_CAPTURE_SUSPENDED);
	xSuspended.m_pxBuffer->m_axEvents[0] = Zenith_Profiling::Event(100, 200, 1, 0);
	xSuspended.m_pxBuffer->m_uWriteCursor.store(1, std::memory_order_release);
	xSuspended.m_pxBuffer->m_uReadCursor.store(0, std::memory_order_release);
	FlushExitingRingLocked(*xSuspended.Control(), &xSuspended.Capture(), *xSuspended.m_pxBuffer, 0);
	ZENITH_ASSERT_EQ(xSuspended.Capture().m_uTotalEvents, static_cast<u_int64>(0), "SUSPENDED must discard an exiting ring, not capture it");
	ZENITH_ASSERT_EQ(xSuspended.m_pxBuffer->m_uReadCursor.load(std::memory_order_acquire), static_cast<u_int64>(1),
		"discarding must still advance the read cursor");
}

// Shutdown with a producer still registered: the control block is deliberately
// LEAKED alongside the surviving ring, the state goes DEAD, and that ring's
// producer can still route safely (it drops).
ZENITH_TEST(Profiling, BootShutdownLiveProducer)
{
	Zenith_Profiling* pxProfiling = new Zenith_Profiling();
	Zenith_Profiling::BootControlBlock* pxControl = pxProfiling->GetControlBlock();

	// Slot 1, never the main-thread slot — m_pxThreading is null here, so the
	// main-thread id resolves to ~0u and NOTHING matches it. This is exactly the
	// FileWatcher case: a producer that never unregistered.
	Zenith_Profiling::ThreadBuffer* pxBuffer = new Zenith_Profiling::ThreadBuffer();
	pxBuffer->m_uThreadID = 1;
	pxBuffer->m_pxControl = pxControl;
	pxControl->m_apxThreadBuffers[1].store(pxBuffer, std::memory_order_release);
	pxControl->m_uState.store(Zenith_Profiling::BOOT_CAPTURE_ACTIVE, std::memory_order_release);

	// Shutdown clears the CALLING thread's tl_pxBuffer, which here is the live
	// engine's main-thread ring. Save and restore it, or the rest of this batch
	// silently stops recording on the main thread.
	Zenith_Profiling::ThreadBuffer* pxSavedTLS = tl_pxBuffer;
	pxProfiling->Shutdown();
	tl_pxBuffer = pxSavedTLS;

	ZENITH_ASSERT_NULL(pxProfiling->GetControlBlock(), "Shutdown must release the profiler's handle on the leaked block");
	ZENITH_ASSERT_EQ(pxControl->m_uState.load(std::memory_order_acquire), static_cast<u_int>(Zenith_Profiling::BOOT_CAPTURE_DEAD),
		"Shutdown must publish DEAD before anything is freed");
	ZENITH_ASSERT_NULL(pxControl->m_pxProfiling, "Shutdown must null the profiler back-pointer under the mutex");
	ZENITH_ASSERT_NOT_NULL(pxControl->m_apxThreadBuffers[1].load(std::memory_order_acquire), "a live producer's ring must survive Shutdown");

	// The surviving producer routes safely against the leaked block.
	for (u_int u = 0; u < Zenith_Profiling::uRING_CAPACITY; ++u)
	{
		pxBuffer->m_axEvents[u] = Zenith_Profiling::Event(u, u + 1, 1, 0);
	}
	pxBuffer->m_uWriteLocal = Zenith_Profiling::uRING_CAPACITY;
	pxBuffer->m_uWriteCursor.store(Zenith_Profiling::uRING_CAPACITY, std::memory_order_release);
	pxBuffer->m_uReadCursor.store(0, std::memory_order_release);
	RouteFullRingEvent(*pxBuffer, Zenith_Profiling::Event(9000, 9100, 1, 0));
	ZENITH_ASSERT_EQ(pxBuffer->m_uDroppedEvents.load(std::memory_order_relaxed), static_cast<u_int64>(1),
		"a producer racing Shutdown must see DEAD and drop, never dereference a freed profiler");

	delete pxProfiling;   // its handle is already null: frees nothing twice
	delete pxBuffer;      // the test owns the deliberate leak, so it cleans it up
	delete pxControl;
}

// Sentinel zone ids (NULL / OVERFLOW) sit far past the descriptor table. They are
// counted as unattributed, never used to index.
ZENITH_TEST(Profiling, BootZoneAggSentinelGuard)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(64, true, 0);

	pxCapture->Ingest(Zenith_Profiling::Event(10, 20, ZENITH_PROFILE_ZONE_NULL, 0), 0);
	pxCapture->Ingest(Zenith_Profiling::Event(10, 20, ZENITH_PROFILE_ZONE_OVERFLOW, 0), 0);
	pxCapture->Ingest(Zenith_Profiling::Event(10, 20, Zenith_Profiling::uMAX_ZONES, 0), 0);

	ZENITH_ASSERT_EQ(pxCapture->m_uUnattributedEvents, static_cast<u_int64>(3), "sentinel and out-of-range zone ids must be counted as unattributed");
	ZENITH_ASSERT_EQ(pxCapture->m_uTotalEvents, static_cast<u_int64>(3), "unattributed events still count toward the total");

	delete pxCapture;
}

// A permanent snapshot cannot hold arbitrary transient label pointers, so labels
// are dropped at ingest and only the stable interned zone identity survives.
ZENITH_TEST(Profiling, BootCaptureLabelPolicy)
{
	Zenith_Profiling::BootCapture* pxCapture = new Zenith_Profiling::BootCapture();
	pxCapture->Configure(64, true, 0);

	char acTransient[32];
	snprintf(acTransient, sizeof(acTransient), "Transient Label");
	pxCapture->Ingest(Zenith_Profiling::Event(10, 20, 1, 0, acTransient), 0);
	pxCapture->IngestLate(Zenith_Profiling::Event(30, 40, 1, 0, acTransient), 0);

	ZENITH_ASSERT_NULL(pxCapture->m_xRawEvents.Get(0).m_xEvent.m_szLabel, "raw ingest must drop the label pointer");
	ZENITH_ASSERT_NULL(pxCapture->m_xLatePending.Get(0).m_xEvent.m_szLabel, "late ingest must drop the label pointer too");
	ZENITH_ASSERT_EQ(pxCapture->m_xRawEvents.Get(0).m_xEvent.m_uZoneID, static_cast<Zenith_ProfileZoneID>(1), "the interned zone identity is what survives");

	delete pxCapture;
}

// The report renders its headline, the milestone table (with N/A for milestones
// this run never reached), the phase table and the per-zone aggregates.
ZENITH_TEST(Profiling, BootCaptureReport)
{
	Zenith_Profiling* pxProfiling = new Zenith_Profiling();
	const Zenith_ProfileZoneID uZone = pxProfiling->RegisterZone("BootReportTestZone");
	pxProfiling->m_uMainThreadID = 0;
	pxProfiling->m_pxBootCapture = new Zenith_Profiling::BootCapture();
	pxProfiling->m_pxBootCapture->Configure(64, true, 1000);
	pxProfiling->m_pxBootCapture->Ingest(Zenith_Profiling::Event(1100, 1300, uZone, 0), 0);
	pxProfiling->m_pxBootCapture->AddMarker("BootReportPhaseBegin", 1050);
	pxProfiling->m_pxBootCapture->AddMarker("BootReportPhaseEnd", 1090);
	pxProfiling->m_pxBootCapture->AddMarker("BootReportLonelyMarker", 1095);
	pxProfiling->m_pxBootCapture->SetMilestone("FirstFrameCompleted", 1400);
	pxProfiling->m_pxBootCapture->Seal(1500);

	FILE* pxFile = nullptr;
	tmpfile_s(&pxFile);
	ZENITH_ASSERT_NOT_NULL(pxFile, "tmpfile() must open for the report round-trip");
	if (pxFile != nullptr)
	{
		pxProfiling->WriteBootReport(pxFile);

		Zenith_Vector<char> xText;
		ReadWholeFile(pxFile, xText);
		const char* szText = xText.GetDataPointer();

		ZENITH_ASSERT_NOT_NULL(strstr(szText, "=== Boot Profile"), "the report must carry its section header");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "BootReportTestZone"), "the per-zone aggregate table must list zones that fired");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "BootReportPhase"), "the phase table must list a matched marker pair");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "BootReportLonelyMarker"), "an unpaired marker must still list, as a point");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "(point)"), "an unpaired marker must be rendered as a point");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "FirstPresentSubmitted"), "canonical milestones must always be listed");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "N/A"), "a milestone this run never reached must print as N/A");
		ZENITH_ASSERT_NOT_NULL(strstr(szText, "Seal overhead"), "seal overhead must be reported");

		fclose(pxFile);
	}

	delete pxProfiling;
}

// The report degrades cleanly when the capture was never allocated.
ZENITH_TEST(Profiling, BootCaptureReportDisabled)
{
	Zenith_Profiling* pxProfiling = new Zenith_Profiling();

	FILE* pxFile = nullptr;
	tmpfile_s(&pxFile);
	ZENITH_ASSERT_NOT_NULL(pxFile, "tmpfile() must open for the disabled-report round-trip");
	if (pxFile != nullptr)
	{
		pxProfiling->WriteBootReport(pxFile);

		Zenith_Vector<char> xText;
		ReadWholeFile(pxFile, xText);
		ZENITH_ASSERT_NOT_NULL(strstr(xText.GetDataPointer(), "boot capture disabled"),
			"with no capture the report must say so rather than print an empty table");
		fclose(pxFile);
	}

	delete pxProfiling;
}

// Signed tick deltas: an event that began before its reference point yields a
// NEGATIVE offset instead of underflowing to ~1.8e19 ns and vanishing.
ZENITH_TEST(Profiling, ProfilingSignedTickDelta)
{
	ZENITH_ASSERT_TRUE(SignedTickDeltaToMs(100, 50) < 0.0, "a backwards delta must be negative, not a huge positive");
	ZENITH_ASSERT_TRUE(SignedTickDeltaToMs(50, 100) > 0.0, "a forwards delta must stay positive");
	ZENITH_ASSERT_TRUE(SignedTickDeltaToMs(50, 50) == 0.0, "a zero delta must be exactly zero");
	// The unsigned form is what the timeline used to use — this is the bug being fixed.
	ZENITH_ASSERT_TRUE(TickDeltaToMs(100, 50) > 0.0, "the unsigned form underflows (documenting why the signed one exists)");
}

// --boot-profile-dump[=path] parsing, exercised through the pure splitter so the
// process-wide parse state is never re-run (which would clobber the rest of the batch).
ZENITH_TEST(CommandLine, BootFlags)
{
	const char* szDefault = "zenith_boot_profile_dump.txt";
	ZENITH_ASSERT_STREQ(Zenith_CommandLine::ResolveBootProfileDumpArg("--boot-profile-dump", szDefault), szDefault,
		"the bare form must use the default filename");
	ZENITH_ASSERT_STREQ(Zenith_CommandLine::ResolveBootProfileDumpArg("--boot-profile-dump=", szDefault), szDefault,
		"a trailing '=' with no path is still the bare form");
	ZENITH_ASSERT_STREQ(Zenith_CommandLine::ResolveBootProfileDumpArg("--boot-profile-dump=C:/tmp/boot.txt", szDefault), "C:/tmp/boot.txt",
		"the =path form must return everything after the first '='");
	ZENITH_ASSERT_STREQ(Zenith_CommandLine::ResolveBootProfileDumpArg(nullptr, szDefault), szDefault,
		"a null argument must fall back to the default");
}

#ifdef ZENITH_TOOLS

// The boot timeline's overlap index. A viewport deep into boot must still pick up a
// long scope that ENTERED it from far to the left — the running-max-end search is what
// makes that possible without walking every event from index 0.
ZENITH_TEST(Profiling, BootTimelineOverlapIndex)
{
	Zenith_Vector<Zenith_Profiling::BootRawEvent> xEvents;
	const auto xPush = [&xEvents](const u_int64 uBegin, const u_int64 uEnd)
	{
		Zenith_Profiling::BootRawEvent xRaw;
		xRaw.m_xEvent = Zenith_Profiling::Event(uBegin, uEnd, 1, 0);
		xRaw.m_uThreadID = 0;
		xEvents.PushBack(xRaw);
	};

	// A scope spanning the whole boot, then two short ones well after it started.
	xPush(0, 1000);
	xPush(10, 20);
	xPush(30, 40);

	const Zenith_Vector<u_int64> xMaxEnd = BuildRunningMaxEnd(xEvents);
	ZENITH_ASSERT_EQ(xMaxEnd.Get(0), static_cast<u_int64>(1000), "running max-end must be monotonic from the first event");
	ZENITH_ASSERT_EQ(xMaxEnd.Get(2), static_cast<u_int64>(1000), "a later shorter event must not lower the running max");
	ZENITH_ASSERT_EQ(FirstPossiblyVisibleBootEvent(xMaxEnd, 500), 0u,
		"a window at tick 500 must walk back to the long scope that entered it");

	// Without the long scope, the same window skips everything that already ended.
	Zenith_Vector<Zenith_Profiling::BootRawEvent> xShort;
	{
		Zenith_Profiling::BootRawEvent xRaw;
		xRaw.m_uThreadID = 0;
		xRaw.m_xEvent = Zenith_Profiling::Event(0, 5, 1, 0);   xShort.PushBack(xRaw);
		xRaw.m_xEvent = Zenith_Profiling::Event(10, 20, 1, 0); xShort.PushBack(xRaw);
		xRaw.m_xEvent = Zenith_Profiling::Event(30, 40, 1, 0); xShort.PushBack(xRaw);
	}
	const Zenith_Vector<u_int64> xShortMaxEnd = BuildRunningMaxEnd(xShort);
	ZENITH_ASSERT_EQ(FirstPossiblyVisibleBootEvent(xShortMaxEnd, 25), 2u, "events that ended before the window must be skipped");
	ZENITH_ASSERT_EQ(FirstPossiblyVisibleBootEvent(xShortMaxEnd, 9999), 3u, "a window past everything must yield the list size");

	ZENITH_ASSERT_EQ(LastPossiblyVisibleBootEvent(xShort, 25), 2u, "the upper bound is one past the last begin before the window end");
	ZENITH_ASSERT_EQ(LastPossiblyVisibleBootEvent(xShort, 0), 0u, "a window ending at the origin contains nothing");
}

// LOD + draw budget: a sub-half-pixel bar cannot be seen, and the hard cap keeps a
// fully zoomed-out boot bounded instead of queueing hundreds of thousands of draws.
ZENITH_TEST(Profiling, BootTimelineLODBudget)
{
	ZENITH_ASSERT_TRUE(BootTimelineShouldDraw(1.0f, 0, 100), "a visible bar under budget must draw");
	ZENITH_ASSERT_FALSE(BootTimelineShouldDraw(0.4f, 0, 100), "a sub-half-pixel bar must be skipped");
	ZENITH_ASSERT_TRUE(BootTimelineShouldDraw(0.5f, 0, 100), "exactly half a pixel is the inclusive threshold");
	ZENITH_ASSERT_FALSE(BootTimelineShouldDraw(1000.0f, 100, 100), "the budget is a HARD cap, whatever the bar width");
	ZENITH_ASSERT_FALSE(BootTimelineShouldDraw(1000.0f, 101, 100), "past the budget stays refused");
}

#endif // ZENITH_TOOLS

// Live engine state, READ-ONLY: this batch runs inside the suspended window that
// Zenith_Engine::InitialiseProject opens around RunAllTests. If this ever reads
// ACTIVE, the profiler's own self-tests (ProfilingOverflow especially) are being
// fought by the ring-full routing path.
ZENITH_TEST(Profiling, BootCaptureLiveEngineState)
{
	Zenith_Profiling& xProfiling = g_xEngine.Profiling();
	Zenith_Profiling::BootControlBlock* pxControl = xProfiling.GetControlBlock();
	ZENITH_ASSERT_NOT_NULL(pxControl, "the live profiler must own a control block from construction");

	const u_int uState = pxControl->m_uState.load(std::memory_order_acquire);
	if (xProfiling.GetBootCapture() == nullptr)
	{
		ZENITH_ASSERT_EQ(uState, static_cast<u_int>(Zenith_Profiling::BOOT_CAPTURE_DISABLED),
			"no capture allocated means --skip-boot-capture, which must leave the state DISABLED");
	}
	else
	{
		ZENITH_ASSERT_EQ(uState, static_cast<u_int>(Zenith_Profiling::BOOT_CAPTURE_SUSPENDED),
			"boot capture MUST be suspended across the unit-test batch");
	}
	ZENITH_ASSERT_FALSE(xProfiling.IsBootCaptureSealed(), "the capture cannot be sealed yet — Zenith_Init has not returned");
}

#endif // ZENITH_PROFILING_ENABLED
