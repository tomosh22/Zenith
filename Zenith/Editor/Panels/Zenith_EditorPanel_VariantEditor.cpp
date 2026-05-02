#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_VariantEditor.h"
#include "Editor/Zenith_Editor.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"

#include <string>
#include <cstring>
#include <cstdio>

namespace
{
	bool s_bVisible = true;

	// === Authoring state ===
	// Path to the base prefab the new variant inherits from.
	char s_acBasePath[512]   = {};
	// Name field for the new variant.
	char s_acVariantName[128] = "NewVariant";
	// Path the next save will write to (defaults to <name>.zpfb in cwd).
	char s_acSavePath[512]   = "NewVariant.zpfb";

	// In-memory variant currently being authored. Owned by this panel; deleted
	// on destruction or when the user starts a new variant.
	Zenith_Prefab* s_pxAuthoringVariant = nullptr;

	// === Inspection state ===
	// Path of an existing prefab (variant or otherwise) being inspected
	// read-only. Resolved through the asset registry — never authored against.
	char s_acInspectPath[512] = {};

	// --- Override input form state ---
	// Component name dropdown choice (typically just "Transform" until more
	// components register flat overrideable properties).
	int  s_iOverrideComponentIdx = 0;
	int  s_iOverridePropertyIdx  = 0;
	float s_afOverrideValue[3]   = { 0.0f, 0.0f, 0.0f };

	struct OverrideTemplate
	{
		const char* m_szComponentName;
		const char* m_szPropertyName;
		const char* m_szLabel;
	};

	// What the form supports today: Vector3 fields on registered components.
	// More types (float, bool, asset handle) are a future add — would need a
	// dynamic value-input widget.
	const OverrideTemplate s_axOverrideTemplates[] = {
		{ "Transform", "Position", "Transform.Position (vec3)" },
		{ "Transform", "Scale",    "Transform.Scale (vec3)"    },
	};
	constexpr int s_iOverrideTemplateCount = sizeof(s_axOverrideTemplates) / sizeof(OverrideTemplate);

	// Try to load a prefab through the registry. Catches common failures so the
	// panel doesn't crash on a bad path. Returns null on miss.
	Zenith_Prefab* TryLoadPrefab(const char* szPath)
	{
		if (szPath == nullptr || szPath[0] == '\0') return nullptr;
		// IsLoaded gate: avoid triggering a registry load that asserts on missing files.
		if (Zenith_AssetRegistry::IsLoaded(szPath))
		{
			return Zenith_AssetRegistry::Get<Zenith_Prefab>(szPath);
		}
		// Path may be valid but unloaded; attempt the load. Worst case the
		// registry will warn or fail silently — the user retries.
		return Zenith_AssetRegistry::Get<Zenith_Prefab>(szPath);
	}

	void RenderHeaderHelp()
	{
		ImGui::TextWrapped(
			"Author prefab variants: pick a base, add overrides for a few flat "
			"properties (Transform.Position / Transform.Scale), save to disk. "
			"See Prefab/CLAUDE.md for the full feature set.");
		ImGui::Separator();
	}

	void RenderBasePrefabSection()
	{
		ImGui::TextUnformatted("Base prefab");
		ImGui::PushID("base");
		ImGui::InputText("Path", s_acBasePath, sizeof(s_acBasePath));
		// Drag-drop target reuses the Content Browser's prefab payload.
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_PREFAB))
			{
				const DragDropFilePayload* pxFile = static_cast<const DragDropFilePayload*>(pxPayload->Data);
				strncpy_s(s_acBasePath, sizeof(s_acBasePath), pxFile->m_szFilePath, _TRUNCATE);
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopID();
	}

	void RenderCreateVariantSection()
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Variant authoring");
		ImGui::InputText("Name##variantname", s_acVariantName, sizeof(s_acVariantName));
		ImGui::InputText("Save path##savepath", s_acSavePath, sizeof(s_acSavePath));

		const bool bHasBase = s_acBasePath[0] != '\0';
		ImGui::BeginDisabled(!bHasBase);
		if (ImGui::Button("Create variant"))
		{
			Zenith_Prefab* pxBase = TryLoadPrefab(s_acBasePath);
			if (pxBase != nullptr)
			{
				PrefabHandle xHandle(s_acBasePath);
				delete s_pxAuthoringVariant;
				s_pxAuthoringVariant = new Zenith_Prefab();
				if (!s_pxAuthoringVariant->CreateAsVariant(xHandle, s_acVariantName))
				{
					Zenith_Warning(LOG_CATEGORY_ECS,
						"VariantEditor: CreateAsVariant rejected — likely a cycle or unset handle.");
					delete s_pxAuthoringVariant;
					s_pxAuthoringVariant = nullptr;
				}
			}
			else
			{
				Zenith_Warning(LOG_CATEGORY_ECS, "VariantEditor: could not load base prefab '%s'", s_acBasePath);
			}
		}
		ImGui::EndDisabled();
		if (!bHasBase) { ImGui::SameLine(); ImGui::TextDisabled("(set base prefab first)"); }
	}

	void RenderActiveVariantOverrideForm()
	{
		if (s_pxAuthoringVariant == nullptr) return;

		ImGui::Separator();
		ImGui::Text("Editing: %s (%u override%s)",
			s_pxAuthoringVariant->GetName().c_str(),
			s_pxAuthoringVariant->GetOverrides().GetSize(),
			s_pxAuthoringVariant->GetOverrides().GetSize() == 1 ? "" : "s");

		// Override list (read-only — overrides are append-only via this panel).
		const Zenith_Vector<Zenith_PropertyOverride>& xOverrides = s_pxAuthoringVariant->GetOverrides();
		for (u_int u = 0; u < xOverrides.GetSize(); ++u)
		{
			const Zenith_PropertyOverride& xOv = xOverrides.Get(u);
			ImGui::BulletText("%s.%s", xOv.m_strComponentName.c_str(), xOv.m_strPropertyPath.c_str());
		}
		if (xOverrides.GetSize() > 0 && ImGui::Button("Clear all overrides"))
		{
			s_pxAuthoringVariant->ClearOverrides();
		}

		// Override input form — Vector3 fields on Transform only for now.
		ImGui::Separator();
		ImGui::TextUnformatted("Add override");

		// Build the dropdown out of the static template list.
		if (ImGui::BeginCombo("Field##overridefield",
			s_axOverrideTemplates[s_iOverridePropertyIdx].m_szLabel))
		{
			for (int i = 0; i < s_iOverrideTemplateCount; ++i)
			{
				const bool bSelected = (i == s_iOverridePropertyIdx);
				if (ImGui::Selectable(s_axOverrideTemplates[i].m_szLabel, bSelected))
				{
					s_iOverridePropertyIdx = i;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat3("Value##overridevalue", s_afOverrideValue, 0.05f);

		if (ImGui::Button("Add"))
		{
			Zenith_PropertyOverride xOv;
			xOv.m_strComponentName = s_axOverrideTemplates[s_iOverridePropertyIdx].m_szComponentName;
			xOv.m_strPropertyPath  = s_axOverrideTemplates[s_iOverridePropertyIdx].m_szPropertyName;
			xOv.m_xValue << Zenith_Maths::Vector3(s_afOverrideValue[0], s_afOverrideValue[1], s_afOverrideValue[2]);
			s_pxAuthoringVariant->AddOverride(std::move(xOv));
		}

		// Save button — writes the in-memory variant to s_acSavePath as a .zpfb.
		ImGui::Separator();
		if (ImGui::Button("Save to disk"))
		{
			const bool bOk = s_pxAuthoringVariant->SaveToFile(s_acSavePath);
			if (bOk)
			{
				Zenith_Log(LOG_CATEGORY_ECS, "VariantEditor: saved variant '%s' to %s",
					s_pxAuthoringVariant->GetName().c_str(), s_acSavePath);
			}
			else
			{
				Zenith_Warning(LOG_CATEGORY_ECS, "VariantEditor: save to '%s' failed", s_acSavePath);
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Discard"))
		{
			delete s_pxAuthoringVariant;
			s_pxAuthoringVariant = nullptr;
		}
	}

	void RenderInspectSection()
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Inspect existing prefab (read-only)");
		ImGui::PushID("inspect");
		ImGui::InputText("Path", s_acInspectPath, sizeof(s_acInspectPath));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_PREFAB))
			{
				const DragDropFilePayload* pxFile = static_cast<const DragDropFilePayload*>(pxPayload->Data);
				strncpy_s(s_acInspectPath, sizeof(s_acInspectPath), pxFile->m_szFilePath, _TRUNCATE);
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopID();

		Zenith_Prefab* pxInspected = TryLoadPrefab(s_acInspectPath);
		if (pxInspected == nullptr)
		{
			ImGui::TextDisabled("(drop a .zpfb here to inspect)");
			return;
		}
		ImGui::Text("Name: %s", pxInspected->GetName().c_str());
		ImGui::Text("Variant: %s", pxInspected->IsVariant() ? "yes" : "no");
		if (pxInspected->IsVariant())
		{
			ImGui::Text("Base path: %s", pxInspected->GetBasePrefab().GetPath().c_str());
		}
		const Zenith_Vector<Zenith_PropertyOverride>& xOv = pxInspected->GetOverrides();
		ImGui::Text("Overrides: %u", xOv.GetSize());
		for (u_int u = 0; u < xOv.GetSize(); ++u)
		{
			ImGui::BulletText("%s.%s", xOv.Get(u).m_strComponentName.c_str(), xOv.Get(u).m_strPropertyPath.c_str());
		}
	}
}

void Zenith_EditorPanelVariantEditor::Render()
{
	if (!s_bVisible) return;

	if (!ImGui::Begin("Variant Editor", &s_bVisible))
	{
		ImGui::End();
		return;
	}

	RenderHeaderHelp();
	RenderBasePrefabSection();
	RenderCreateVariantSection();
	RenderActiveVariantOverrideForm();
	RenderInspectSection();

	ImGui::End();
}

void Zenith_EditorPanelVariantEditor::SetVisible(bool bVisible) { s_bVisible = bVisible; }
bool Zenith_EditorPanelVariantEditor::IsVisible()                { return s_bVisible; }

#endif // ZENITH_TOOLS
