#pragma once

#include <cmath>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <imgui.h>
#include <stdexcept>

#include "Editor.hpp"

// editor renderer contains const& to editor and is only responsible for displaying its internal
// state
class EditorRenderer {
    bool      isGridCrosses   = true;
    float     defaultViewSize = 35;
    float     zoomFact        = 1.05f;
    float     nameScale       = 1.5f;
    float     crossSize       = 0.1f;
    float     nodeRad         = 0.1f;
    float     newNodeScale    = 1.5f;
    sf::Color gridColour{255, 255, 255, 70};
    sf::Color cursorPointColour{255, 255, 255, 100};
    sf::Color nodeColour   = sf::Color::White;
    sf::Color conColour    = sf::Color::White;
    sf::Color newConColour = sf::Color::Green;
    sf::Color overlapColour{255, 102, 0, 150};
    sf::Color highlightConColour = {102, 255, 255, 255};
    sf::Color errorColour        = sf::Color::Red;
    sf::Color debugConColour     = sf::Color::Magenta;
    sf::Color debugNodeColour    = sf::Color::Yellow;

    const sf::Font& font;

    const Editor&     editor;
    const Block&      block;
    sf::RenderWindow& window;
    sf::View          view;
    sf::Vector2u      prevWindowSize;

    std::vector<sf::Vertex>   gridVerts;
    std::array<sf::Vertex, 5> borderVerts;
    sf::Text                  name;

    enum struct MoveStatus : std::size_t { idle = 0, moveStarted = 1, moveConfirmed = 2 };
    static constexpr std::array<std::string, 3> MoveStatusStrings{"idle", "moveStarted",
                                                                  "moveConfirmed"};
    MoveStatus                                  moveStatus{MoveStatus::idle};
    sf::Vector2i                                mousePosOriginal;
    sf::Vector2f                                mousePosLast;
    bool                                        debugEnabled = true;
    std::optional<Ref<Node>>                    debugNode;
    std::optional<Connection>                   debugCon;
    std::optional<Ref<ClosedNet>>               debugNet;

    static std::string portRefToString(const PortRef& port) {
        std::string portName = PortObjRefStrings[port.ref.index()] + " ";
        if (typeOf(port) == PortObjType::Node) {
            portName += DirectionStrings[port.portNum];
        } else {
            portName += std::to_string(port.portNum);
        }
        return portName;
    }

    void setViewDefault() {
        float vsScale =
            static_cast<float>(std::min(window.getSize().x, window.getSize().y)) / defaultViewSize;

        view = window.getDefaultView();
        view.zoom(1 / vsScale);
        view.setCenter(view.getSize() / 2.0f + sf::Vector2f{-1, -1});
        window.setView(view);
        prevWindowSize = window.getSize();
    }

    void updateGrid() {
        gridVerts.clear();
        gridVerts.reserve(block.size * block.size * (isGridCrosses ? 4 : 1));
        for (std::size_t x = 0; x < block.size; ++x) {
            for (std::size_t y = 0; y < block.size; ++y) {
                sf::Vector2f pos{static_cast<float>(x), static_cast<float>(y)};
                if (isGridCrosses) {
                    gridVerts.emplace_back(pos - sf::Vector2f{-crossSize, 0.0f}, gridColour);
                    gridVerts.emplace_back(pos - sf::Vector2f{crossSize, 0.0f}, gridColour);
                    gridVerts.emplace_back(pos - sf::Vector2f{0.0f, -crossSize}, gridColour);
                    gridVerts.emplace_back(pos - sf::Vector2f{0.0f, crossSize}, gridColour);
                } else {
                    gridVerts.emplace_back(pos, gridColour);
                }
            }
        }
    }

  public:
    EditorRenderer(const Editor& editor_, sf::RenderWindow& window_, const sf::Font& font_)
        : font(font_), editor(editor_), block(editor.block), window(window_),
          name(block.name, font) {
        // make grid
        updateGrid();

        // make border
        borderVerts[0] = {{-1.0f, -1.0f}};
        borderVerts[1] = {{-1.0f, static_cast<float>(block.size)}};
        borderVerts[2] = {{static_cast<float>(block.size), static_cast<float>(block.size)}};
        borderVerts[3] = {{static_cast<float>(block.size), -1.0f}};
        borderVerts[4] = {{-1.0f, -1.0f}};

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
            bool captureEvent = moveStatus == MoveStatus::moveConfirmed;
            moveStatus        = MoveStatus::idle;
            return captureEvent;
        }
        if (event.type == sf::Event::Resized) {
            sf::Vector2f scale(
                static_cast<float>(event.size.width) / static_cast<float>(prevWindowSize.x),
                static_cast<float>(event.size.height) / static_cast<float>(prevWindowSize.y));
            view.setSize({view.getSize().x * scale.x, view.getSize().y * scale.y});
            prevWindowSize = {event.size.width, event.size.height};
            window.setView(view);
            return true;
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
            ImGui::Text("Window size: (%u, %u)", window.getSize().x, window.getSize().y);
            ImGui::Text("View size: (%F, %F)", view.getSize().x, view.getSize().y);
            ImGui::Text("Move status: %s",
                        MoveStatusStrings[static_cast<std::size_t>(moveStatus)].c_str());
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Current Connection")) {
            std::string startHover = ObjAtCoordStrings[editor.conStartObjVar.index()];
            switch (editor.state) {
            case Editor::EditorState::Idle: {
                ImGui::Text("Hovering %s at pos (%d, %d)", startHover.c_str(), editor.conStartPos.x,
                            editor.conStartPos.y);
                ImGui::Text("Proposed start point is %slegal", editor.conStartLegal ? "" : "il");
                break;
            }
            case Editor::EditorState::Connecting: {
                std::string endHover = ObjAtCoordStrings[editor.conEndObjVar.index()];
                ImGui::Text("Start connected to %s at pos (%d, %d)", startHover.c_str(),
                            editor.conStartPos.x, editor.conStartPos.y);
                ImGui::Text("Hovering %s at pos (%d, %d)", endHover.c_str(), editor.conEndPos.x,
                            editor.conEndPos.y);
                ImGui::Text("Proposed end point is %slegal", editor.conEndLegal ? "" : "il");
                break;
            }
            case Editor::EditorState::Deleting: {
                throw std::runtime_error("unhandled deleting editor state");
                break;
            }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Networks", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Node count: %zu", block.nodes.size());
            static ImGuiTableFlags flags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame |
                ImGuiTableFlags_Resizable; // ImGuiTableFlags_NoHostExtendX |
                                           // ImGuiTableFlags_SizingFixedFit
            if (ImGui::BeginTable("Connections", 4, flags)) {
                ImGui::TableSetupColumn("Network");
                ImGui::TableSetupColumn("Input");
                ImGui::TableSetupColumn("Output");
                ImGui::TableSetupColumn("Connections");
                ImGui::TableHeadersRow();
                int netCount = 1;
                for (const auto& net: block.nets) {
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
                        ImGui::TableSetColumnIndex(1);
                        if (net.obj.hasInput()) {
                            ImGui::Selectable(
                                PortObjRefStrings[net.obj.getInput().value().ref.index()].c_str());
                        } else {
                            ImGui::TextDisabled("none");
                        }
                        const auto& outputs = net.obj.getOutputs();
                        if (outputs.empty()) {
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextDisabled("none");
                        }
                        bool isFirst  = true;
                        auto conIt    = net.obj.begin();
                        auto outputIt = outputs.begin();
                        while (conIt != net.obj.end() || outputIt != outputs.end()) {
                            if (isFirst) {
                                isFirst = false;
                            } else {
                                ImGui::TableNextRow();
                            }
                            if (outputIt != outputs.end()) {
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Selectable(PortObjRefStrings[outputIt->ref.index()].c_str());
                            }
                            if (conIt != net.obj.end()) {
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Selectable(portRefToString(conIt->portRef1).c_str(),
                                                  debugNetHovered, 0,
                                                  {ImGui::GetContentRegionAvail().x / 2.0f,
                                                   ImGui::GetTextLineHeight()});
                                if (ImGui::IsItemHovered() &&
                                    typeOf(conIt->portRef1) == PortObjType::Node) {
                                    debugNode = std::get<Ref<Node>>(conIt->portRef1.ref);
                                    debugCon  = *conIt;
                                    ImGui::SetTooltip("Debug node and con");
                                }
                                ImGui::SameLine();
                                ImGui::Selectable(portRefToString(conIt->portRef2).c_str(),
                                                  debugNetHovered);
                                if (ImGui::IsItemHovered() &&
                                    typeOf(conIt->portRef2) == PortObjType::Node) {
                                    debugNode = std::get<Ref<Node>>(conIt->portRef2.ref);
                                    debugCon  = *conIt;
                                    ImGui::SetTooltip("Debug node and con");
                                }
                                ++conIt;
                            }
                        }
                        ImGui::TreePop();
                    } else {
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("...");
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
        window.draw(gridVerts.data(), gridVerts.size(),
                    isGridCrosses ? sf::PrimitiveType::Lines : sf::PrimitiveType::Points);
        window.draw(borderVerts.data(), borderVerts.size(), sf::PrimitiveType::LineStrip);

        // draw connections
        std::vector<sf::Vertex> connectionVerts{};
        for (const auto& net: block.nets) {
            sf::Color col = conColour;
            if (debugNet && debugNet.value() == net.ind) // sneaky debug overlay... rest down
                col = debugConColour;
            else if (editor.state == Editor::EditorState::Connecting &&
                     ((editor.conStartCloNet && editor.conStartCloNet.value() == net.ind) ||
                      (editor.conEndCloNet && editor.conEndCloNet.value() == net.ind))) {
                col = highlightConColour; // editor hover color
            }

            for (const auto& con: net.obj) {
                connectionVerts.emplace_back(sf::Vector2f(block.getPort(con.portRef1).portPos),
                                             col);
                connectionVerts.emplace_back(sf::Vector2f(block.getPort(con.portRef2).portPos),
                                             col);
            }
        }
        window.draw(connectionVerts.data(), connectionVerts.size(), sf::PrimitiveType::Lines);

        // draw nodes
        for (const auto& node: block.nodes) {
            if (block.getNodeConCount(node.ind) != 2) {
                drawNode(node.obj.pos, nodeRad, nodeColour);
            };
        }

        std::vector<sf::Vertex> lineVertecies{};
        // editor state based gui

        if (!editor.overlapPos.empty()) {
            ImGui::SetTooltip("Connection overlaps connected wires");
            for (const auto& pos: editor.overlapPos) {
                drawNode(pos, nodeRad * newNodeScale, overlapColour);
            }
        }

        switch (editor.state) {
        case Editor::EditorState::Idle:
            drawNode(mouseCoord, nodeRad, cursorPointColour);
            if (typeOf(editor.conStartObjVar) == ObjAtCoordType::ConCross) {
                drawNode(editor.conStartPos, newNodeScale * nodeRad, newConColour);
            }
            break;
        case Editor::EditorState::Connecting:
            if (editor.conEndLegal) {
                drawSingleLine(lineVertecies, editor.conStartPos, editor.conEndPos, newConColour);
                drawNode(editor.conEndPos, newNodeScale * nodeRad, newConColour);
            }
            drawNode(editor.conStartPos, newNodeScale * nodeRad,
                     editor.conEndLegal ? newConColour : errorColour);
            break;
        case Editor::EditorState::Deleting:
            if (editor.delLegal) {
                switch (typeOf(editor.delObjVar)) {
                case ObjAtCoordType::Con: {
                    auto con = std::get<Connection>(editor.delObjVar);
                    drawSingleLine(lineVertecies, block.getPort(con.portRef1).portPos,
                                   block.getPort(con.portRef2).portPos, sf::Color::Red);
                    break;
                }
                default:
                    break;
                }
            }
        }

        // Debug overlays
        if (debugNet) { // draw debug nodes over top
            const auto& net = block.nets[debugNet.value()];
            for (const auto& con: net) {
                // potentially multi draw when multi connected port
                auto port = block.getPort(con.portRef1);
                drawNode(port.portPos, nodeRad * newNodeScale, debugNodeColour);
                port = block.getPort(con.portRef2);
                drawNode(port.portPos, nodeRad * newNodeScale, debugNodeColour);
            }
        }
        if (debugNode) { // draw debug node over top
            drawNode(block.nodes[debugNode.value()].pos, newNodeScale * nodeRad, debugNodeColour);
        }
        if (debugCon) { // draw debug con over top
            drawSingleLine(lineVertecies, block.getPort(debugCon->portRef1).portPos,
                           block.getPort(debugCon->portRef2).portPos, debugConColour);
        }
        window.draw(lineVertecies.data(), lineVertecies.size(), sf::PrimitiveType::Lines);
    }
};
