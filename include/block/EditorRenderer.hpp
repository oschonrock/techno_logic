#pragma once

#include <cmath>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <imgui.h>

#include "Editor.hpp"

// editor renderer contains const& to editor and is only responsible for displaying its internal
// state
class EditorRenderer {
    static constexpr float defaultViewSize = 35;
    static constexpr float zoomFact        = 1.05f;
    static constexpr float nameScale       = 1.5f;
    static constexpr float crossSize       = 0.1f;
    static constexpr float nodeRad         = 0.1f;
    static constexpr float hlRad           = 0.1f;
    sf::Color              crossColour{255, 255, 255, 70};
    sf::Color              indicatorColour{255, 255, 255, 100};
    sf::Color              nodeColour         = sf::Color::White;
    sf::Color              conColour          = sf::Color::White;
    sf::Color              newConColour       = sf::Color::Blue;
    sf::Color              highlightConColour = sf::Color::Green;
    sf::Color              errorColour        = sf::Color::Red;
    sf::Color              debugConColour     = sf::Color::Magenta;
    sf::Color              debugNodeColour    = sf::Color::Yellow;

    const sf::Font& font;

    float vsScale;

    const Editor&     editor;
    const Block&      block;
    sf::RenderWindow& window;
    sf::View          view;
    sf::Vector2u      prevWindowSize;

    std::vector<sf::Vertex> gridVertecies;
    sf::CircleShape         mouseIndicator{hlRad};
    sf::Text                name;

    enum struct MoveStatus : std::size_t { idle = 0, moveStarted = 1, moveConfirmed = 2 };
    static constexpr std::array<std::string, 3> MoveStatusStrings{"idle", "moveStarted",
                                                                  "moveConfirmed"};
    MoveStatus                                  moveStatus{MoveStatus::idle};
    sf::Vector2i                                mousePosOriginal;
    sf::Vector2f                                mousePosLast;
    bool                                        debugEnabled = true;
    std::optional<Ref<Node>>                    debugNode;
    std::optional<std::pair<PortRef, PortRef>>  debugCon;
    std::optional<Ref<ClosedNet>>               debugNet;

    void setViewDefault() {
        vsScale =
            static_cast<float>(std::min(window.getSize().x, window.getSize().y)) / defaultViewSize;

        view = window.getDefaultView();
        view.zoom(1 / vsScale);
        view.setCenter(view.getSize() / 2.0f + sf::Vector2f{-1, -1});
        window.setView(view);
        prevWindowSize = window.getSize();
    }

  public:
    EditorRenderer(const Editor& editor_, sf::RenderWindow& window_, const sf::Font& font_)
        : font(font_), editor(editor_), block(editor.block), window(window_),
          gridVertecies(block.size * block.size * 4 + 8), name(block.name, font) {
        // make grid
        for (std::size_t x = 0; x < block.size; ++x) {
            for (std::size_t y = 0; y < block.size; ++y) {
                std::size_t  index = (x + y * block.size) * 4;
                sf::Vector2f pos{static_cast<float>(x), static_cast<float>(y)};
                gridVertecies[index]     = {pos - sf::Vector2f{-crossSize, 0.0f}, crossColour};
                gridVertecies[index + 1] = {pos - sf::Vector2f{crossSize, 0.0f}, crossColour};
                gridVertecies[index + 2] = {pos - sf::Vector2f{0.0f, -crossSize}, crossColour};
                gridVertecies[index + 3] = {pos - sf::Vector2f{0.0f, crossSize}, crossColour};
            }
        }

        // make border
        sf::Vertex bl = {{-1.0f, -1.0f}};
        sf::Vertex tl = {{-1.0f, static_cast<float>(block.size)}};
        sf::Vertex tr = {{static_cast<float>(block.size), static_cast<float>(block.size)}};
        sf::Vertex br = {{static_cast<float>(block.size), -1.0f}};

        // maybe should be replaced by a linestrip or other drawing
        gridVertecies[gridVertecies.size() - 8] = bl;
        gridVertecies[gridVertecies.size() - 7] = tl;
        gridVertecies[gridVertecies.size() - 6] = tl;
        gridVertecies[gridVertecies.size() - 5] = tr;
        gridVertecies[gridVertecies.size() - 4] = tr;
        gridVertecies[gridVertecies.size() - 3] = br;
        gridVertecies[gridVertecies.size() - 2] = br;
        gridVertecies[gridVertecies.size() - 1] = bl;

        // set up highlighter
        mouseIndicator.setOrigin({hlRad, hlRad});

        // set up name
        name.setScale(sf::Vector2f{1.0f, 1.0f} *
                      (nameScale / static_cast<float>(name.getCharacterSize())));
        name.setPosition({-0.8f, -1.0f - (nameScale * 1.5f)});

        // set up view
        setViewDefault();
    }

    // returns if it wants to capture event
    bool event(const sf::Event& event, const sf::Vector2i& mousePixPos) {
        sf::Vector2f mousePos = window.mapPixelToCoords(mousePixPos);
        if (ImGui::GetIO().WantCaptureMouse) return false;
        if (event.type == sf::Event::MouseWheelMoved) {
            float        zoom = static_cast<float>(std::pow(zoomFact, -event.mouseWheel.delta));
            sf::Vector2f diff = mousePos - view.getCenter();
            view.zoom(zoom);
            view.move(diff * (1 - zoom));
            window.setView(view);
            return true;
        }
        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            moveStatus       = MoveStatus::moveStarted;
            mousePosOriginal = mousePixPos;
            mousePosLast     = mousePos;
            return true;
        }
        if (event.type == sf::Event::MouseButtonReleased &&
            event.mouseButton.button == sf::Mouse::Left) {
            moveStatus = MoveStatus::idle;
            return moveStatus == MoveStatus::moveConfirmed; // capture release if move confirmed
        }
        if (event.type == sf::Event::Resized) {
            sf::Vector2f scale(
                static_cast<float>(event.size.width) / static_cast<float>(prevWindowSize.x),
                static_cast<float>(event.size.height) / static_cast<float>(prevWindowSize.y));
            view.setSize({view.getSize().x * scale.x, view.getSize().y * scale.y});
            prevWindowSize = {event.size.width, event.size.height};
            window.setView(view);
        }
        return false;
    }

    // called every visual frame
    void frame(const sf::Vector2i& mousePixPos) {
        sf::Vector2f mousePos = window.mapPixelToCoords(mousePixPos);
        if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && moveStatus != MoveStatus::idle) {
            if (moveStatus == MoveStatus::moveConfirmed)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            view.move(mousePosLast - mousePos); // moves grabbed point underneath cursor
            window.setView(view);
            if (mag(mousePixPos - mousePosOriginal) / static_cast<float>(window.getSize().x) >
                0.03f)
                moveStatus = MoveStatus::moveConfirmed;
        }
        // DEBUG
        debugCon.reset();
        debugNode.reset();
        debugNet.reset();
        if (debugEnabled) {
            debug(mousePos);
        }
        draw(mousePos);
    }

  private:
    void drawNode(const sf::Vector2i& pos, float radius, const sf::Color& col) {
        sf::CircleShape circ{radius};
        circ.setFillColor(col);
        circ.setPosition(sf::Vector2f(pos) - (sf::Vector2f(radius, radius)));
        window.draw(circ);
    }

    void drawSingleLine(std::vector<sf::Vertex>& vertVec, const sf::Vector2i& pos1,
                        const sf::Vector2i& pos2, const sf::Color& col) {
        vertVec.emplace_back(sf::Vector2f{pos1}, col);
        vertVec.emplace_back(sf::Vector2f{pos2}, col);
    }

    void debug(const sf::Vector2f& mousePos) {
        auto mouseCoord = editor.snapToGrid(mousePos);
        ImGui::Begin("Debug", NULL);
        if (ImGui::TreeNode("Visual")) {
            ImGui::Text("Mouse pos: (%F, %F)", mousePos.x, mousePos.y);
            ImGui::Text("Closest coord: (%d, %d)", mouseCoord.x, mouseCoord.y);
            ImGui::Text("View size: (%F, %F)", view.getSize().x, view.getSize().y);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Current Connection")) {
            std::string startHover = ObjAtCoordStrings[editor.conStartObjVar.index()];
            switch (editor.state) {
            case Editor::EditorState::Idle:
                ImGui::Text("Hovering %s at pos (%d, %d)", startHover.c_str(), editor.conStartPos.x,
                            editor.conStartPos.y);
                ImGui::Text("Proposed start point is %slegal", editor.conStartLegal ? "" : "il");
                break;
            case Editor::EditorState::Connecting:
                std::string endHover = ObjAtCoordStrings[editor.conEndObjVar.index()];
                ImGui::Text("Start connected to %s at pos (%d, %d)", startHover.c_str(),
                            editor.conStartPos.x, editor.conStartPos.y);
                ImGui::Text("Hovering %s", endHover.c_str());
                ImGui::Text("Proposed end point is %slegal", editor.conEndLegal ? "" : "il");
                break;
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Networks", ImGuiTreeNodeFlags_DefaultOpen)) {
            static ImGuiTableFlags flags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame |
                ImGuiTableFlags_Resizable; // ImGuiTableFlags_NoHostExtendX |
                                           // ImGuiTableFlags_SizingFixedFit
            if (ImGui::BeginTable("Connections", 3, flags)) {
                ImGui::TableSetupColumn("Network");
                ImGui::TableSetupColumn("Port1");
                ImGui::TableSetupColumn("Port2");
                ImGui::TableHeadersRow();
                int netCount = 1;
                for (const auto& net: block.conNet.nets) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushID(netCount);
                    bool netOpen         = ImGui::TreeNodeEx("",
                                                             ImGuiTreeNodeFlags_DefaultOpen |
                                                                 ImGuiTreeNodeFlags_AllowItemOverlap |
                                                                 ImGuiTreeNodeFlags_SpanAvailWidth,
                                                             "Net %d", netCount);
                    bool debugNetHovered = false;
                    if (ImGui::IsItemHovered()) {
                        debugNet        = net.ind;
                        debugNetHovered = true;
                        ImGui::SetTooltip("Debug net");
                    }
                    if (netOpen) {
                        std::size_t conCount = 1;
                        for (const auto& portPair: net.obj.getMap()) {
                            if (conCount != 1) {
                                ImGui::TableNextRow();
                            }

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(PortObjRefStrings[portPair.first.ref.index()].c_str(),
                                              debugNetHovered);
                            if (ImGui::IsItemHovered() &&
                                typeOf(portPair.first) == PortObjType::Node) {
                                debugNode = std::get<Ref<Node>>(portPair.first.ref);
                                debugCon  = portPair;
                                ImGui::SetTooltip("Debug node and con");
                            }

                            ImGui::TableNextColumn();
                            ImGui::Selectable(
                                PortObjRefStrings[portPair.second.ref.index()].c_str(),
                                debugNetHovered);
                            if (ImGui::IsItemHovered() &&
                                typeOf(portPair.second) == PortObjType::Node) {
                                debugNode = std::get<Ref<Node>>(portPair.second.ref);
                                debugCon  = portPair;
                                ImGui::SetTooltip("Debug node and con");
                            }
                            ++conCount;
                        }
                        ImGui::TreePop();
                    } else {
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("...");
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("...");
                    }
                    ImGui::PopID();
                    ++netCount;
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::End();
    }

    void draw(const sf::Vector2f& mousePos) {
        sf::Vector2i mouseCoord = editor.snapToGrid(mousePos);

        // basics
        window.draw(name);
        window.draw(gridVertecies.data(), gridVertecies.size(), sf::PrimitiveType::Lines);

        // draw connections
        std::vector<sf::Vertex> conVerts{};
        for (const auto& net: block.conNet.nets) {
            sf::Color col = conColour;
            if (debugNet && debugNet.value() == net.ind) // sneaky debug overlay... rest down
                col = debugConColour;
            else if ((editor.conStartCloNet && editor.conStartCloNet.value() == net.ind) ||
                     (editor.conEndCloNet && editor.conEndCloNet.value() == net.ind)) {
                col = highlightConColour; // editor hover color
            }

            for (const auto& portPair: net.obj.getMap()) {
                conVerts.emplace_back(sf::Vector2f(block.getPort(portPair.first).portPos), col);
                conVerts.emplace_back(sf::Vector2f(block.getPort(portPair.second).portPos), col);
            }
        }
        window.draw(conVerts.data(), conVerts.size(), sf::PrimitiveType::Lines);

        // draw nodes
        for (const auto& node: block.nodes) {
            if (block.conNet.getNodeConCount(node.ind) != 2) {
                drawNode(node.obj.pos, nodeRad, nodeColour);
            };
        }

        std::vector<sf::Vertex> lineVertecies{};
        // editor state based gui
        switch (editor.state) {
        case Editor::EditorState::Idle:
            mouseIndicator.setPosition(sf::Vector2f(mouseCoord));
            mouseIndicator.setFillColor(editor.conStartLegal ? indicatorColour : errorColour);
            window.draw(mouseIndicator);
            break;
        case Editor::EditorState::Connecting:
            if (editor.conEndLegal) {
                drawSingleLine(lineVertecies, editor.conStartPos, editor.conEndPos,
                               editor.conEndLegal ? newConColour : errorColour);
            }
            switch (typeOf(editor.conStartObjVar)) {
            case ObjAtCoordType::Con:
            case ObjAtCoordType::Empty: {
                drawNode(editor.conStartPos, 1.2f * nodeRad,
                         editor.conEndLegal ? newConColour : errorColour);
            }
            default: // errors
                break;
            }
        }

        // Debug overlays
        if (debugNet) { // draw debug nodes over top
            const auto& net = block.conNet.nets[debugNet.value()];
            for (const auto& portRef: net.getMap()) {
                // potentially multi draw when multi connected port
                auto port = block.getPort(portRef.first);
                drawNode(port.portPos, nodeRad * 1.2f, debugNodeColour);
                port = block.getPort(portRef.second);
                drawNode(port.portPos, nodeRad * 1.2f, debugNodeColour);
            }
        }
        if (debugNode) { // draw debug node over top
            drawNode(block.nodes[debugNode.value()].pos, 1.2f * nodeRad, debugNodeColour);
        }
        if (debugCon) { // draw debug con over top
            drawSingleLine(lineVertecies, block.getPort(debugCon->first).portPos,
                           block.getPort(debugCon->second).portPos, debugConColour);
        }
        window.draw(lineVertecies.data(), lineVertecies.size(), sf::PrimitiveType::Lines);
    }
};