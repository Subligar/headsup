#pragma once

#include "Ashita.h"
#include "rml_render.h"
#include "rml_system.h"

#include <RmlUi/Core.h>

#include <bitset>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    constexpr int kKeyDownBit = 0x8000; // GetKeyState and GetAsyncKeyState: the key is down

    // RmlUi inside the plugin: its interfaces, the bundled fonts, and two contexts: the bar's, drawn into the back
    // buffer where the bar is, and the overlay's (popups, the menu, the debugger), drawn over ImGui.
    class RmlHost
    {
    public:
        // False when RmlUi could not start; warn gets its warnings and errors from then on.
        bool Start(IDirect3DDevice8* device, int width, int height, std::function<void(const std::string&)> warn,
            RmlRender::ImageSource images);
        void Stop();
        void SetLivePicture(std::string name, int width, int height, RmlRender::LiveSource source)
        {
            m_Render.SetLivePicture(std::move(name), width, height, std::move(source));
        }
        bool Running() const { return m_Overlay != nullptr; }
        // The back buffer's size, when it changes.
        void Resize(int width, int height);
        Rml::Context* Bar() const { return m_Bar; }
        Rml::Context* Overlay() const { return m_Overlay; }

        // Makes a font family known to RmlUi: the bundled ones are from the start, an installed one is read from
        // Windows by its name. False for a family Windows does not have.
        bool EnsureFont(const std::string& family);
        // The same for a font in a file, known by the name given.
        bool EnsureFontFile(const std::string& name, const std::string& path);
        // A text's width and a font's measures as RmlUi lays them out, in pixels; nothing for an unknown family.
        float TextWidth(const std::string& text, const std::string& family, int pixels);
        Rml::FontMetrics Metrics(const std::string& family, int pixels);

        // A mouse message in back-buffer pixels, to the overlay: true when an element of it is under the pointer or
        // being dragged, so neither the game nor the bar gets the message.
        bool Mouse(uint32_t message, float x, float y, int wheelDelta);
        // Whether the overlay's element under the pointer is in a document loaded from that source.
        bool HoverIn(const char* source) const;
        // A key to the overlay, typed into its focused text field as well: true when a text field has the focus,
        // so the game must not get the key.
        bool Key(uint32_t virtualKey, bool down);
        bool WantsText() const;
        // Whether the game must not read the keyboard: a text field has it, or a key pressed into one is still down.
        bool HoldsKeys();

        // Each updates its context and draws it; the caller saves and restores the game's states around them.
        void RenderBar();
        void RenderOverlay();
        void SetDebugger(bool visible);
        bool DebuggerVisible() const;
        std::string TakeFailure() { return m_Render.TakeFailure(); }
        void ReleaseLayers() { m_Render.ReleaseLayers(); }
        void RefuseLayers() { m_Render.RefuseLayers(); }

    private:
        void Render(Rml::Context* context);
        void Type(uint32_t virtualKey);
        bool Press(int button, int modifiers);

        std::optional<RmlSystem> m_System;
        RmlRender m_Render;
        std::unordered_map<std::string, bool> m_Fonts;  // by lower-case family: whether RmlUi has it
        std::vector<std::vector<Rml::byte>> m_FontData; // installed fonts' files, which RmlUi reads until it shuts down
        Rml::Context* m_Bar     = nullptr;
        Rml::Context* m_Overlay = nullptr;
        int m_Width = 0, m_Height = 0;
        // Keys that went down into a text field and are not up yet, by virtual key: Enter blurs its box as it goes down.
        std::bitset<256> m_Swallowed;
    };
}
