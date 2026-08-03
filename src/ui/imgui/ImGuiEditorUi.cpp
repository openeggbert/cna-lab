// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Ui/ImGuiEditorUi.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "imgui_internal.h"

namespace CNA::Editor
{
    namespace
    {
        /** @brief Maps the editor's key enumeration onto ImGui's. */
        ImGuiKey toImGuiKey(UiKey key)
        {
            switch (key)
            {
                case UiKey::Tab: return ImGuiKey_Tab;
                case UiKey::LeftArrow: return ImGuiKey_LeftArrow;
                case UiKey::RightArrow: return ImGuiKey_RightArrow;
                case UiKey::UpArrow: return ImGuiKey_UpArrow;
                case UiKey::DownArrow: return ImGuiKey_DownArrow;
                case UiKey::PageUp: return ImGuiKey_PageUp;
                case UiKey::PageDown: return ImGuiKey_PageDown;
                case UiKey::Home: return ImGuiKey_Home;
                case UiKey::End: return ImGuiKey_End;
                case UiKey::Insert: return ImGuiKey_Insert;
                case UiKey::Delete: return ImGuiKey_Delete;
                case UiKey::Backspace: return ImGuiKey_Backspace;
                case UiKey::Space: return ImGuiKey_Space;
                case UiKey::Enter: return ImGuiKey_Enter;
                case UiKey::Escape: return ImGuiKey_Escape;
                case UiKey::A: return ImGuiKey_A;
                case UiKey::C: return ImGuiKey_C;
                case UiKey::V: return ImGuiKey_V;
                case UiKey::X: return ImGuiKey_X;
                case UiKey::Y: return ImGuiKey_Y;
                case UiKey::Z: return ImGuiKey_Z;
                case UiKey::D: return ImGuiKey_D;
                case UiKey::F: return ImGuiKey_F;
                case UiKey::N: return ImGuiKey_N;
                case UiKey::S: return ImGuiKey_S;
                case UiKey::W: return ImGuiKey_W;
                case UiKey::E: return ImGuiKey_E;
                case UiKey::R: return ImGuiKey_R;
                case UiKey::F1: return ImGuiKey_F1;
                case UiKey::F2: return ImGuiKey_F2;
                case UiKey::F5: return ImGuiKey_F5;
                case UiKey::None:
                case UiKey::Count: break;
            }
            return ImGuiKey_None;
        }

        /** @brief Returns a stable per-node ImGui id derived from a Uuid. */
        ImGuiID toImGuiId(const Uuid& id)
        {
            // ImGui ids are 32-bit; the Uuid's low eight bytes folded down are far more than
            // enough to keep tree expansion state stable across reordering, which is the only
            // thing this id is used for.
            const std::array<std::uint8_t, 16>& bytes = id.getBytes();
            std::uint32_t hash = 2166136261u;
            for (const std::uint8_t byte : bytes)
            {
                hash ^= byte;
                hash *= 16777619u;
            }
            return static_cast<ImGuiID>(hash);
        }

        ImVec4 toImGuiColor(LogSeverity severity)
        {
            switch (severity)
            {
                case LogSeverity::Trace: return ImVec4{0.55f, 0.55f, 0.55f, 1.0f};
                case LogSeverity::Info: return ImVec4{0.85f, 0.85f, 0.85f, 1.0f};
                case LogSeverity::Warning: return ImVec4{0.95f, 0.75f, 0.30f, 1.0f};
                case LogSeverity::Error: return ImVec4{0.95f, 0.35f, 0.35f, 1.0f};
            }
            return ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
        }

        UiClipboardHooks& clipboardHooks()
        {
            static UiClipboardHooks hooks;
            return hooks;
        }

        std::string& clipboardScratch()
        {
            static std::string scratch;
            return scratch;
        }

        const char* clipboardGetAdapter(ImGuiContext*)
        {
            if (clipboardHooks().getText == nullptr) { return ""; }
            clipboardScratch() = clipboardHooks().getText();
            return clipboardScratch().c_str();
        }

        void clipboardSetAdapter(ImGuiContext*, const char* text)
        {
            if (clipboardHooks().setText != nullptr) { clipboardHooks().setText(text != nullptr ? text : ""); }
        }
    }

    struct ImGuiEditorUi::Impl
    {
        ImGuiContext* context = nullptr;
        UiInputState input;
        UiDrawData drawData;
        std::vector<std::pair<LogSeverity, std::string>> log;
        std::string layoutPath;
        /** @brief Source of UiTextureIds. Zero stays reserved as the "no texture" sentinel. */
        UiTextureId nextTextureId = 1;
        bool running = true;
        bool frameActive = false;
        /** @brief Depth of open panels, so endPanel() can pair with a skipped beginPanel(). */
        int panelDepth = 0;

        /** @brief Copies UiInputState into ImGui's IO for this frame. */
        void applyInput()
        {
            ImGuiIO& io = ImGui::GetIO();

            io.DisplaySize = ImVec2{input.displayWidth, input.displayHeight};
            io.DisplayFramebufferScale = ImVec2{input.framebufferScaleX, input.framebufferScaleY};
            // ImGui asserts on a non-positive delta, and a caller that forgets to set one should
            // get a slightly wrong animation rather than an abort.
            io.DeltaTime = input.deltaSeconds > 0.0f ? input.deltaSeconds : 1.0f / 60.0f;

            if (input.mouseInWindow) { io.AddMousePosEvent(input.mouseX, input.mouseY); }
            else { io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); }

            io.AddMouseButtonEvent(0, input.isMouseDown(UiMouseButton::Left));
            io.AddMouseButtonEvent(1, input.isMouseDown(UiMouseButton::Right));
            io.AddMouseButtonEvent(2, input.isMouseDown(UiMouseButton::Middle));

            if (input.wheelX != 0.0f || input.wheelY != 0.0f)
            {
                io.AddMouseWheelEvent(input.wheelX, input.wheelY);
            }

            io.AddKeyEvent(ImGuiMod_Ctrl, input.modifiers.control);
            io.AddKeyEvent(ImGuiMod_Shift, input.modifiers.shift);
            io.AddKeyEvent(ImGuiMod_Alt, input.modifiers.alt);
            io.AddKeyEvent(ImGuiMod_Super, input.modifiers.super);

            for (int index = 1; index < static_cast<int>(UiKey::Count); ++index)
            {
                const ImGuiKey key = toImGuiKey(static_cast<UiKey>(index));
                if (key != ImGuiKey_None) { io.AddKeyEvent(key, input.keyDown[index]); }
            }

            for (const char16_t unit : input.characters) { io.AddInputCharacterUTF16(unit); }
        }

        /** @brief Converts ImGui's frame output into the toolkit-independent UiDrawData. */
        void captureDrawData()
        {
            drawData.clearGeometry();

            const ImDrawData* source = ImGui::GetDrawData();
            if (source == nullptr) { return; }

            drawData.displayX = source->DisplayPos.x;
            drawData.displayY = source->DisplayPos.y;
            drawData.displayWidth = source->DisplaySize.x;
            drawData.displayHeight = source->DisplaySize.y;
            drawData.framebufferScaleX = source->FramebufferScale.x;
            drawData.framebufferScaleY = source->FramebufferScale.y;

            capturePendingTextures(source);

            drawData.lists.reserve(static_cast<std::size_t>(source->CmdListsCount));
            for (int listIndex = 0; listIndex < source->CmdListsCount; ++listIndex)
            {
                const ImDrawList* sourceList = source->CmdLists[listIndex];
                UiDrawList list;

                // The repack from ImDrawVert to UiVertex is the conversion CNA's
                // VertexPositionColorTexture would have required anyway; doing it here means it
                // happens exactly once per vertex per frame.
                list.vertices.reserve(static_cast<std::size_t>(sourceList->VtxBuffer.Size));
                for (int vertexIndex = 0; vertexIndex < sourceList->VtxBuffer.Size; ++vertexIndex)
                {
                    const ImDrawVert& vertex = sourceList->VtxBuffer[vertexIndex];
                    list.vertices.push_back(UiVertex{vertex.pos.x, vertex.pos.y,
                                                     vertex.uv.x, vertex.uv.y,
                                                     static_cast<std::uint32_t>(vertex.col)});
                }

                static_assert(sizeof(ImDrawIdx) == sizeof(std::uint16_t),
                              "cna-editor assumes ImGui's 16-bit index type; see imconfig.h");
                list.indices.assign(sourceList->IdxBuffer.Data,
                                    sourceList->IdxBuffer.Data + sourceList->IdxBuffer.Size);

                list.commands.reserve(static_cast<std::size_t>(sourceList->CmdBuffer.Size));
                for (int commandIndex = 0; commandIndex < sourceList->CmdBuffer.Size; ++commandIndex)
                {
                    const ImDrawCmd& sourceCommand = sourceList->CmdBuffer[commandIndex];
                    if (sourceCommand.UserCallback != nullptr) { continue; }

                    UiDrawCommand command;
                    command.indexOffset = sourceCommand.IdxOffset;
                    command.indexCount = sourceCommand.ElemCount;
                    command.vertexOffset = sourceCommand.VtxOffset;
                    command.clipRect = UiClipRect{sourceCommand.ClipRect.x, sourceCommand.ClipRect.y,
                                                  sourceCommand.ClipRect.z, sourceCommand.ClipRect.w};
                    command.texture = static_cast<UiTextureId>(sourceCommand.GetTexID());
                    list.commands.push_back(command);
                }

                drawData.lists.push_back(std::move(list));
            }
        }

        /**
         * @brief Turns ImGui's texture requests into UiTextureRequests.
         *
         * ImGui 1.92 owns font atlas lifetime and may grow or re-rasterise it mid-session, so the
         * renderer is asked to create, update and destroy textures rather than being handed one
         * atlas at start-up.
         *
         * The id is allocated *here*, not by the renderer. ImGui asserts the moment a draw
         * command references a texture whose id is still unset, and draw commands are read later
         * in this same function -- so a design where the renderer assigns ids and reports them
         * back cannot work without splitting the frame into two phases. Owning the id namespace
         * on this side removes the ordering hazard outright and leaves the renderer with a plain
         * map from UiTextureId to its own texture object.
         */
        void capturePendingTextures(const ImDrawData* source)
        {
            if (source->Textures == nullptr) { return; }

            for (ImTextureData* texture : *source->Textures)
            {
                if (texture == nullptr) { continue; }

                UiTextureRequest request;
                request.width = texture->Width;
                request.height = texture->Height;
                request.pitch = texture->GetPitch();

                switch (texture->Status)
                {
                    case ImTextureStatus_WantCreate:
                        request.action = UiTextureAction::Create;
                        request.texture = nextTextureId++;
                        request.updateX = 0;
                        request.updateY = 0;
                        request.updateWidth = texture->Width;
                        request.updateHeight = texture->Height;
                        request.pixels = static_cast<const std::uint8_t*>(texture->GetPixels());
                        texture->SetTexID(static_cast<ImTextureID>(request.texture));
                        texture->SetStatus(ImTextureStatus_OK);
                        break;

                    case ImTextureStatus_WantUpdates: {
                        // ImGui reports a set of dirty blocks. Their bounding box is one upload
                        // instead of N, and for a font atlas the blocks are almost always
                        // adjacent glyph rows, so the wasted area is negligible.
                        request.action = UiTextureAction::Update;
                        request.texture = static_cast<UiTextureId>(texture->TexID);
                        const ImTextureRect& box = texture->UpdateRect;
                        request.updateX = box.x;
                        request.updateY = box.y;
                        request.updateWidth = box.w;
                        request.updateHeight = box.h;
                        request.pixels = static_cast<const std::uint8_t*>(
                            texture->GetPixelsAt(box.x, box.y));
                        texture->SetStatus(ImTextureStatus_OK);
                        break;
                    }

                    case ImTextureStatus_WantDestroy:
                        request.action = UiTextureAction::Destroy;
                        request.texture = static_cast<UiTextureId>(texture->TexID);
                        request.pixels = nullptr;
                        texture->SetStatus(ImTextureStatus_Destroyed);
                        break;

                    case ImTextureStatus_OK:
                    case ImTextureStatus_Destroyed:
                        continue;
                }

                drawData.textureRequests.push_back(request);
            }
        }
    };

    ImGuiEditorUi::ImGuiEditorUi() : impl_(std::make_unique<Impl>())
    {
        IMGUI_CHECKVERSION();
        impl_->context = ImGui::CreateContext();
        ImGui::SetCurrentContext(impl_->context);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.BackendPlatformName = "cna-editor";
        io.BackendRendererName = "cna-editor-viewport";

        // Declaring both flags is what enables the modern paths this binding relies on:
        // VtxOffset lets one draw list exceed 65535 vertices while keeping 16-bit indices, and
        // RendererHasTextures moves the font atlas onto the incremental request protocol above.
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        // The layout is a property of the user, not of the project being edited, so ImGui's own
        // automatic `imgui.ini` in the working directory is disabled in favour of an explicit
        // path chosen by the application (loadLayout).
        io.IniFilename = nullptr;

        ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
        platformIo.Platform_GetClipboardTextFn = clipboardGetAdapter;
        platformIo.Platform_SetClipboardTextFn = clipboardSetAdapter;

        ImGui::StyleColorsDark();
    }

    ImGuiEditorUi::~ImGuiEditorUi()
    {
        if (impl_->context != nullptr)
        {
            ImGui::SetCurrentContext(impl_->context);
            ImGui::DestroyContext(impl_->context);
            impl_->context = nullptr;
        }
    }

    bool ImGuiEditorUi::isRunning() const { return impl_->running; }

    void ImGuiEditorUi::setInput(const UiInputState& input) { impl_->input = input; }

    const UiDrawData& ImGuiEditorUi::getDrawData() const { return impl_->drawData; }

    void ImGuiEditorUi::setClipboardHooks(const UiClipboardHooks& hooks) { clipboardHooks() = hooks; }

    void ImGuiEditorUi::requestExit() { impl_->running = false; }

    const std::vector<std::pair<LogSeverity, std::string>>& ImGuiEditorUi::getLog() const
    {
        return impl_->log;
    }

    bool ImGuiEditorUi::beginFrame()
    {
        if (!impl_->running) { return false; }
        if (impl_->input.quitRequested)
        {
            impl_->running = false;
            return false;
        }

        ImGui::SetCurrentContext(impl_->context);
        impl_->applyInput();
        ImGui::NewFrame();
        impl_->frameActive = true;
        impl_->panelDepth = 0;
        return true;
    }

    void ImGuiEditorUi::endFrame()
    {
        if (!impl_->frameActive) { return; }

        ImGui::Render();
        impl_->captureDrawData();
        impl_->frameActive = false;
    }

    void ImGuiEditorUi::beginDockSpace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

        // NoDocking on the host itself: without it a panel can be docked *into* the host window
        // rather than into the dock space, which detaches it from the layout entirely.
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                                     | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                     | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
                                     | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;

        ImGui::Begin("##CnaEditorDockHost", nullptr, flags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("CnaEditorDockSpace"), ImVec2{0.0f, 0.0f},
                         ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::BeginMenuBar();
    }

    void ImGuiEditorUi::endDockSpace()
    {
        ImGui::EndMenuBar();
        ImGui::End();
    }

    bool ImGuiEditorUi::beginPanel(const std::string& title, DockSide preferredSide)
    {
        // The side is a first-run hint only. Once the user has moved a panel, the saved layout
        // wins -- an editor that reasserts its own idea of where a panel belongs on every launch
        // is an editor people stop rearranging.
        (void)preferredSide;

        const bool visible = ImGui::Begin(title.c_str());
        ++impl_->panelDepth;
        return visible;
    }

    void ImGuiEditorUi::endPanel()
    {
        if (impl_->panelDepth <= 0) { return; }
        --impl_->panelDepth;
        // ImGui::End() must be called even when Begin() returned false, which is why panels are
        // written as "begin; maybe skip the body; always end".
        ImGui::End();
    }

    void ImGuiEditorUi::text(const std::string& value) { ImGui::TextUnformatted(value.c_str()); }

    bool ImGuiEditorUi::button(const std::string& label) { return ImGui::Button(label.c_str()); }

    bool ImGuiEditorUi::checkbox(const std::string& label, bool& value)
    {
        return ImGui::Checkbox(label.c_str(), &value);
    }

    bool ImGuiEditorUi::propertyField(const std::string& label,
                                      PropertyValue& value,
                                      const std::vector<std::string>& enumOptions,
                                      bool readOnly)
    {
        ImGui::PushID(label.c_str());
        const ImGuiInputTextFlags flags = readOnly ? ImGuiInputTextFlags_ReadOnly : 0;
        bool changed = false;

        switch (value.getType())
        {
            case PropertyType::Boolean: {
                bool held = value.get<bool>();
                if (ImGui::Checkbox(label.c_str(), &held) && !readOnly)
                {
                    value = PropertyValue{held};
                    changed = true;
                }
                break;
            }
            case PropertyType::Integer: {
                int held = static_cast<int>(value.get<std::int64_t>());
                if (ImGui::InputInt(label.c_str(), &held, 1, 10, flags) && !readOnly)
                {
                    value = PropertyValue{static_cast<std::int64_t>(held)};
                    changed = true;
                }
                break;
            }
            case PropertyType::Float: {
                float held = value.get<float>();
                if (ImGui::DragFloat(label.c_str(), &held, 0.1f, 0.0f, 0.0f, "%.3f", flags) && !readOnly)
                {
                    value = PropertyValue{held};
                    changed = true;
                }
                break;
            }
            case PropertyType::String: {
                std::string held = value.get<std::string>();
                // Sized for a name or a short path; longer strings are rare enough in the
                // inspector that a fixed buffer is not worth a heap allocation per frame.
                char buffer[512];
                std::snprintf(buffer, sizeof(buffer), "%s", held.c_str());
                if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer), flags) && !readOnly)
                {
                    value = PropertyValue{std::string{buffer}};
                    changed = true;
                }
                break;
            }
            case PropertyType::Enum: {
                const std::string current = value.get<PropertyValue::EnumValue>().name;
                if (ImGui::BeginCombo(label.c_str(), current.c_str()))
                {
                    for (const std::string& option : enumOptions)
                    {
                        const bool selected = option == current;
                        if (ImGui::Selectable(option.c_str(), selected) && !readOnly)
                        {
                            value = PropertyValue{PropertyValue::EnumValue{option}};
                            changed = true;
                        }
                        if (selected) { ImGui::SetItemDefaultFocus(); }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            case PropertyType::Color: {
                const EditorColor held = value.get<EditorColor>();
                float components[4] = {held.r / 255.0f, held.g / 255.0f, held.b / 255.0f, held.a / 255.0f};
                if (ImGui::ColorEdit4(label.c_str(), components) && !readOnly)
                {
                    value = PropertyValue{EditorColor{
                        static_cast<std::uint8_t>(components[0] * 255.0f + 0.5f),
                        static_cast<std::uint8_t>(components[1] * 255.0f + 0.5f),
                        static_cast<std::uint8_t>(components[2] * 255.0f + 0.5f),
                        static_cast<std::uint8_t>(components[3] * 255.0f + 0.5f)}};
                    changed = true;
                }
                break;
            }
            case PropertyType::Vector2: {
                EditorVector2 held = value.get<EditorVector2>();
                float components[2] = {held.x, held.y};
                if (ImGui::DragFloat2(label.c_str(), components, 0.1f, 0.0f, 0.0f, "%.3f", flags) && !readOnly)
                {
                    value = PropertyValue{EditorVector2{components[0], components[1]}};
                    changed = true;
                }
                break;
            }
            case PropertyType::Vector3: {
                EditorVector3 held = value.get<EditorVector3>();
                float components[3] = {held.x, held.y, held.z};
                if (ImGui::DragFloat3(label.c_str(), components, 0.1f, 0.0f, 0.0f, "%.3f", flags) && !readOnly)
                {
                    value = PropertyValue{EditorVector3{components[0], components[1], components[2]}};
                    changed = true;
                }
                break;
            }
            case PropertyType::Vector4:
            case PropertyType::Quaternion: {
                // Quaternions are edited as four raw components for now. Presenting them as Euler
                // angles needs a stable angle convention and round-trip handling that plan.md
                // ED-113 covers; showing the honest stored value is better than showing angles
                // that silently drift on every edit.
                float components[4] = {};
                if (value.getType() == PropertyType::Vector4)
                {
                    const EditorVector4 held = value.get<EditorVector4>();
                    components[0] = held.x; components[1] = held.y;
                    components[2] = held.z; components[3] = held.w;
                }
                else
                {
                    const EditorQuaternion held = value.get<EditorQuaternion>();
                    components[0] = held.x; components[1] = held.y;
                    components[2] = held.z; components[3] = held.w;
                }
                if (ImGui::DragFloat4(label.c_str(), components, 0.01f, 0.0f, 0.0f, "%.3f", flags) && !readOnly)
                {
                    if (value.getType() == PropertyType::Vector4)
                    {
                        value = PropertyValue{EditorVector4{components[0], components[1],
                                                            components[2], components[3]}};
                    }
                    else
                    {
                        value = PropertyValue{EditorQuaternion{components[0], components[1],
                                                                components[2], components[3]}};
                    }
                    changed = true;
                }
                break;
            }
            case PropertyType::Rectangle: {
                const EditorRectangle held = value.get<EditorRectangle>();
                int components[4] = {held.x, held.y, held.width, held.height};
                if (ImGui::InputInt4(label.c_str(), components, flags) && !readOnly)
                {
                    value = PropertyValue{EditorRectangle{components[0], components[1],
                                                           components[2], components[3]}};
                    changed = true;
                }
                break;
            }
            case PropertyType::AssetReference: {
                const Uuid id = value.get<PropertyValue::AssetReference>().id;
                ImGui::TextUnformatted(label.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(id.isValid() ? id.toString().c_str() : "(none)");
                // Drag-and-drop from the asset browser lands here; plan.md ED-208.
                break;
            }
            case PropertyType::EntityReference: {
                const Uuid id = value.get<PropertyValue::EntityReference>().id;
                ImGui::TextUnformatted(label.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(id.isValid() ? id.toString().c_str() : "(none)");
                break;
            }
            case PropertyType::None:
                ImGui::TextUnformatted(label.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted("<none>");
                break;
        }

        ImGui::PopID();
        return changed;
    }

    bool ImGuiEditorUi::treeNode(const Uuid& id,
                                 const std::string& label,
                                 bool selected,
                                 bool leaf,
                                 bool& outClicked)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected) { flags |= ImGuiTreeNodeFlags_Selected; }
        if (leaf) { flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(toImGuiId(id))),
                                            flags, "%s", label.c_str());
        outClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();

        // A leaf pushed nothing onto the tree stack, so it must not report "expanded" -- the
        // caller would pair it with a treePop() that pops somebody else's node.
        return open && !leaf;
    }

    void ImGuiEditorUi::treePop() { ImGui::TreePop(); }

    bool ImGuiEditorUi::beginMenu(const std::string& label) { return ImGui::BeginMenu(label.c_str()); }

    void ImGuiEditorUi::endMenu() { ImGui::EndMenu(); }

    bool ImGuiEditorUi::menuItem(const std::string& label, const std::string& shortcut, bool enabled)
    {
        return ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), false, enabled);
    }

    void ImGuiEditorUi::drawLogView()
    {
        // Rendered here rather than in the panel layer because the scroll position, the severity
        // colours and the auto-scroll behaviour are all toolkit state; the panel only decides
        // *that* a console exists, not how it scrolls.
        for (const auto& [severity, message] : impl_->log)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, toImGuiColor(severity));
            ImGui::TextUnformatted(message.c_str());
            ImGui::PopStyleColor();
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) { ImGui::SetScrollHereY(1.0f); }
    }

    void ImGuiEditorUi::separator() { ImGui::Separator(); }

    void ImGuiEditorUi::sameLine() { ImGui::SameLine(); }

    void ImGuiEditorUi::log(LogSeverity severity, const std::string& message)
    {
        impl_->log.emplace_back(severity, message);

        // Bounded so that a game logging every frame through the runtime bridge cannot grow the
        // editor's memory without limit over a long session.
        constexpr std::size_t kMaxEntries = 10000;
        if (impl_->log.size() > kMaxEntries)
        {
            impl_->log.erase(impl_->log.begin(),
                             impl_->log.begin() + static_cast<std::ptrdiff_t>(impl_->log.size() - kMaxEntries));
        }
    }

    void ImGuiEditorUi::loadLayout(const std::string& path)
    {
        impl_->layoutPath = path;
        ImGui::SetCurrentContext(impl_->context);
        ImGui::LoadIniSettingsFromDisk(path.c_str());
    }

    void ImGuiEditorUi::saveLayout() const
    {
        if (impl_->layoutPath.empty()) { return; }
        ImGui::SetCurrentContext(impl_->context);
        ImGui::SaveIniSettingsToDisk(impl_->layoutPath.c_str());
    }
}
