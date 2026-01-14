#include "Zenith.h"

#ifdef ZENITH_TOOLS
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

#include "Zenith_EditorPanel_Memory.h"
#include "Memory/Zenith_MemoryManagement.h"
#include "Memory/Zenith_MemoryTracker.h"
#include "Memory/Zenith_MemoryBudgets.h"
#include "Memory/Zenith_MemoryCategories.h"
#include "Callstack/Zenith_Callstack.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "Physics/Zenith_Physics.h"
#ifdef ZENITH_WINDOWS
#include "Windows/Zenith_Windows_Window.h"
#endif

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include <algorithm>
#include <vector>

#ifdef ZENITH_WINDOWS
#include <windows.h>
#include <psapi.h>
#endif

namespace Zenith_EditorPanelMemory
{
	static bool s_bVisible = false;

#ifdef ZENITH_WINDOWS
	// Get actual process memory from Windows (what Task Manager shows)
	static void GetProcessMemoryStats(u_int64& ulWorkingSet, u_int64& ulPrivateBytes, u_int64& ulVirtualBytes)
	{
		PROCESS_MEMORY_COUNTERS_EX xMemInfo = {};
		xMemInfo.cb = sizeof(xMemInfo);
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&xMemInfo), sizeof(xMemInfo)))
		{
			ulWorkingSet = xMemInfo.WorkingSetSize;
			ulPrivateBytes = xMemInfo.PrivateUsage;
			ulVirtualBytes = xMemInfo.PagefileUsage;
		}
		else
		{
			ulWorkingSet = 0;
			ulPrivateBytes = 0;
			ulVirtualBytes = 0;
		}
	}
#endif
	static int s_iSelectedAllocationIndex = -1;
	static int s_iSortColumn = 1;  // Sort by size by default
	static bool s_bSortAscending = false;  // Descending by default for size
	static char s_acFilterText[256] = "";
	static int s_iTabSelection = 0;

	// Helper to format bytes
	static void FormatBytes(u_int64 ulBytes, char* szBuffer, size_t ulBufferSize)
	{
		if (ulBytes >= 1024ULL * 1024 * 1024)
		{
			snprintf(szBuffer, ulBufferSize, "%.2f GB", static_cast<double>(ulBytes) / (1024.0 * 1024.0 * 1024.0));
		}
		else if (ulBytes >= 1024ULL * 1024)
		{
			snprintf(szBuffer, ulBufferSize, "%.2f MB", static_cast<double>(ulBytes) / (1024.0 * 1024.0));
		}
		else if (ulBytes >= 1024)
		{
			snprintf(szBuffer, ulBufferSize, "%.2f KB", static_cast<double>(ulBytes) / 1024.0);
		}
		else
		{
			snprintf(szBuffer, ulBufferSize, "%llu B", ulBytes);
		}
	}

	static void RenderSummaryTab()
	{
		const Zenith_MemoryStats& xStats = Zenith_MemoryManagement::GetStats();

		char acBuffer[64];

#ifdef ZENITH_WINDOWS
		// Process memory from Windows (what Task Manager shows)
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Process Memory (Windows)");
		ImGui::Separator();

		u_int64 ulWorkingSet = 0, ulPrivateBytes = 0, ulVirtualBytes = 0;
		GetProcessMemoryStats(ulWorkingSet, ulPrivateBytes, ulVirtualBytes);

		FormatBytes(ulWorkingSet, acBuffer, sizeof(acBuffer));
		ImGui::Text("Working Set: %s", acBuffer);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Physical memory currently in use (what Task Manager shows)");
		}

		FormatBytes(ulPrivateBytes, acBuffer, sizeof(acBuffer));
		ImGui::Text("Private Bytes: %s", acBuffer);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Memory committed by the process (heap + stack + other)");
		}

		ImGui::Separator();
#endif

		ImGui::Text("Tracked Memory Summary");
		ImGui::Separator();

		// Current usage
		FormatBytes(xStats.m_ulTotalAllocated, acBuffer, sizeof(acBuffer));
		ImGui::Text("Allocated: %s (%llu allocations)", acBuffer, xStats.m_ulTotalAllocationCount);

		FormatBytes(xStats.m_ulPeakAllocated, acBuffer, sizeof(acBuffer));
		ImGui::Text("Peak: %s (%llu peak allocations)", acBuffer, xStats.m_ulPeakAllocationCount);

		ImGui::Separator();

		// Lifetime stats
		FormatBytes(xStats.m_ulTotalBytesAllocatedLifetime, acBuffer, sizeof(acBuffer));
		ImGui::Text("Lifetime allocated: %s", acBuffer);
		ImGui::Text("Total allocations: %llu", xStats.m_ulTotalAllocationsLifetime);
		ImGui::Text("Total deallocations: %llu", xStats.m_ulTotalDeallocationsLifetime);

		ImGui::Separator();

		// Frame stats
		double fDeltaKB = static_cast<double>(xStats.m_ilFrameDelta) / 1024.0;
		ImGui::Text("Frame delta: %+.2f KB", fDeltaKB);
		ImGui::Text("Frame allocs: %u | deallocs: %u",
			xStats.m_uFrameAllocations, xStats.m_uFrameDeallocations);

		ImGui::Separator();

		// GPU/VMA Memory Stats - use VMA's own statistics for accuracy
		ImGui::Text("GPU Memory (VMA)");
		ImGui::Separator();

		Zenith_Vulkan_MemoryManager::VMAStats xVmaStats = Zenith_Vulkan_MemoryManager::GetVMAStats();

		// Manual tracking (for comparison)
		u_int64 ulImageMem = *Zenith_Vulkan_MemoryManager::GetImageMemoryUsagePtr();
		u_int64 ulBufferMem = *Zenith_Vulkan_MemoryManager::GetBufferMemoryUsagePtr();

		FormatBytes(ulImageMem, acBuffer, sizeof(acBuffer));
		ImGui::Text("Image Memory (tracked): %s", acBuffer);

		FormatBytes(ulBufferMem, acBuffer, sizeof(acBuffer));
		ImGui::Text("Buffer Memory (tracked): %s", acBuffer);

		// VMA's accurate statistics
		FormatBytes(xVmaStats.m_ulTotalUsedBytes, acBuffer, sizeof(acBuffer));
		ImGui::Text("VMA Used: %s (%llu allocs)", acBuffer, xVmaStats.m_ulAllocationCount);

		FormatBytes(xVmaStats.m_ulTotalAllocatedBytes, acBuffer, sizeof(acBuffer));
		ImGui::Text("VMA Allocated (blocks): %s", acBuffer);

		ImGui::Separator();

		// Jolt Physics Memory - tracked separately as it uses its own allocator
		ImGui::Text("Physics Memory (Jolt)");
		ImGui::Separator();

		u_int64 ulJoltMem = Zenith_Physics::GetJoltMemoryAllocated();
		u_int64 ulJoltAllocs = Zenith_Physics::GetJoltAllocationCount();

		FormatBytes(ulJoltMem, acBuffer, sizeof(acBuffer));
		ImGui::Text("Jolt Allocated: %s (%llu allocs)", acBuffer, ulJoltAllocs);

		ImGui::Separator();

		// ImGui Memory - tracked separately as it uses its own allocator
		ImGui::Text("UI Memory (ImGui)");
		ImGui::Separator();

		u_int64 ulImGuiMem = Zenith_Vulkan::GetImGuiMemoryAllocated();
		u_int64 ulImGuiAllocs = Zenith_Vulkan::GetImGuiAllocationCount();

		FormatBytes(ulImGuiMem, acBuffer, sizeof(acBuffer));
		ImGui::Text("ImGui Allocated: %s (%llu allocs)", acBuffer, ulImGuiAllocs);

		// GLFW Memory - tracked separately as it uses its own allocator (Windows only)
#ifdef ZENITH_WINDOWS
		ImGui::Separator();

		ImGui::Text("Window System (GLFW)");
		ImGui::Separator();

		u_int64 ulGLFWMem = Zenith_Window::GetGLFWMemoryAllocated();
		u_int64 ulGLFWAllocs = Zenith_Window::GetGLFWAllocationCount();

		FormatBytes(ulGLFWMem, acBuffer, sizeof(acBuffer));
		ImGui::Text("GLFW Allocated: %s (%llu allocs)", acBuffer, ulGLFWAllocs);
#else
		u_int64 ulGLFWMem = 0;
#endif

		// Combined total using all tracked memory
		ImGui::Separator();
		u_int64 ulCombinedTotal = xStats.m_ulTotalAllocated + xVmaStats.m_ulTotalAllocatedBytes + ulJoltMem + ulImGuiMem + ulGLFWMem;
		FormatBytes(ulCombinedTotal, acBuffer, sizeof(acBuffer));
		ImGui::Text("Combined Tracked: %s", acBuffer);

#ifdef ZENITH_WINDOWS
		// Show untracked memory (difference between process memory and tracked)
		u_int64 ulWorkingSetNow = 0, ulPrivateBytesNow = 0, ulVirtualBytesNow = 0;
		GetProcessMemoryStats(ulWorkingSetNow, ulPrivateBytesNow, ulVirtualBytesNow);

		// Untracked = Process private bytes - (CPU tracked + external library tracked)
		// Note: VMA memory is GPU memory, not part of process heap
		u_int64 ulCPUTracked = xStats.m_ulTotalAllocated + ulJoltMem + ulImGuiMem + ulGLFWMem;
		int64_t ilUntracked = static_cast<int64_t>(ulPrivateBytesNow) - static_cast<int64_t>(ulCPUTracked);

		if (ilUntracked > 0)
		{
			FormatBytes(static_cast<u_int64>(ilUntracked), acBuffer, sizeof(acBuffer));
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Untracked (est): %s", acBuffer);
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"Estimated untracked memory:\n"
					"- Vulkan driver internal allocations\n"
					"- CRT heap overhead & fragmentation\n"
					"- Thread stacks & TLS\n"
					"- Memory tracker's own data structures\n"
					"- std::vector/map internal storage"
				);
			}
		}
#endif

		ImGui::Separator();

		// Actions
		if (ImGui::Button("Report Leaks"))
		{
			Zenith_MemoryManagement::ReportLeaks();
		}
		ImGui::SameLine();
		if (ImGui::Button("Check Guards"))
		{
			Zenith_MemoryManagement::CheckAllGuards();
		}
		ImGui::SameLine();
		if (ImGui::Button("Dump Categories"))
		{
			Zenith_MemoryManagement::DumpAllocationsByCategory();
		}
	}

	static void RenderCategoryTab()
	{
		const Zenith_MemoryStats& xStats = Zenith_MemoryManagement::GetStats();

		ImGui::Text("Memory by Category");
		ImGui::Separator();

		char acBuffer[64];

		// Table of categories
		if (ImGui::BeginTable("CategoryTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Allocated", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Peak", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Budget", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
			{
				Zenith_MemoryCategory eCategory = static_cast<Zenith_MemoryCategory>(i);

				// Skip empty categories
				if (xStats.m_aulCategoryAllocationCount[i] == 0 &&
					!Zenith_MemoryBudgets::GetBudgetInfo(eCategory).m_bEnabled)
				{
					continue;
				}

				ImGui::TableNextRow();

				// Category name
				ImGui::TableNextColumn();
				ImGui::Text("%s", GetMemoryCategoryName(eCategory));

				// Allocated
				ImGui::TableNextColumn();
				FormatBytes(xStats.m_aulCategoryAllocated[i], acBuffer, sizeof(acBuffer));
				ImGui::Text("%s", acBuffer);

				// Count
				ImGui::TableNextColumn();
				ImGui::Text("%llu", xStats.m_aulCategoryAllocationCount[i]);

				// Peak
				ImGui::TableNextColumn();
				FormatBytes(xStats.m_aulCategoryPeakAllocated[i], acBuffer, sizeof(acBuffer));
				ImGui::Text("%s", acBuffer);

				// Budget bar
				ImGui::TableNextColumn();
				const Zenith_MemoryBudget& xBudget = Zenith_MemoryBudgets::GetBudgetInfo(eCategory);
				if (xBudget.m_bEnabled && xBudget.m_ulBudgetBytes > 0)
				{
					float fPercent = Zenith_MemoryBudgets::GetBudgetUsagePercent(eCategory) / 100.0f;

					// Color based on usage
					ImVec4 xColor;
					if (fPercent > 1.0f)
					{
						xColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red - over budget
					}
					else if (fPercent > 0.8f)
					{
						xColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange - near budget
					}
					else
					{
						xColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);  // Green - under budget
					}

					ImGui::PushStyleColor(ImGuiCol_PlotHistogram, xColor);
					ImGui::ProgressBar(fPercent > 1.0f ? 1.0f : fPercent, ImVec2(-1, 0),
						acBuffer);
					ImGui::PopStyleColor();

					// Show actual vs budget in tooltip
					if (ImGui::IsItemHovered())
					{
						char acBudgetStr[64];
						FormatBytes(xBudget.m_ulBudgetBytes, acBudgetStr, sizeof(acBudgetStr));
						ImGui::SetTooltip("%.1f%% of %s budget", fPercent * 100.0f, acBudgetStr);
					}
				}
				else
				{
					ImGui::Text("No budget");
				}
			}

			ImGui::EndTable();
		}
	}

	static void RenderAllocationTab()
	{
		ImGui::Text("Allocation List");
		ImGui::Separator();

		// Filter
		ImGui::InputText("Filter", s_acFilterText, sizeof(s_acFilterText));
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			s_acFilterText[0] = '\0';
		}

		// Collect allocations
		std::vector<const Zenith_AllocationRecord*> axRecords;
		axRecords.reserve(Zenith_MemoryTracker::GetAllocationCount());

		Zenith_MemoryTracker::ForEachAllocation(
			[](const Zenith_AllocationRecord& xRecord, void* pUserData)
			{
				auto* pRecords = static_cast<std::vector<const Zenith_AllocationRecord*>*>(pUserData);
				pRecords->push_back(&xRecord);
			},
			&axRecords
		);

		// Filter
		if (s_acFilterText[0] != '\0')
		{
			axRecords.erase(
				std::remove_if(axRecords.begin(), axRecords.end(),
					[](const Zenith_AllocationRecord* pRecord)
					{
						// Filter by file name or category
						if (pRecord->m_szFile != nullptr &&
							strstr(pRecord->m_szFile, s_acFilterText) != nullptr)
						{
							return false;
						}
						if (strstr(GetMemoryCategoryName(pRecord->m_eCategory), s_acFilterText) != nullptr)
						{
							return false;
						}
						return true;
					}
				),
				axRecords.end()
			);
		}

		// Sort - use proper strict weak ordering (swap operands for descending, don't negate)
		std::sort(axRecords.begin(), axRecords.end(),
			[](const Zenith_AllocationRecord* a, const Zenith_AllocationRecord* b)
			{
				// For descending order, swap a and b instead of negating result
				// Negating breaks strict weak ordering when a == b
				const Zenith_AllocationRecord* pLeft = s_bSortAscending ? a : b;
				const Zenith_AllocationRecord* pRight = s_bSortAscending ? b : a;

				switch (s_iSortColumn)
				{
				case 0:  // Address
					return pLeft->m_pAddress < pRight->m_pAddress;
				case 1:  // Size
					return pLeft->m_ulSize < pRight->m_ulSize;
				case 2:  // Category
					return pLeft->m_eCategory < pRight->m_eCategory;
				case 3:  // ID
					return pLeft->m_ulAllocationID < pRight->m_ulAllocationID;
				default:
					return pLeft->m_ulSize < pRight->m_ulSize;
				}
			}
		);

		ImGui::Text("%zu allocations", axRecords.size());

		// Table
		if (ImGui::BeginTable("AllocationTable", 5,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_Resizable,
			ImVec2(0, 300)))
		{
			ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
			ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_DefaultSort, 0.0f, 2);
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, 3);
			ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_NoSort);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			// Handle sorting
			if (ImGuiTableSortSpecs* pSortSpecs = ImGui::TableGetSortSpecs())
			{
				if (pSortSpecs->SpecsDirty && pSortSpecs->SpecsCount > 0)
				{
					s_iSortColumn = static_cast<int>(pSortSpecs->Specs[0].ColumnUserID);
					s_bSortAscending = (pSortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
					pSortSpecs->SpecsDirty = false;
				}
			}

			char acBuffer[64];
			ImGuiListClipper xClipper;
			xClipper.Begin(static_cast<int>(axRecords.size()));

			while (xClipper.Step())
			{
				for (int i = xClipper.DisplayStart; i < xClipper.DisplayEnd; ++i)
				{
					const Zenith_AllocationRecord* pRecord = axRecords[i];

					ImGui::TableNextRow();

					// Selectable row
					ImGui::TableNextColumn();
					bool bSelected = (s_iSelectedAllocationIndex == i);
					char acLabel[32];
					snprintf(acLabel, sizeof(acLabel), "0x%p", pRecord->m_pAddress);
					if (ImGui::Selectable(acLabel, bSelected, ImGuiSelectableFlags_SpanAllColumns))
					{
						s_iSelectedAllocationIndex = bSelected ? -1 : i;
					}

					// Size
					ImGui::TableNextColumn();
					FormatBytes(pRecord->m_ulSize, acBuffer, sizeof(acBuffer));
					ImGui::Text("%s", acBuffer);

					// Category
					ImGui::TableNextColumn();
					ImGui::Text("%s", GetMemoryCategoryName(pRecord->m_eCategory));

					// ID
					ImGui::TableNextColumn();
					ImGui::Text("%llu", pRecord->m_ulAllocationID);

					// Location
					ImGui::TableNextColumn();
					if (pRecord->m_szFile != nullptr)
					{
						ImGui::Text("%s:%d", pRecord->m_szFile, pRecord->m_iLine);
					}
					else
					{
						ImGui::TextDisabled("Unknown");
					}
				}
			}

			ImGui::EndTable();
		}

		// Show callstack for selected allocation
		if (s_iSelectedAllocationIndex >= 0 && s_iSelectedAllocationIndex < static_cast<int>(axRecords.size()))
		{
			const Zenith_AllocationRecord* pRecord = axRecords[s_iSelectedAllocationIndex];

			ImGui::Separator();
			ImGui::Text("Callstack for allocation #%llu:", pRecord->m_ulAllocationID);

			if (pRecord->m_uCallstackDepth > 0)
			{
				for (u_int j = 0; j < pRecord->m_uCallstackDepth; ++j)
				{
					Zenith_CallstackFrame xFrame;
					if (Zenith_Callstack::Symbolicate(pRecord->m_apCallstack[j], xFrame))
					{
						if (xFrame.m_uLine > 0 && xFrame.m_acFile[0] != '\0')
						{
							ImGui::Text("  [%u] %s (%s:%u)", j, xFrame.m_acSymbol, xFrame.m_acFile, xFrame.m_uLine);
						}
						else
						{
							ImGui::Text("  [%u] %s", j, xFrame.m_acSymbol);
						}
					}
					else
					{
						ImGui::Text("  [%u] 0x%p", j, pRecord->m_apCallstack[j]);
					}
				}
			}
			else
			{
				ImGui::TextDisabled("  No callstack available");
			}
		}
	}

	static void RenderBudgetTab()
	{
		ImGui::Text("Memory Budgets Configuration");
		ImGui::Separator();

		static int s_iSelectedCategory = 0;
		static u_int64 s_ulBudgetMB = 0;
		static u_int64 s_ulWarningMB = 0;

		// Category selector
		if (ImGui::BeginCombo("Category", GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(s_iSelectedCategory))))
		{
			for (int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
			{
				bool bSelected = (s_iSelectedCategory == i);
				if (ImGui::Selectable(GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(i)), bSelected))
				{
					s_iSelectedCategory = i;
					const Zenith_MemoryBudget& xBudget = Zenith_MemoryBudgets::GetBudgetInfo(static_cast<Zenith_MemoryCategory>(i));
					s_ulBudgetMB = xBudget.m_ulBudgetBytes / (1024 * 1024);
					s_ulWarningMB = xBudget.m_ulWarningBytes / (1024 * 1024);
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// Budget input (in MB for convenience)
		int iBudgetMB = static_cast<int>(s_ulBudgetMB);
		if (ImGui::InputInt("Budget (MB)", &iBudgetMB))
		{
			s_ulBudgetMB = iBudgetMB > 0 ? static_cast<u_int64>(iBudgetMB) : 0;
		}

		int iWarningMB = static_cast<int>(s_ulWarningMB);
		if (ImGui::InputInt("Warning (MB)", &iWarningMB))
		{
			s_ulWarningMB = iWarningMB > 0 ? static_cast<u_int64>(iWarningMB) : 0;
		}

		if (ImGui::Button("Set Budget"))
		{
			Zenith_MemoryBudgets::SetBudget(
				static_cast<Zenith_MemoryCategory>(s_iSelectedCategory),
				s_ulBudgetMB * 1024 * 1024,
				s_ulWarningMB * 1024 * 1024
			);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Budget"))
		{
			Zenith_MemoryBudgets::ClearBudget(static_cast<Zenith_MemoryCategory>(s_iSelectedCategory));
		}

		ImGui::Separator();

		// Show current budgets
		ImGui::Text("Active Budgets:");
		for (int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
		{
			const Zenith_MemoryBudget& xBudget = Zenith_MemoryBudgets::GetBudgetInfo(static_cast<Zenith_MemoryCategory>(i));
			if (xBudget.m_bEnabled)
			{
				char acBudget[64], acWarning[64];
				FormatBytes(xBudget.m_ulBudgetBytes, acBudget, sizeof(acBudget));
				FormatBytes(xBudget.m_ulWarningBytes, acWarning, sizeof(acWarning));
				ImGui::BulletText("%s: %s (warn at %s)",
					GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(i)),
					acBudget, acWarning);
			}
		}
	}

	void Render()
	{
		if (!s_bVisible)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Memory Profiler", &s_bVisible))
		{
			if (ImGui::BeginTabBar("MemoryTabs"))
			{
				if (ImGui::BeginTabItem("Summary"))
				{
					RenderSummaryTab();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Categories"))
				{
					RenderCategoryTab();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Allocations"))
				{
					RenderAllocationTab();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Budgets"))
				{
					RenderBudgetTab();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}
		ImGui::End();
	}

	void SetVisible(bool bVisible)
	{
		s_bVisible = bVisible;
	}

	bool IsVisible()
	{
		return s_bVisible;
	}
}

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED
#endif // ZENITH_TOOLS
