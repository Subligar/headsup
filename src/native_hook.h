#pragma once

#include "code_patch.h"
#include "combat_popup.h"
#include "fish_bar.h"
#include "friend_list.h"
#include "name_frame.h"
#include "native_locate.h"
#include "plate_settings.h"
#include "strip_text.h"
#include "weather.h"
#include "window_views.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

struct IDirect3DDevice8;
struct IDirect3DSurface8;

namespace headsup
{
    constexpr const char* kGameModule = "FFXiMain.dll";
    constexpr const char* kAshitaModule = "Ashita.dll";
    // The folder a loaded module was loaded from, without its last separator; empty when it is not loaded.
    std::string ModuleFolder(const char* module);
    // The plugin's CodeWriter.
    bool WriteCode(uint8_t* at, const uint8_t* bytes, size_t count);

    // An entity whose names the hook may meet this frame, and what becomes of them: kept, hidden or replaced.
    struct NameOwner
    {
        uint32_t actor;
        uint16_t index;
        HookAction action;
    };

    struct NameStats
    {
        uint32_t names    = 0; // tracked entities' names the game was about to draw
        uint32_t replaced = 0; // of those, drawn by HeadsUp instead
        uint32_t hidden   = 0; // of those, drawn by no one
    };

    // Sends every name the game is about to draw through HeadsUp first: the six bytes at the name routine's hook
    // (native_locate.h) become a jump to a gate that calls OnName with the routine's frame, then either runs the bytes
    // it replaced and resumes the routine, or skips to the routine's exit so the game draws nothing.
    class NameHook
    {
    public:
        void SetDevice(IDirect3DDevice8* device);
        ~NameHook() { Stop(); }
        // Only at a render boundary, where the game cannot be inside the routine. Returns why it could not start, or an
        // empty string.
        std::string Start();
        // Puts the game's bytes back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return !m_Patches.empty(); }

#ifdef HEADSUP_DEV
        uint32_t ModuleBase() const { return m_ModuleBase; }
        std::span<const uint32_t> CallerReturns() const;
#endif

        // At the end of Present, once everything has read the frame's names: forget them, and say whose the next
        // frame's are.
        void NewFrame(std::vector<NameOwner> owners);
        // From now to the next NewFrame every name the game draws goes, whatever its owner's action.
        void HideAll();
        NameStats Stats() const;
        // The frame of the name the game drew for this entity this frame, or nullptr.
        const NameFrame* Drawn(uint16_t index) const;
        // The image and depth buffer this frame's names went to, from the first one; nullptr before any.
        IDirect3DSurface8* SceneTarget() const;
        IDirect3DSurface8* SceneDepth() const;

    private:
        std::vector<CodePatch> m_Patches; // the jump into the gate, then the calls into the entity wrapper
#ifdef HEADSUP_DEV
        uint32_t m_ModuleBase = 0;
#endif
    };

    // Sends the game's two target arrows through HeadsUp: the calls that draw them (native_locate.h) go to a gate that
    // notes the arrow's color, then lets the game draw it or skips it while HeadsUp draws its own cursor instead.
    class ArrowHook
    {
    public:
        ~ArrowHook() { Stop(); }
        // Only at a render boundary, where the game cannot be inside the target window's draw. Returns why it could
        // not start, or an empty string.
        std::string Start();
        // Puts the game's calls back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return m_Calls[0].at != nullptr; }

        // Whether the game's arrows are skipped from now on.
        void Hide(bool hide);
        // The color of the arrow the game last drew over a candidate being picked, or 0 since forgotten.
        uint32_t PickedColor() const;
        void ForgetPicked();
        // The color the game would give that arrow now, asked of its own routine: the draw that asks is skipped while
        // HudBeGone hides the target window. 0 when the routine was not found or there is nobody to ask about.
        uint32_t CandidateColor(const void* window, uintptr_t yourActor, uintptr_t candidateActor) const;

    private:
        std::array<CodePatch, 2> m_Calls; // the target's and the candidate's
    };

    // Sends the game's calls that draw one character's model (native_locate.h) through a gate that notes the actor
    // until the draw returns, so every draw in between is that character's.
    class ModelHook
    {
    public:
        ~ModelHook() { Stop(); }
        // Only at a render boundary, where the game cannot be inside a model draw. Returns why it could not start, or
        // an empty string.
        std::string Start();
        // Puts the game's calls back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return !m_Calls.empty(); }
        // At Present: no model is being drawn, whatever a draw the game left early kept noted.
        void NewFrame();
        // The actor whose model the game is drawing now, or 0 between models.
        uintptr_t Drawing() const;

    private:
        std::vector<CodePatch> m_Calls;
    };

    // Render flags 0's bit 9, which the game tests before it starts a popup over a character (0x18C940).
    constexpr uint32_t kDrawnRenderFlag = 0x200;

    // Takes the game's combat popups while asked: the call that draws a popup's text (native_locate.h) goes to a gate
    // that opens a popup, lets the game draw it and closes it. Every quad the draw sends in between is the popup's,
    // which the plugin's draw hook adds here and drops. Not taken, the gate passes the call straight on. Three more
    // calls, all in or none, tie each popup to its message (PopupTies) as the game starts it, and three after them
    // start it as soon as its action is parsed (EarlyPopups).
    class CombatTextHook
    {
    public:
        void SetDevice(IDirect3DDevice8* device);
        ~CombatTextHook() { Stop(); }
        // Only at a render boundary, where the game cannot be inside a popup's draw. Returns why it could not start, or
        // an empty string.
        std::string Start();
        // Puts the game's call back if it is still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return m_Call.at != nullptr; }
        // Whether popups are taken from now on: never while the gate is not in.
        void Take(bool take);
        // At Present: the last frame's popups are forgotten, and their image unless HeadsUp still draws popups into it,
        // and the popups the game starts next count as started now.
        void NewFrame(double now, bool keepImage);
        // Before the device's Reset, which a kept image would make fail.
        static void ForgetImage();
        // At Present: how long HeadsUp shows a popup, which its line is kept for.
        static void SetLife(double seconds);
        // At Present, for the popups the game starts next: your entity index and your pet's (0 for none).
        static void SetYou(uint32_t you, uint32_t pet);
        // At Present: whether popups start as soon as their action arrives and spread when started together.
        static void SetRightAway(bool on);
        static void SetSides(const PopupSides& sides);
        // At Present: whether buffs and debuffs pop, and the server ids of you, your party and your alliance.
        static void SetStatuses(bool pop, std::span<const uint32_t> party);
        // An action packet as the server sent it, for the game's parse of it this frame.
        static void Sent(const SentAction& action);
        // Whether StartPopup can start one: the ties are in and the game's popup effects were found.
        bool CanStartPopups() const;
        // On the game's thread: starts the game's popup effect of event's kind over the character whose actor pointer
        // is over, with value, in the color the game gives message between colorActor and colorTarget (entity
        // indexes), tied to event as if the game had decided on it. Returns why it could not, or nothing.
        std::string StartPopup(const PopupEvent& event, uint32_t value, uint32_t colorActor, uint32_t colorTarget, uintptr_t over);
        // Inside a taken popup's draw, whose quads go to Add.
        static bool Open();
        static void Add(const void* vertices, size_t count, size_t stride);
        const std::vector<CombatPopup>& Popups() const;
        // The image and depth buffer the frame's first popup was drawn into, and the image's size; none before one.
        IDirect3DSurface8* Target() const;
        IDirect3DSurface8* Depth() const;
        uint32_t TargetWidth() const;
        uint32_t TargetHeight() const;
        // Why the calls that tie each popup to its message are not in, so a miss shows as Miss and every popup in the
        // game's color; empty when they are.
        const std::string& TieProblem() const { return m_TieProblem; }
        // Why the calls that start each popup as soon as its action arrives are not in, so popups wait for their hit;
        // empty when they are.
        const std::string& EarlyProblem() const { return m_EarlyProblem; }
#ifdef HEADSUP_DEV
        // /hu testpopup, at Present: asks the game's decision for message as if you did it to yourself and starts what
        // it picks over your character (yourActor, Ashita's actor pointer) with value. Returns what happened.
        std::string TestPopup(uint32_t message, uint32_t value, uint32_t you, uint32_t yourId, uintptr_t yourActor);
#endif

    private:
        CodePatch m_Call;
        std::array<CodePatch, 3> m_Ties{}, m_Early{};
        std::string m_TieProblem, m_EarlyProblem;
    };

    // How the game's windows are placed this frame: the panel's band, and the settings for the help's mode.
    struct WindowPlacing
    {
        HelpBand band;
        int placedUnder   = 0;
        bool shift        = false; // Shift Vanilla UI
        bool moveGameHelp = false; // the game's strip goes right under the panel, and the windows under it
    };

    // Skips the game's help bar and connection readout while asked: the draw loop's call to the menu window draw
    // (native_locate.h) goes to HeadsUp, which passes every window but those hidden on to the game.
    class WindowHook
    {
    public:
        ~WindowHook() { Stop(); }
        // Only at a render boundary. Returns why it could not start, or an empty string.
        std::string Start();
        // Puts the game's call back if it is still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return m_Call.at != nullptr; }
        // Which of the game's windows are skipped from now on.
        void Hide(HiddenWindows hidden);
        // The windows, by short name, whose draws are dropped from now on (Base Game UI's Hide).
        void HideWindows(const std::vector<std::string>& names);
        // While the game draws one of those, every draw call it makes is one to drop.
        static bool Silent();
        // While hide is set the game's fish bar is dropped: the party window's draws above its own rectangle.
        void HideFishBar(bool hide);
        // Inside the party window's draw while it is, its rectangle in the game's menu pixels.
        static std::optional<WindowRect> KeptWindow();
        // Where those drawn in the last whole frame are, in the game's menu pixels: clicks there are kept from it.
        static std::span<const WindowRect> HiddenRects();
        // Once a frame, before the game draws its windows: Stop puts back those that drew in the last two.
        void NewFrame();
        // While placing is set, the game's windows are placed as they draw: at the first, the frame's windows are read
        // from the game's own list and its view picked (window_views.h); each that hangs from the top then moves by
        // the view's shift and its own offset, the strip by its own rule, and every window that opens is noted.
        void Place(std::optional<WindowPlacing> placing);
        // The player's shifts for the views of the help's mode now.
        void Shift(const std::vector<ViewShift>& shifts);
        // The places the player put windows at: such a window goes there, and no view's shift moves it.
        void Pin(const std::vector<WindowPlace>& places);
        // Where each window seen since HeadsUp loaded last stood, by short name.
        std::vector<WindowPlace> Places() const;
        // The line the windows went under in the last frame placed, in the game's menu pixels; 0 before one.
        int WindowsBand() const;
        // The windows that drew since the last call, by short name, sorted: those open now.
        std::vector<std::string> TakeOpen();
        // The windows noted since HeadsUp loaded, by short name, in the order they came.
        std::vector<std::string> SeenWindows() const;
        size_t SeenCount() const;
#ifdef HEADSUP_DEV
        // Each window in the game's list now: its short name, record, rectangle, owner, the owner's class and layout
        // (RVAs) with where HeadsUp reads it keeps its window's place and an entry's corner, and the layout's first
        // bytes, to find what an owner keeps that a move leaves behind.
        static std::string OwnersReport();
#endif
        // Why windows cannot be moved, once, after a Start that could not find the game's move.
        std::string TakeMoveProblem() { return std::exchange(m_MoveProblem, std::string()); }
        // Why the Compass row cannot hide the compass, once, after a Start that could not find its routine.
        std::string TakeCompassProblem() { return std::exchange(m_CompassProblem, std::string()); }

    private:
        CodePatch m_Call;
        CodePatch m_LabelCall; // Wide Scan's grid label's ask for the strip title's place, when it was found
        CodePatch m_CountCall; // an item list's count's ask for it, when it was found
        CodePatch m_MoveStart; // MCD::MoveTo's start, so a window the game moves goes where HeadsUp would put it
        CodePatch m_BuildCall; // the catalog's window build, so where a new window goes is known before it draws
        CodePatch m_Compass;   // the compass routine's answer while the Compass row hides it
        std::string m_MoveProblem, m_CompassProblem;
        void PutCompassBack();
    };

    // Puts right the game's guess of its window's client area (native_locate.h's PointerMapping): each guess's
    // GetWindowRect call and GetSystemMetrics load go to HeadsUp, which answers with the client area and no borders
    // while the fix is on, and as Windows would while it is off. It also brings back the pointer the game hid for
    // typing when the mouse moves over an Ashita window, which keeps those moves from the game. And it scales the mouse Ashita hands ImGui from
    // client pixels to ImGui's (ImGuiMouseCalls), which differ once the window is resized away from the game's size.
    class PointerFix
    {
    public:
        ~PointerFix() { Stop(); }
        // Only at a render boundary. Returns why it could not start, or an empty string.
        std::string Start();
        // Puts the game's bytes back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return !m_Patches.empty(); }
        void Enable(bool on);
        // On a mouse move an Ashita window took: shows the game's pointer through its own routine if it is hidden.
        static void RevealUnderWindow();
        // Keys that set off a game command without hiding its pointer (native_locate.h's KeyHide); none keeps the game's
        // behavior.
        void KeepPointerFor(const std::vector<uint8_t>& keys);
        // Each frame: ImGui's display size and the game window's client size.
        void ImGuiSize(float width, float height, int clientWidth, int clientHeight);
        // Why Ashita's ImGui mouse was not fixed though Ashita.dll is loaded, or empty.
        const std::string& ImGuiProblem() const { return m_ImGuiProblem; }

    private:
        std::vector<CodePatch> m_Patches;
        std::string m_ImGuiProblem;
    };

#ifdef HEADSUP_DEV
    // One strip window's text object as read, or why not.
    struct StripRead
    {
        uint32_t owner = 0;
        uint32_t at    = 0;          // the text object's address, once read
        std::vector<uint8_t> object; // kStripObjectBytes, empty when it could not be read
        std::string why;             // empty when read
    };
#endif

    // The game's top-strip text (native_locate.h's LocateStripText), read on the game's thread at Present.
    class StripTextReader
    {
    public:
        // Finds the slots and offsets: "" when found, else why not.
        std::string Find();
#ifdef HEADSUP_DEV
        // Each check fails closed: no slot, a 0 owner (the title screen, character select, a teardown), an owner
        // whose window record does not name the window, or memory that cannot be read. Every check, every call.
        StripRead Read(StripWindow window) const;
#endif
        // Each frame: the window's text object into into, false when hidden. Memory is checked only where the chain
        // to it changed (StripWindowReader).
        bool Read(StripWindow window, StripObject& into);

    private:
        StripSites m_Sites;
        std::array<StripWindowReader, 2> m_Readers; // by StripWindow
    };

    // The search server's address and port as the client keeps them since login (LocateSearchAddress), found once.
    class SearchAddressReader
    {
    public:
        // "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded or before the client has an address.
        std::optional<std::pair<uint32_t, uint16_t>> Read() const;

    private:
        uint32_t m_Address = 0, m_Port = 0;
    };

    // The Window Type the game is set to, by the client's own config getter, found once.
    class WindowTypeReader
    {
    public:
        // "" when found, else why not.
        std::string Find();
        // 0 to 7; nothing before Find succeeded or when the game answers no type.
        std::optional<int> Read() const;
        // Whether the game's Damage Display shows popups; taken as shown before Find succeeded.
        bool DamagePopupsShown() const;

    private:
        int32_t(__cdecl* m_Get)(int32_t) = nullptr;
    };

    // The map square you stand in, by the client's own <pos> routine (LocatePosText), found once.
    class MapSquareReader
    {
    public:
        // "" when found, else why not.
        std::string Find();
        // "(H-8)", or "(?-?)" where the zone has no map; "" before Find succeeded or while the zone's map is not loaded.
        std::string Read() const;

    private:
        const char*(__cdecl* m_PosText)(uint8_t) = nullptr;
        uint32_t m_ZoneMap = 0;
    };

    // The zone's weather as the client keeps it, found once.
    class WeatherReader
    {
    public:
        // "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded or when what is there is no weather's id.
        std::optional<Weather> Read() const;

    private:
        uint32_t m_Global = 0;
    };

    // The friends of the game's own friend list (friend_list.h), read where the client keeps it.
    class FriendListReader
    {
    public:
        // The list's first slot; null while it is not found. It is looked for on the first calls, as the client
        // fills it once you are signed on.
        const uint8_t* Slots();

    private:
        const uint8_t* m_First = nullptr;
        int m_Searches         = 0;
    };

    // The game's PlayOnline message counts (LocatePlayOnline), read on the game's thread.
    class PlayOnlineReader
    {
    public:
        // Finds where the client keeps them: "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded, while the client has no session, or when they cannot be read.
        std::optional<PlayOnlineCounts> Read() const;

    private:
        uint32_t m_Global = 0;
    };

    // A hooked fish's stamina (LocateFish), read on the game's thread.
    class FishReader
    {
    public:
        // Finds where the client keeps it: "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded or while no fish is on the line.
        std::optional<FishStamina> Read() const;

    private:
        uint32_t m_Global = 0;
    };

    // The client's connection counters (LocateNetStats), read on the game's thread.
    class NetStatsReader
    {
    public:
        // Finds where the client keeps them: "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded, while the client has no session (the title screen), or when what is there
        // cannot be read or cannot be the counters.
        std::optional<NetCounters> Read() const;

    private:
        uint32_t m_Global = 0;
    };
}
