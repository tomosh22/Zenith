#include "Zenith.h"
#include "Core/Zenith_PropertySystem.h"
#include "DataStream/Zenith_DataStream.h"

#include <algorithm>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace
{
	// Current WriteProperties/ReadProperties blob version.
	//   v1 = name-matched entries, per-entry payload length framing.
	constexpr u_int uPROPERTY_BLOB_VERSION = 1;
}

//------------------------------------------------------------------------------
// Zenith_PropertyValue
//------------------------------------------------------------------------------

bool Zenith_PropertyValue::Equals(const Zenith_PropertyValue& xOther) const
{
	if (m_eType != xOther.m_eType)
	{
		return false;
	}
	switch (m_eType)
	{
	case PROPERTY_TYPE_FLOAT:     return m_fFloat == xOther.m_fFloat;
	case PROPERTY_TYPE_INT32:     return m_iInt32 == xOther.m_iInt32;
	case PROPERTY_TYPE_UINT32:    return m_uUInt32 == xOther.m_uUInt32;
	case PROPERTY_TYPE_BOOL:      return m_bBool == xOther.m_bBool;
	case PROPERTY_TYPE_VECTOR2:   return m_afVector[0] == xOther.m_afVector[0] && m_afVector[1] == xOther.m_afVector[1];
	case PROPERTY_TYPE_VECTOR3:   return m_afVector[0] == xOther.m_afVector[0] && m_afVector[1] == xOther.m_afVector[1] && m_afVector[2] == xOther.m_afVector[2];
	case PROPERTY_TYPE_VECTOR4:   return m_afVector[0] == xOther.m_afVector[0] && m_afVector[1] == xOther.m_afVector[1] && m_afVector[2] == xOther.m_afVector[2] && m_afVector[3] == xOther.m_afVector[3];
	case PROPERTY_TYPE_STRING:    return m_strString == xOther.m_strString;
	case PROPERTY_TYPE_ENTITY_ID: return m_ulPacked == xOther.m_ulPacked;
	case PROPERTY_TYPE_GUID:      return m_ulPacked == xOther.m_ulPacked;
	default:
		Zenith_Assert(false, "Zenith_PropertyValue::Equals: unhandled type %u", m_eType);
		return false;
	}
}

void Zenith_PropertyValue::WritePayloadToDataStream(Zenith_DataStream& xStream) const
{
	switch (m_eType)
	{
	case PROPERTY_TYPE_FLOAT:     xStream << m_fFloat; break;
	case PROPERTY_TYPE_INT32:     xStream << m_iInt32; break;
	case PROPERTY_TYPE_UINT32:    xStream << m_uUInt32; break;
	case PROPERTY_TYPE_BOOL:      xStream << m_bBool; break;
	case PROPERTY_TYPE_VECTOR2:   xStream << m_afVector[0]; xStream << m_afVector[1]; break;
	case PROPERTY_TYPE_VECTOR3:   xStream << m_afVector[0]; xStream << m_afVector[1]; xStream << m_afVector[2]; break;
	case PROPERTY_TYPE_VECTOR4:   xStream << m_afVector[0]; xStream << m_afVector[1]; xStream << m_afVector[2]; xStream << m_afVector[3]; break;
	case PROPERTY_TYPE_STRING:    xStream << m_strString; break;
	case PROPERTY_TYPE_ENTITY_ID: xStream << m_ulPacked; break;
	case PROPERTY_TYPE_GUID:      xStream << m_ulPacked; break;
	default:
		Zenith_Assert(false, "Zenith_PropertyValue::WritePayloadToDataStream: unhandled type %u", m_eType);
		break;
	}
}

void Zenith_PropertyValue::ReadPayloadFromDataStream(Zenith_DataStream& xStream, Zenith_PropertyType eType)
{
	m_eType = eType;
	switch (eType)
	{
	case PROPERTY_TYPE_FLOAT:     xStream >> m_fFloat; break;
	case PROPERTY_TYPE_INT32:     xStream >> m_iInt32; break;
	case PROPERTY_TYPE_UINT32:    xStream >> m_uUInt32; break;
	case PROPERTY_TYPE_BOOL:      xStream >> m_bBool; break;
	case PROPERTY_TYPE_VECTOR2:   xStream >> m_afVector[0]; xStream >> m_afVector[1]; break;
	case PROPERTY_TYPE_VECTOR3:   xStream >> m_afVector[0]; xStream >> m_afVector[1]; xStream >> m_afVector[2]; break;
	case PROPERTY_TYPE_VECTOR4:   xStream >> m_afVector[0]; xStream >> m_afVector[1]; xStream >> m_afVector[2]; xStream >> m_afVector[3]; break;
	case PROPERTY_TYPE_STRING:    xStream >> m_strString; break;
	case PROPERTY_TYPE_ENTITY_ID: xStream >> m_ulPacked; break;
	case PROPERTY_TYPE_GUID:      xStream >> m_ulPacked; break;
	default:
		Zenith_Assert(false, "Zenith_PropertyValue::ReadPayloadFromDataStream: unhandled type %u", eType);
		break;
	}
}

void Zenith_PropertyValue::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const u_int8 uType = static_cast<u_int8>(m_eType);
	xStream << uType;
	WritePayloadToDataStream(xStream);
}

void Zenith_PropertyValue::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int8 uType = 0;
	xStream >> uType;
	if (uType >= PROPERTY_TYPE_COUNT)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Zenith_PropertyValue: corrupt type tag %u", uType);
		return;
	}
	ReadPayloadFromDataStream(xStream, static_cast<Zenith_PropertyType>(uType));
}

//------------------------------------------------------------------------------
// Zenith_PropertySystem
//------------------------------------------------------------------------------

bool Zenith_PropertySystem::SetPropertyValue(void* pxInstance, const Zenith_ReflectedProperty& xProperty, const Zenith_PropertyValue& xValue)
{
	Zenith_Assert(pxInstance != nullptr, "SetPropertyValue: null instance");
	Zenith_Assert(xProperty.m_pfnGet != nullptr && xProperty.m_pfnSet != nullptr, "SetPropertyValue: property '%s' has null accessors", xProperty.m_szName);

	if (xValue.GetType() != xProperty.m_eType)
	{
		Zenith_Error(LOG_CATEGORY_CORE,
			"Zenith_PropertySystem: type mismatch setting '%s' (property type %u, value type %u); value dropped",
			xProperty.m_szName ? xProperty.m_szName : "(null)", xProperty.m_eType, xValue.GetType());
		return false;
	}

	// Clamp ranged scalars. The clamp lives here (the single central set path)
	// so the serializer, the editor panel, and tuning re-apply all agree.
	Zenith_PropertyValue xClamped = xValue;
	if (xProperty.m_bHasRange)
	{
		switch (xProperty.m_eType)
		{
		case PROPERTY_TYPE_FLOAT:
			xClamped.SetFloat(std::clamp(xValue.GetFloat(), xProperty.m_fMin, xProperty.m_fMax));
			break;
		case PROPERTY_TYPE_INT32:
			xClamped.SetInt32(std::clamp(xValue.GetInt32(), static_cast<int32_t>(xProperty.m_fMin), static_cast<int32_t>(xProperty.m_fMax)));
			break;
		case PROPERTY_TYPE_UINT32:
			xClamped.SetUInt32(std::clamp(xValue.GetUInt32(), static_cast<u_int>(std::max(xProperty.m_fMin, 0.0f)), static_cast<u_int>(std::max(xProperty.m_fMax, 0.0f))));
			break;
		default:
			break;	// ranges are meaningless for the other types; ignore
		}
	}

	Zenith_PropertyValue xCurrent;
	xProperty.m_pfnGet(pxInstance, xCurrent);
	if (xCurrent.Equals(xClamped))
	{
		return false;
	}

	xProperty.m_pfnSet(pxInstance, xClamped);
	return true;
}

void Zenith_PropertySystem::WriteProperties(const void* pxInstance, const Zenith_PropertyTable& xTable, Zenith_DataStream& xStream)
{
	xStream << uPROPERTY_BLOB_VERSION;

	// Reserve space for the blob byte count, backpatched after the entries so
	// readers of an unsupported version can skip the whole blob.
	const uint64_t ulBlobSizeFieldCursor = xStream.GetCursor();
	u_int uBlobPlaceholder = 0;
	xStream << uBlobPlaceholder;
	const uint64_t ulBlobStartCursor = xStream.GetCursor();

	const u_int uCount = xTable.GetPropertyCount();
	xStream << uCount;

	for (u_int u = 0; u < uCount; ++u)
	{
		const Zenith_ReflectedProperty& xProperty = xTable.GetPropertyAt(u);

		std::string strName(xProperty.m_szName ? xProperty.m_szName : "");
		xStream << strName;
		const u_int8 uType = static_cast<u_int8>(xProperty.m_eType);
		xStream << uType;

		// Per-entry payload length framing - lets the reader skip an entry it
		// doesn't recognise without misaligning the rest of the blob.
		const uint64_t ulPayloadSizeFieldCursor = xStream.GetCursor();
		u_int uPayloadPlaceholder = 0;
		xStream << uPayloadPlaceholder;
		const uint64_t ulPayloadStartCursor = xStream.GetCursor();

		Zenith_PropertyValue xValue;
		xProperty.m_pfnGet(pxInstance, xValue);
		xValue.WritePayloadToDataStream(xStream);

		const uint64_t ulPayloadEndCursor = xStream.GetCursor();
		const u_int uPayloadBytes = static_cast<u_int>(ulPayloadEndCursor - ulPayloadStartCursor);
		xStream.SetCursor(ulPayloadSizeFieldCursor);
		xStream << uPayloadBytes;
		xStream.SetCursor(ulPayloadEndCursor);
	}

	const uint64_t ulBlobEndCursor = xStream.GetCursor();
	const u_int uBlobBytes = static_cast<u_int>(ulBlobEndCursor - ulBlobStartCursor);
	xStream.SetCursor(ulBlobSizeFieldCursor);
	xStream << uBlobBytes;
	xStream.SetCursor(ulBlobEndCursor);
}

void Zenith_PropertySystem::ReadProperties(void* pxInstance, const Zenith_PropertyTable& xTable, Zenith_DataStream& xStream,
	PropertyChangedFn pfnOnChanged, void* pxUserData)
{
	u_int uVersion = 0;
	xStream >> uVersion;
	u_int uBlobBytes = 0;
	xStream >> uBlobBytes;

	const uint64_t ulBlobStart = xStream.GetCursor();
	const uint64_t ulBlobEnd = ulBlobStart + uBlobBytes;

	if (uVersion != uPROPERTY_BLOB_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"Zenith_PropertySystem: unsupported property blob version %u (expected %u); skipping %u bytes",
			uVersion, uPROPERTY_BLOB_VERSION, uBlobBytes);
		xStream.SkipBytes(uBlobBytes);
		return;
	}

	u_int uCount = 0;
	xStream >> uCount;

	for (u_int u = 0; u < uCount; ++u)
	{
		std::string strName;
		xStream >> strName;
		u_int8 uType = 0;
		xStream >> uType;
		u_int uPayloadBytes = 0;
		xStream >> uPayloadBytes;
		const uint64_t ulPayloadEnd = xStream.GetCursor() + uPayloadBytes;

		const Zenith_ReflectedProperty* pxProperty = xTable.FindProperty(strName.c_str());
		if (!pxProperty || static_cast<u_int8>(pxProperty->m_eType) != uType || uType >= PROPERTY_TYPE_COUNT)
		{
			// Unknown name (removed/renamed field) or type changed - drop the
			// stored value rather than reinterpret it. Never corrupt.
			Zenith_Log(LOG_CATEGORY_CORE,
				"Zenith_PropertySystem: dropping stored property '%s' (type %u): %s",
				strName.c_str(), uType, pxProperty ? "declared type differs" : "no longer declared");
			xStream.SkipBytes(uPayloadBytes);
			continue;
		}

		Zenith_PropertyValue xValue;
		xValue.ReadPayloadFromDataStream(xStream, pxProperty->m_eType);

		// Defensive: payload framing exists precisely so a malformed payload
		// cannot misalign the entries that follow it.
		if (xStream.GetCursor() != ulPayloadEnd)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"Zenith_PropertySystem: property '%s' payload read %llu bytes, expected %u; clamping cursor",
				strName.c_str(),
				static_cast<unsigned long long>(xStream.GetCursor() - (ulPayloadEnd - uPayloadBytes)),
				uPayloadBytes);
			xStream.SetCursor(ulPayloadEnd);
			continue;
		}

		if (SetPropertyValue(pxInstance, *pxProperty, xValue) && pfnOnChanged)
		{
			pfnOnChanged(pxUserData, pxProperty->m_szName);
		}
	}

	// Final guard: land exactly on the documented blob end.
	if (xStream.GetCursor() != ulBlobEnd)
	{
		Zenith_Error(LOG_CATEGORY_CORE,
			"Zenith_PropertySystem: blob read cursor %llu doesn't match expected end %llu; clamping",
			static_cast<unsigned long long>(xStream.GetCursor()),
			static_cast<unsigned long long>(ulBlobEnd));
		xStream.SetCursor(ulBlobEnd);
	}
}

const char* Zenith_PropertySystem::GetDisplayName(const char* szName)
{
	if (!szName || szName[0] == '\0')
	{
		return szName;
	}

	const char* szCursor = szName;

	// Strip the scope prefix: m_, s_, g_, ls_.
	if (szCursor[0] == 'm' && szCursor[1] == '_')       szCursor += 2;
	else if (szCursor[0] == 's' && szCursor[1] == '_')  szCursor += 2;
	else if (szCursor[0] == 'g' && szCursor[1] == '_')  szCursor += 2;
	else if (szCursor[0] == 'l' && szCursor[1] == 's' && szCursor[2] == '_') szCursor += 3;

	// Skip the lowercase type-prefix run (f, u, b, x, str, ax, ...) up to the
	// first uppercase character.
	const char* szFirstUpper = szCursor;
	while (*szFirstUpper != '\0' && !(*szFirstUpper >= 'A' && *szFirstUpper <= 'Z'))
	{
		++szFirstUpper;
	}

	if (*szFirstUpper != '\0')
	{
		return szFirstUpper;
	}
	// No uppercase found - fall back to whatever survived the scope strip,
	// or the full name if that would be empty.
	return (*szCursor != '\0') ? szCursor : szName;
}

//------------------------------------------------------------------------------
// Editor panel
//------------------------------------------------------------------------------

#ifdef ZENITH_TOOLS

namespace
{
	// Renders one property's widget into xNewValue. Returns true when the user
	// edited the value this frame.
	bool RenderPropertyWidget(const Zenith_ReflectedProperty& xProperty, const Zenith_PropertyValue& xCurrent, Zenith_PropertyValue& xNewValue)
	{
		const char* szLabel = Zenith_PropertySystem::GetDisplayName(xProperty.m_szName);
		bool bEdited = false;

		switch (xProperty.m_eType)
		{
	case PROPERTY_TYPE_FLOAT:
	{
		float fValue = xCurrent.GetFloat();
		bEdited = xProperty.m_bHasRange
			? ImGui::SliderFloat(szLabel, &fValue, xProperty.m_fMin, xProperty.m_fMax)
			: ImGui::DragFloat(szLabel, &fValue, 0.01f);
		xNewValue.SetFloat(fValue);
		break;
	}
	case PROPERTY_TYPE_INT32:
	{
		int32_t iValue = xCurrent.GetInt32();
		bEdited = xProperty.m_bHasRange
			? ImGui::SliderInt(szLabel, &iValue, static_cast<int32_t>(xProperty.m_fMin), static_cast<int32_t>(xProperty.m_fMax))
			: ImGui::DragInt(szLabel, &iValue);
		xNewValue.SetInt32(iValue);
		break;
	}
	case PROPERTY_TYPE_UINT32:
	{
		u_int uValue = xCurrent.GetUInt32();
		const u_int uMin = static_cast<u_int>(std::max(xProperty.m_fMin, 0.0f));
		const u_int uMax = static_cast<u_int>(std::max(xProperty.m_fMax, 0.0f));
		bEdited = xProperty.m_bHasRange
			? ImGui::SliderScalar(szLabel, ImGuiDataType_U32, &uValue, &uMin, &uMax)
			: ImGui::DragScalar(szLabel, ImGuiDataType_U32, &uValue);
		xNewValue.SetUInt32(uValue);
		break;
	}
	case PROPERTY_TYPE_BOOL:
	{
		bool bValue = xCurrent.GetBool();
		bEdited = ImGui::Checkbox(szLabel, &bValue);
		xNewValue.SetBool(bValue);
		break;
	}
	case PROPERTY_TYPE_VECTOR2:
	{
		Zenith_Maths::Vector2 xValue = xCurrent.GetVector2();
		bEdited = ImGui::DragFloat2(szLabel, &xValue.x, 0.01f);
		xNewValue.SetVector2(xValue);
		break;
	}
	case PROPERTY_TYPE_VECTOR3:
	{
		Zenith_Maths::Vector3 xValue = xCurrent.GetVector3();
		bEdited = (xProperty.m_uFlags & PROPERTY_FLAG_COLOUR)
			? ImGui::ColorEdit3(szLabel, &xValue.x)
			: ImGui::DragFloat3(szLabel, &xValue.x, 0.01f);
		xNewValue.SetVector3(xValue);
		break;
	}
	case PROPERTY_TYPE_VECTOR4:
	{
		Zenith_Maths::Vector4 xValue = xCurrent.GetVector4();
		bEdited = (xProperty.m_uFlags & PROPERTY_FLAG_COLOUR)
			? ImGui::ColorEdit4(szLabel, &xValue.x)
			: ImGui::DragFloat4(szLabel, &xValue.x, 0.01f);
		xNewValue.SetVector4(xValue);
		break;
	}
	case PROPERTY_TYPE_STRING:
	{
		char acBuffer[256];
		const std::string& strValue = xCurrent.GetString();
		const size_t uCopyLen = std::min(strValue.length(), sizeof(acBuffer) - 1);
		std::memcpy(acBuffer, strValue.c_str(), uCopyLen);
		acBuffer[uCopyLen] = '\0';
		bEdited = ImGui::InputText(szLabel, acBuffer, sizeof(acBuffer));
		xNewValue.SetString(std::string(acBuffer));
		break;
	}
	case PROPERTY_TYPE_ENTITY_ID:
	{
		ImGui::Text("%s: entity 0x%llX", szLabel, static_cast<unsigned long long>(xCurrent.GetPackedEntityID()));
		xNewValue = xCurrent;
		break;
	}
	case PROPERTY_TYPE_GUID:
	{
		ImGui::Text("%s: guid 0x%llX", szLabel, static_cast<unsigned long long>(xCurrent.GetGUID().m_uGUID));
		xNewValue = xCurrent;
		break;
	}
	default:
		ImGui::TextDisabled("%s: <unsupported type %u>", szLabel, xProperty.m_eType);
		xNewValue = xCurrent;
		break;
		}

		return bEdited;
	}
}

bool Zenith_PropertySystem::RenderPropertyPanel(void* pxInstance, const Zenith_PropertyTable& xTable,
	PropertyChangedFn pfnOnChanged, void* pxUserData,
	PropertyRowRectFn pfnRowRect, void* pxRowRectUserData)
{
	bool bAnyChanged = false;

	const u_int uCount = xTable.GetPropertyCount();
	for (u_int u = 0; u < uCount; ++u)
	{
		const Zenith_ReflectedProperty& xProperty = xTable.GetPropertyAt(u);
		ImGui::PushID(xProperty.m_szName);

		Zenith_PropertyValue xCurrent;
		xProperty.m_pfnGet(pxInstance, xCurrent);

		if (xProperty.m_uFlags & PROPERTY_FLAG_READ_ONLY)
		{
			Zenith_PropertyValue xUnused;
			ImGui::BeginDisabled();
			RenderPropertyWidget(xProperty, xCurrent, xUnused);
			ImGui::EndDisabled();
			if (pfnRowRect)
			{
				const ImVec2 xMin = ImGui::GetItemRectMin();
				const ImVec2 xMax = ImGui::GetItemRectMax();
				pfnRowRect(pxRowRectUserData, xProperty.m_szName, xMin.x, xMin.y, xMax.x, xMax.y);
			}
			ImGui::PopID();
			continue;
		}

		Zenith_PropertyValue xNewValue;
		const bool bEdited = RenderPropertyWidget(xProperty, xCurrent, xNewValue);
		if (pfnRowRect)
		{
			const ImVec2 xMin = ImGui::GetItemRectMin();
			const ImVec2 xMax = ImGui::GetItemRectMax();
			pfnRowRect(pxRowRectUserData, xProperty.m_szName, xMin.x, xMin.y, xMax.x, xMax.y);
		}
		if (bEdited)
		{
			if (SetPropertyValue(pxInstance, xProperty, xNewValue))
			{
				bAnyChanged = true;
				if (pfnOnChanged)
				{
					pfnOnChanged(pxUserData, xProperty.m_szName);
				}
			}
		}

		ImGui::PopID();
	}

	return bAnyChanged;
}

#endif // ZENITH_TOOLS

#include "Core/Zenith_PropertySystem.Tests.inl"
