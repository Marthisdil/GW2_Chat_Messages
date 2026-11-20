#pragma once

// The macro library window: folder tree (left) and details of the selected item (right).
// Owns the in-memory library and saves it (debounced) after every change.
// All functions run on the render thread.
namespace Ui
{
    constexpr const char* kMainWindowName = "Chat Messages";

    void  Init();     // loads the library; call in AddonLoad before registering render callbacks
    void  Shutdown(); // saves pending changes; call in AddonUnload after deregistering renders

    void  Render();        // RT_Render
    void  RenderOptions(); // RT_OptionsRender (Nexus' addon options page)

    void  ToggleMainWindow();
    bool* MainWindowOpenFlag(); // for GUI_RegisterCloseOnEscape
}
