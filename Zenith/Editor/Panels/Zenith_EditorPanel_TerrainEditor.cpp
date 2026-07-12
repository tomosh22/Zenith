#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_TerrainEditor.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Core/Zenith_EditorWindowNames.h"

#include "imgui.h"

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"

namespace
{
	const char* aszToolNames[] = {
		"Raise", "Lower", "Smooth", "Flatten", "Set Height", "Noise",
		"Terrace", "Ramp", "Stamp", "Paint Layer", "Grass Density", "Trees"
	};
	static_assert(IM_ARRAYSIZE(aszToolNames) == static_cast<int>(Zenith_TerrainBrushTool::Count),
		"Tool name table out of sync with Zenith_TerrainBrushTool");

	const char* aszFalloffNames[] = { "Smooth", "Linear", "Sphere", "Sharp" };
	static_assert(IM_ARRAYSIZE(aszFalloffNames) == static_cast<int>(Zenith_TerrainBrushFalloff::Count),
		"Falloff name table out of sync with Zenith_TerrainBrushFalloff");

	char s_szAssetSetDraft[128] = {};
	std::string s_strObservedStagedSet;
	Zenith_EntityID s_uObservedTarget = INVALID_ENTITY_ID;
	bool s_bObservedSessionActive = false;
	bool s_bObservedStandalone = false;
	bool s_bHasObservedSession = false;

	void RefreshAssetSetDraftForSession(const Zenith_TerrainEditor& xEditor)
	{
		const bool bSessionChanged = !s_bHasObservedSession ||
			s_uObservedTarget != xEditor.GetTargetEntity() ||
			s_bObservedSessionActive != xEditor.IsActive() ||
			s_bObservedStandalone != xEditor.IsStandalone();
		if (bSessionChanged || s_strObservedStagedSet != xEditor.GetAssetSet())
		{
			strncpy_s(s_szAssetSetDraft, sizeof(s_szAssetSetDraft),
				xEditor.GetAssetSet().c_str(), _TRUNCATE);
			s_strObservedStagedSet = xEditor.GetAssetSet();
		}
		s_uObservedTarget = xEditor.GetTargetEntity();
		s_bObservedSessionActive = xEditor.IsActive();
		s_bObservedStandalone = xEditor.IsStandalone();
		s_bHasObservedSession = true;
	}

	// Resolve the session target for material-slot labels. Returns nullptr in
	// standalone / unresolved sessions.
	Zenith_TerrainComponent* ResolveTarget(const Zenith_TerrainEditor& xEditor)
	{
		const Zenith_EntityID uEntity = xEditor.GetTargetEntity();
		if (uEntity == INVALID_ENTITY_ID)
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(uEntity);
		if (!xEntity.IsValid())
		{
			return nullptr;
		}
		return xEntity.TryGetComponent<Zenith_TerrainComponent>();
	}

	void RenderBrushSection(Zenith_TerrainEditor& xEditor)
	{
		if (!ImGui::CollapsingHeader("Brush", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		Zenith_TerrainBrushSettings& xBrush = xEditor.m_xBrush;

		// Tool palette: 2 rows of selectable buttons.
		const int iToolCount = static_cast<int>(Zenith_TerrainBrushTool::Count);
		for (int i = 0; i < iToolCount; i++)
		{
			const bool bSelected = (static_cast<int>(xBrush.m_eTool) == i);
			if (bSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
			}
			if (ImGui::Button(aszToolNames[i]))
			{
				xBrush.m_eTool = static_cast<Zenith_TerrainBrushTool>(i);
			}
			if (bSelected)
			{
				ImGui::PopStyleColor();
			}
			if ((i % 4) != 3 && i != iToolCount - 1)
			{
				ImGui::SameLine();
			}
		}

		ImGui::SliderFloat("Radius (m)", &xBrush.m_fRadius, 1.0f, 512.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Strength", &xBrush.m_fStrength, 0.0f, 1.0f);

		int iFalloff = static_cast<int>(xBrush.m_eFalloff);
		if (ImGui::Combo("Falloff", &iFalloff, aszFalloffNames, IM_ARRAYSIZE(aszFalloffNames)))
		{
			xBrush.m_eFalloff = static_cast<Zenith_TerrainBrushFalloff>(iFalloff);
		}

		// Per-tool parameters.
		switch (xBrush.m_eTool)
		{
		case Zenith_TerrainBrushTool::Flatten:
		case Zenith_TerrainBrushTool::SetHeight:
			ImGui::SliderFloat("Target Height (m)", &xBrush.m_fTargetHeight, 0.0f, Zenith_TerrainEditor::fTERRAIN_MAX_HEIGHT);
			ImGui::TextDisabled("Ctrl+Click samples the height under the cursor");
			break;
		case Zenith_TerrainBrushTool::Noise:
			ImGui::SliderFloat("Noise Amount (m)", &xBrush.m_fNoiseAmount, 0.5f, 64.0f);
			ImGui::SliderFloat("Noise Scale", &xBrush.m_fNoiseScale, 0.005f, 0.5f, "%.3f", ImGuiSliderFlags_Logarithmic);
			break;
		case Zenith_TerrainBrushTool::Terrace:
			ImGui::SliderFloat("Step (m)", &xBrush.m_fTerraceStep, 1.0f, 64.0f);
			break;
		case Zenith_TerrainBrushTool::Ramp:
			ImGui::SliderFloat("Hardness", &xBrush.m_fRampHardness, 0.1f, 1.0f);
			ImGui::TextDisabled("Drag from the ramp's start to its end");
			break;
		case Zenith_TerrainBrushTool::Stamp:
			if (xEditor.HasStamp())
			{
				ImGui::Text("Stamp captured");
				ImGui::SameLine();
				if (ImGui::SmallButton("Clear##Stamp"))
				{
					xEditor.ClearStamp();
				}
			}
			else
			{
				ImGui::TextDisabled("Ctrl+Click captures the terrain under the cursor");
			}
			break;
		case Zenith_TerrainBrushTool::SplatPaint:
		{
			Zenith_TerrainComponent* pxTerrain = ResolveTarget(xEditor);
			for (u_int u = 0; u < 4; u++)
			{
				char szLabel[80];
				const Zenith_MaterialAsset* pxMaterial = pxTerrain ? pxTerrain->GetMaterial(u) : nullptr;
				snprintf(szLabel, sizeof(szLabel), "%u: %s", u,
					pxMaterial ? pxMaterial->GetName().c_str() : "(unset)");
				if (ImGui::RadioButton(szLabel, xBrush.m_uSplatLayer == u))
				{
					xBrush.m_uSplatLayer = u;
				}
			}
			break;
		}
		case Zenith_TerrainBrushTool::GrassDensity:
			ImGui::SliderFloat("Density", &xBrush.m_fGrassDensity, 0.0f, 1.0f);
			break;
		case Zenith_TerrainBrushTool::TreePaint:
		{
			int iTrees = static_cast<int>(xBrush.m_uTreesPerDab);
			if (ImGui::SliderInt("Trees / Dab", &iTrees, 1, 12))
			{
				xBrush.m_uTreesPerDab = static_cast<u_int>(iTrees);
			}
			ImGui::SliderFloat("Scale Min", &xBrush.m_fTreeScaleMin, 0.4f, xBrush.m_fTreeScaleMax);
			ImGui::SliderFloat("Scale Max", &xBrush.m_fTreeScaleMax, xBrush.m_fTreeScaleMin, 2.5f);
			ImGui::SliderFloat("Spacing (m)", &xBrush.m_fTreeSpacing, 1.0f, 16.0f);
			ImGui::SliderFloat("Max Slope (deg)", &xBrush.m_fTreeMaxSlopeDeg, 5.0f, 70.0f);
			ImGui::TextDisabled("Drag to plant; SHIFT-drag to erase. Not undoable.");
			break;
		}
		default:
			ImGui::TextDisabled("Shift inverts Raise/Lower; [ ] resize the brush");
			break;
		}
	}

	void RenderProceduralSection(Zenith_TerrainEditor& xEditor)
	{
		if (!ImGui::CollapsingHeader("Procedural Generation"))
		{
			return;
		}
		Zenith_TerrainProceduralParams& xParams = xEditor.m_xProceduralParams;
		int iSeed = static_cast<int>(xParams.m_uSeed);
		if (ImGui::InputInt("Seed", &iSeed))
		{
			xParams.m_uSeed = static_cast<u_int>(std::max(0, iSeed));
		}
		ImGui::SliderFloat("Base Height", &xParams.m_fBaseHeight, 0.0f, 1.0f);
		ImGui::SliderFloat("Amplitude", &xParams.m_fAmplitude, 0.0f, 0.5f);
		ImGui::SliderFloat("Frequency", &xParams.m_fFrequency, 0.0001f, 0.01f, "%.5f", ImGuiSliderFlags_Logarithmic);
		int iOctaves = static_cast<int>(xParams.m_uOctaves);
		if (ImGui::SliderInt("Octaves", &iOctaves, 1, 10))
		{
			xParams.m_uOctaves = static_cast<u_int>(iOctaves);
		}
		ImGui::SliderFloat("Lacunarity", &xParams.m_fLacunarity, 1.5f, 3.0f);
		ImGui::SliderFloat("Gain", &xParams.m_fGain, 0.2f, 0.8f);
		ImGui::SliderFloat("Ridged Blend", &xParams.m_fRidgedBlend, 0.0f, 1.0f);

		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Replaces the whole heightfield (clears undo)");
		if (ImGui::Button("Generate", ImVec2(160, 0)))
		{
			xEditor.GenerateProcedural(xParams);
		}
	}

	void RenderErosionSection(Zenith_TerrainEditor& xEditor)
	{
		if (!ImGui::CollapsingHeader("Erosion"))
		{
			return;
		}
		Zenith_TerrainErosionParams& xParams = xEditor.m_xErosionParams;
		int iDroplets = static_cast<int>(xParams.m_uHydraulicDroplets);
		if (ImGui::InputInt("Hydraulic Droplets", &iDroplets, 10000, 100000))
		{
			xParams.m_uHydraulicDroplets = static_cast<u_int>(std::max(0, iDroplets));
		}
		int iThermal = static_cast<int>(xParams.m_uThermalIterations);
		if (ImGui::SliderInt("Thermal Iterations", &iThermal, 0, 8))
		{
			xParams.m_uThermalIterations = static_cast<u_int>(iThermal);
		}
		ImGui::SliderFloat("Talus Angle (deg)", &xParams.m_fTalusAngleDeg, 20.0f, 60.0f);
		int iSeed = static_cast<int>(xParams.m_uSeed);
		if (ImGui::InputInt("Seed##Erosion", &iSeed))
		{
			xParams.m_uSeed = static_cast<u_int>(std::max(0, iSeed));
		}

		if (xEditor.IsErosionRunning())
		{
			ImGui::ProgressBar(xEditor.GetErosionProgress(), ImVec2(-1, 0), "Eroding...");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Erosion clears undo history");
			if (ImGui::Button("Erode Whole Terrain", ImVec2(180, 0)))
			{
				xParams.m_bRegionOnly = false;
				xEditor.RunErosion(xParams, false);
			}
			ImGui::SameLine();
			if (ImGui::Button("Erode At Cursor", ImVec2(160, 0)) && xEditor.HasCursor())
			{
				xParams.m_bRegionOnly = true;
				xParams.m_fRegionCentreX = xEditor.GetCursorPos().x;
				xParams.m_fRegionCentreZ = xEditor.GetCursorPos().z;
				xParams.m_fRegionRadius = std::max(64.0f, xEditor.m_xBrush.m_fRadius * 4.0f);
				xEditor.RunErosion(xParams, false);
			}
		}
	}

	void RenderAutoSplatSection(Zenith_TerrainEditor& xEditor)
	{
		if (!ImGui::CollapsingHeader("Auto-Splat (slope/height rules)"))
		{
			return;
		}

		Zenith_TerrainComponent* pxTerrain = ResolveTarget(xEditor);
		for (u_int uSlot = 0; uSlot < 4; uSlot++)
		{
			Zenith_TerrainAutoSplatRule xRule = xEditor.GetAutoSplatRule(uSlot);
			ImGui::PushID(static_cast<int>(uSlot));

			char szLabel[96];
			const Zenith_MaterialAsset* pxMaterial = pxTerrain ? pxTerrain->GetMaterial(uSlot) : nullptr;
			snprintf(szLabel, sizeof(szLabel), "Layer %u: %s", uSlot,
				pxMaterial ? pxMaterial->GetName().c_str() : "(unset)");

			bool bChanged = ImGui::Checkbox(szLabel, &xRule.m_bEnabled);
			if (xRule.m_bEnabled)
			{
				ImGui::Indent();
				float afHeight[2] = { xRule.m_fHeightMin, xRule.m_fHeightMax };
				if (ImGui::DragFloat2("Height (m)", afHeight, 1.0f, 0.0f, Zenith_TerrainEditor::fTERRAIN_MAX_HEIGHT))
				{
					xRule.m_fHeightMin = afHeight[0];
					xRule.m_fHeightMax = afHeight[1];
					bChanged = true;
				}
				float afSlope[2] = { xRule.m_fSlopeMinDeg, xRule.m_fSlopeMaxDeg };
				if (ImGui::DragFloat2("Slope (deg)", afSlope, 0.5f, 0.0f, 90.0f))
				{
					xRule.m_fSlopeMinDeg = afSlope[0];
					xRule.m_fSlopeMaxDeg = afSlope[1];
					bChanged = true;
				}
				bChanged |= ImGui::SliderFloat("Weight", &xRule.m_fWeight, 0.0f, 4.0f);
				bChanged |= ImGui::SliderFloat("Jitter", &xRule.m_fNoiseJitter, 0.0f, 1.0f);
				ImGui::Unindent();
			}
			if (bChanged)
			{
				xEditor.SetAutoSplatRule(uSlot, xRule);
			}
			ImGui::PopID();
		}

		if (ImGui::Button("Run Auto-Splat", ImVec2(160, 0)))
		{
			xEditor.RunAutoSplat();
		}
	}

	void RenderPersistenceSection(Zenith_TerrainEditor& xEditor)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Bake Target");
		RefreshAssetSetDraftForSession(xEditor);
		ImGui::InputText("Terrain Set Draft", s_szAssetSetDraft, sizeof(s_szAssetSetDraft));
		if (ImGui::Button("Apply Staged Target"))
		{
			if (xEditor.SetAssetSet(s_szAssetSetDraft))
			{
				s_strObservedStagedSet = xEditor.GetAssetSet();
			}
		}
		ImGui::Text("Staged Set: %s", xEditor.GetAssetSet().empty() ? "(legacy)" : xEditor.GetAssetSet().c_str());
		ImGui::TextDisabled("The live component changes only when Full Bake begins regeneration.");
		if (!xEditor.GetAssetSetValidationError().empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "%s",
				xEditor.GetAssetSetValidationError().c_str());
		}
		if (xEditor.GetAssetSet().empty())
		{
			ImGui::TextDisabled("Empty uses the legacy shared terrain paths");
		}
		const std::string strMeshOutputDir = xEditor.GetMeshAssetDirectory();
		const std::string strTextureOutputDir = xEditor.GetTextureAssetDirectory();
		ImGui::TextWrapped("Mesh Output: %s", strMeshOutputDir.c_str());
		ImGui::TextWrapped("Texture Output: %s", strTextureOutputDir.c_str());

		if (xEditor.HasUnbakedChanges())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Unbaked changes (distant LOD + physics are stale until bake)");
		}

		if (ImGui::Button("Save Textures", ImVec2(140, 28)))
		{
			xEditor.SaveTextures();
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake Terrain", ImVec2(140, 28)))
		{
			xEditor.BakeFull();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Saves the textures, re-exports every chunk mesh,\nreloads physics and rebuilds render state.\nTakes a few minutes.");
		}

		if (!xEditor.m_strStatus.empty())
		{
			ImGui::TextWrapped("%s", xEditor.m_strStatus.c_str());
		}
	}
}

void Zenith_EditorPanelTerrainEditor::Render(Zenith_TerrainEditor& xEditor, bool& bShowPanel)
{
	if (!bShowPanel)
	{
		return;
	}

	if (!ImGui::Begin(szEDITOR_WINDOW_TERRAIN_EDITOR, &bShowPanel))
	{
		ImGui::End();
		return;
	}
	RefreshAssetSetDraftForSession(xEditor);

	if (!xEditor.IsActive() || xEditor.IsStandalone())
	{
		ImGui::TextWrapped("No terrain editing session. Select an entity with a Terrain component and click 'Open Terrain Editor' in the Properties panel.");
		ImGui::End();
		return;
	}

	bool bEditMode = xEditor.IsEditModeEnabled();
	if (ImGui::Checkbox("Edit Mode (viewport brush)", &bEditMode))
	{
		xEditor.SetEditModeEnabled(bEditMode);
	}
	ImGui::SameLine();
	if (ImGui::Button("Close Session"))
	{
		xEditor.Close();
		RefreshAssetSetDraftForSession(xEditor);
		ImGui::End();
		return;
	}

	RenderBrushSection(xEditor);
	RenderProceduralSection(xEditor);
	RenderErosionSection(xEditor);
	RenderAutoSplatSection(xEditor);
	RenderPersistenceSection(xEditor);

	ImGui::End();
}

#endif // ZENITH_TOOLS
