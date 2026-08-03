// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Ui/EditorUi.hpp
 * @brief The toolkit-independent UI surface every panel is written against.
 *
 * No panel, command or plugin may call Dear ImGui -- or Qt, or anything else -- directly. They
 * call this interface, and exactly one implementation binds it to a real toolkit (ANALYSIS.md
 * decision D-02). The cost is one indirection per widget, which is irrelevant at editor frame
 * rates. The benefit is threefold:
 *
 * 1. the toolkit choice stays reversible after the editor has real panels in it;
 * 2. the whole panel layer is unit-testable against NullEditorUi, with no window and no GPU;
 * 3. a plugin compiled against this header keeps working across a toolkit change.
 *
 * The interface is deliberately immediate-mode, because that is the cheaper direction to adapt:
 * wrapping a retained toolkit (Qt) in an immediate-mode facade is routine, while the reverse
 * requires inventing state the immediate-mode toolkit never had.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "CNA/Editor/Core/EditorMath.hpp"
#include "CNA/Editor/Core/PropertyValue.hpp"
#include "CNA/Editor/Core/Uuid.hpp"
#include "CNA/Editor/Ui/UiDrawData.hpp"

namespace CNA::Editor
{
    /** @brief Severity of a console message, used for filtering and colouring. */
    enum class LogSeverity
    {
        Trace,
        Info,
        Warning,
        Error
    };

    /** @brief Returns the short display name of @p severity, e.g. "warn". */
    const char* toString(LogSeverity severity);

    /** @brief Where a panel is docked by default. The implementation may ignore this. */
    enum class DockSide
    {
        Left,
        Right,
        Bottom,
        Center
    };

    /** @brief A rectangle in panel-local or screen coordinates, in pixels. */
    struct UiRegion
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        [[nodiscard]] bool isEmpty() const { return width <= 0.0f || height <= 0.0f; }
    };

    /**
     * @brief Everything the viewport panel needs to know about one frame of pointer interaction.
     *
     * Returned as one struct from a single call rather than as a scatter of queries, because the
     * viewport needs all of it together and each piece is only meaningful relative to the others:
     * a wheel delta matters only while hovered, a drag delta only while dragging, and a click only
     * when it lands on the image rather than on the panel around it.
     */
    struct UiImageInteraction
    {
        bool hovered = false;

        /** @brief Set on the frame a left click completes over the image, without a drag. */
        bool clicked = false;

        /** @brief Set on the frame the left button goes down over the image. */
        bool leftPressed = false;

        /**
         * @brief True while the left button is held after a press that began over the image.
         *
         * Deliberately independent of @c hovered: a gizmo drag that wanders off the panel must
         * keep going, and must end only when the button is actually released. Tying it to hover
         * would drop the entity wherever the cursor happened to cross the panel edge.
         */
        bool leftDown = false;

        /** @brief Set on the frame such a press is released, wherever the cursor then is. */
        bool leftReleased = false;

        /** @brief Cursor position relative to the image's top-left, in pixels. */
        float localMouseX = 0.0f;
        float localMouseY = 0.0f;

        /** @brief Wheel movement this frame, in notches. Non-zero only while hovered. */
        float wheel = 0.0f;

        /** @brief True while a pan drag is in progress. */
        bool dragging = false;

        /** @brief Cursor movement since the previous frame, while dragging. */
        float dragDeltaX = 0.0f;
        float dragDeltaY = 0.0f;
    };

    /**
     * @brief The immediate-mode UI surface.
     *
     * Every method that draws a widget returns whether the user interacted with it during this
     * frame, so a panel reads as ordinary control flow. Editing widgets take their value by
     * reference and report a change, which is what lets a panel turn an edit into an
     * EditorCommand at exactly the right moment.
     */
    class EditorUi
    {
    public:
        virtual ~EditorUi() = default;

        /** @brief Returns a short name for the bound toolkit, e.g. "imgui" or "null". */
        [[nodiscard]] virtual const char* getBackendName() const = 0;

        /** @brief Returns false once the user has asked to close the editor. */
        [[nodiscard]] virtual bool isRunning() const = 0;

        /** @brief Begins a frame. Returns false when the application should exit. */
        virtual bool beginFrame() = 0;

        /** @brief Ends and presents the frame. */
        virtual void endFrame() = 0;

        /** @brief Establishes the dock space the panels attach to. */
        virtual void beginDockSpace() = 0;
        virtual void endDockSpace() = 0;

        /**
         * @brief Begins a dockable panel window.
         * @return False when the panel is collapsed or hidden; the caller must then skip its
         *         contents but must still call endPanel().
         */
        virtual bool beginPanel(const std::string& title, DockSide preferredSide) = 0;
        virtual void endPanel() = 0;

        /** @brief Draws a non-interactive line of text. */
        virtual void text(const std::string& value) = 0;

        /** @brief Draws a button; returns true on the frame it is clicked. */
        virtual bool button(const std::string& label) = 0;

        /** @brief Draws a checkbox; returns true when @p value changed this frame. */
        virtual bool checkbox(const std::string& label, bool& value) = 0;

        /**
         * @brief Draws the right editing widget for @p value, chosen from its PropertyType.
         *
         * This is the one call the inspector makes per property, which is what keeps the inspector
         * generic over component types it was never compiled against.
         *
         * @param label Field label.
         * @param value Edited in place.
         * @param enumOptions Allowed names when the value is an enum; ignored otherwise.
         * @param readOnly When true the widget is shown but cannot be changed.
         * @return True when the user changed the value this frame.
         */
        virtual bool propertyField(const std::string& label,
                                   PropertyValue& value,
                                   const std::vector<std::string>& enumOptions = {},
                                   bool readOnly = false) = 0;

        /**
         * @brief Draws one node of a tree.
         * @param id Stable identity for the node, so expansion state survives reordering.
         * @param label Displayed text.
         * @param selected Whether the node is currently selected.
         * @param leaf When true the node draws no expander.
         * @param[out] outClicked Set when the user clicked the node this frame.
         * @return True when the node is expanded and its children should be drawn.
         */
        virtual bool treeNode(const Uuid& id,
                              const std::string& label,
                              bool selected,
                              bool leaf,
                              bool& outClicked) = 0;
        virtual void treePop() = 0;

        /** @brief Begins a menu in the main menu bar. */
        virtual bool beginMenu(const std::string& label) = 0;
        virtual void endMenu() = 0;

        /**
         * @brief Draws a menu item.
         * @param shortcut Display-only accelerator text, e.g. "Ctrl+Z".
         * @param enabled When false the item is greyed out and cannot be chosen.
         * @return True on the frame it is chosen.
         */
        virtual bool menuItem(const std::string& label,
                              const std::string& shortcut = {},
                              bool enabled = true) = 0;

        /**
         * @brief Returns the space left in the current panel, in panel-local pixels.
         *
         * The viewport asks for this so it can render its scene at exactly the panel's size --
         * rendering at a fixed size and stretching would make the grid non-square and picking
         * disagree with what is on screen.
         */
        [[nodiscard]] virtual UiRegion getContentRegion() const { return UiRegion{}; }

        /**
         * @brief Draws @p texture filling @p width by @p height, and reports interaction with it.
         *
         * @param id Stable identity for the widget.
         * @param texture Renderer-side texture id, from the scene renderer.
         */
        /**
         * @param flipVertically Sample the texture bottom-up. Needed for render targets on
         *        backends whose textures originate at the bottom-left; see
         *        EditorViewport::isRenderTextureFlippedVertically().
         */
        virtual UiImageInteraction image(const std::string& id,
                                         UiTextureId texture,
                                         float width,
                                         float height,
                                         bool flipVertically = false)
        {
            (void)id;
            (void)texture;
            (void)width;
            (void)height;
            (void)flipVertically;
            return UiImageInteraction{};
        }

        /** @brief Draws a horizontal rule. */
        virtual void separator() = 0;

        /** @brief Places the next widget on the same line as the previous one. */
        virtual void sameLine() = 0;

        /** @brief Appends a message to the console panel. */
        virtual void log(LogSeverity severity, const std::string& message) = 0;

        /**
         * @brief Draws the accumulated console messages.
         *
         * Called by the console panel. The messages live in the implementation because the scroll
         * position, severity colouring and auto-scroll behaviour are all toolkit state -- the
         * panel decides only *that* a console exists.
         */
        virtual void drawLogView() {}
    };

    /**
     * @brief An EditorUi that draws nothing and interacts with nobody.
     *
     * This is not a placeholder for a missing implementation -- it is a first-class one. It makes
     * the whole panel layer testable in CI with no display, and it is what `cna-editor --headless`
     * runs on, which is how the editor's own scene and asset operations get exercised on a build
     * machine that has no GPU.
     *
     * Every widget reports "no interaction", every panel reports "visible", and log messages are
     * collected in memory where a test can assert on them.
     */
    class NullEditorUi : public EditorUi
    {
    public:
        /** @brief One captured log message. */
        struct LogEntry
        {
            LogSeverity severity = LogSeverity::Info;
            std::string message;
        };

        [[nodiscard]] const char* getBackendName() const override { return "null"; }
        [[nodiscard]] bool isRunning() const override { return running_; }

        bool beginFrame() override;
        void endFrame() override;

        void beginDockSpace() override {}
        void endDockSpace() override {}

        bool beginPanel(const std::string& title, DockSide preferredSide) override;
        void endPanel() override {}

        void text(const std::string& value) override { (void)value; }
        bool button(const std::string& label) override { (void)label; return false; }
        bool checkbox(const std::string& label, bool& value) override { (void)label; (void)value; return false; }

        bool propertyField(const std::string& label,
                           PropertyValue& value,
                           const std::vector<std::string>& enumOptions = {},
                           bool readOnly = false) override;

        bool treeNode(const Uuid& id,
                      const std::string& label,
                      bool selected,
                      bool leaf,
                      bool& outClicked) override;
        void treePop() override {}

        bool beginMenu(const std::string& label) override { (void)label; return false; }
        void endMenu() override {}
        bool menuItem(const std::string& label, const std::string& shortcut = {}, bool enabled = true) override;

        void separator() override {}
        void sameLine() override {}

        /**
         * @brief Reports a fixed 1280x720 content region.
         *
         * Non-zero on purpose: a headless run should exercise the viewport's real sizing and
         * rendering path, and a zero region would make every panel silently skip its content --
         * turning the headless smoke test into a no-op exactly where it is most useful.
         */
        [[nodiscard]] UiRegion getContentRegion() const override { return UiRegion{0.0f, 0.0f, 1280.0f, 720.0f}; }

        void log(LogSeverity severity, const std::string& message) override;

        /** @brief Asks the next beginFrame() to report exit. Used by `--frames N` and by tests. */
        void requestExit() { running_ = false; }

        /** @brief Returns every message logged so far. */
        [[nodiscard]] const std::vector<LogEntry>& getLog() const { return log_; }

        /** @brief Returns the number of frames begun. */
        [[nodiscard]] std::uint64_t getFrameCount() const { return frameCount_; }

        /** @brief Returns the titles of the panels drawn during the most recent frame. */
        [[nodiscard]] const std::vector<std::string>& getLastFramePanels() const { return lastFramePanels_; }

    private:
        bool running_ = true;
        std::uint64_t frameCount_ = 0;
        std::vector<LogEntry> log_;
        std::vector<std::string> currentFramePanels_;
        std::vector<std::string> lastFramePanels_;
    };
}
