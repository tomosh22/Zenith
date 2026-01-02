#pragma once

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "UI/Zenith_UI.h"
#include <string>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

/**
 * Zenith_UIComponent - Component for attaching UI to entities
 *
 * This component allows entities to own and manage UI elements.
 * ScriptBehaviour classes can access this component to manipulate
 * the UI during gameplay.
 *
 * Usage from ScriptBehaviour:
 *
 *   void MyBehaviour::OnCreate() {
 *       auto& ui = m_xParentEntity.GetComponent<Zenith_UIComponent>();
 *       auto* pxHealthBar = ui.CreateRect("HealthBar");
 *       pxHealthBar->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
 *       pxHealthBar->SetPosition(20, -20);
 *       pxHealthBar->SetSize(200, 30);
 *       pxHealthBar->SetColor({1, 0, 0, 1});
 *   }
 *
 *   void MyBehaviour::OnUpdate(float fDt) {
 *       auto& ui = m_xParentEntity.GetComponent<Zenith_UIComponent>();
 *       auto* pxHealthBar = ui.FindElement<Zenith_UI::Zenith_UIRect>("HealthBar");
 *       if (pxHealthBar) {
 *           pxHealthBar->SetFillAmount(m_fHealth / m_fMaxHealth);
 *       }
 *   }
 */

class Zenith_UIComponent
{
public:
    Zenith_UIComponent() = delete;
    Zenith_UIComponent(Zenith_Entity& xParentEntity);
    ~Zenith_UIComponent();

    // Prevent copying (UICanvas is non-copyable)
    Zenith_UIComponent(const Zenith_UIComponent&) = delete;
    Zenith_UIComponent& operator=(const Zenith_UIComponent&) = delete;

    // Allow moving (for component pool swap-and-pop)
    Zenith_UIComponent(Zenith_UIComponent&& xOther);
    Zenith_UIComponent& operator=(Zenith_UIComponent&& xOther);

    // ========== Element Creation ==========

    Zenith_UI::Zenith_UIText* CreateText(const std::string& strName, const std::string& strText = "");
    Zenith_UI::Zenith_UIRect* CreateRect(const std::string& strName);
    Zenith_UI::Zenith_UIImage* CreateImage(const std::string& strName);
    Zenith_UI::Zenith_UIElement* CreateElement(const std::string& strName);

    // Add an existing element (canvas takes ownership)
    void AddElement(Zenith_UI::Zenith_UIElement* pxElement);

    // ========== Element Access ==========

    Zenith_UI::Zenith_UIElement* FindElement(const std::string& strName);

    template<typename T>
    T* FindElement(const std::string& strName)
    {
        Zenith_UI::Zenith_UIElement* pxElement = FindElement(strName);
        // static_cast used instead of dynamic_cast because RTTI is disabled (/GR-)
        // Caller is responsible for ensuring type is correct (use GetType() to verify)
        return static_cast<T*>(pxElement);
    }

    void RemoveElement(const std::string& strName);
    void ClearElements();

    // ========== Canvas Access ==========

    Zenith_UI::Zenith_UICanvas& GetCanvas() { return m_xCanvas; }
    const Zenith_UI::Zenith_UICanvas& GetCanvas() const { return m_xCanvas; }

    // ========== Visibility ==========

    void SetVisible(bool bVisible) { m_bVisible = bVisible; }
    bool IsVisible() const { return m_bVisible; }

    // ========== Frame Updates ==========

    void Update(float fDt);
    void Render();

    // ========== Serialization ==========

    void WriteToDataStream(Zenith_DataStream& xStream) const;
    void ReadFromDataStream(Zenith_DataStream& xStream);

    // ========== Editor UI ==========

#ifdef ZENITH_TOOLS
    void RenderPropertiesPanel();

private:
    void RenderElementTree(Zenith_UI::Zenith_UIElement* pxElement, int iDepth);
    Zenith_UI::Zenith_UIElement* m_pxSelectedElement = nullptr;

public:
#endif

private:
    Zenith_Entity m_xParentEntity;
    Zenith_UI::Zenith_UICanvas m_xCanvas;
    bool m_bVisible = true;
};
