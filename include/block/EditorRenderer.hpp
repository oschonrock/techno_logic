#pragma once

#include <cmath>

#include "SFML/Graphics.hpp"
#include <imgui.h>

#include "Editor.hpp"

class EditorRenderer {
    static constexpr float        defaultViewSize    = 35;
    static constexpr float        zoomFact           = 1.05f;
    static constexpr float        nameScale          = 1.5f;
    static constexpr float        crossSize          = 0.1f;
    static constexpr float        hlRad              = 0.1f;
    static constexpr float        nodeRad            = 0.1f;
    inline static const sf::Color nodeColour         = sf::Color::White;
    inline static const sf::Color conColour          = sf::Color::White;
    inline static const sf::Color newConColour       = sf::Color::Blue;
    inline static const sf::Color highlightConColour = sf::Color::Green;
    inline static const sf::Color crossCol{255, 255, 255, 70};

    const sf::Font& font;

    float vsScale;

    Editor       editor;
    const Block&      block;
    sf::RenderWindow& window;
    sf::View          view;

    std::vector<sf::Vertex> gridVertecies;
    sf::CircleShape         coordHl{hlRad};
    sf::Text                name;

    enum struct MoveStatus : std::size_t { idle = 0, moveStarted = 1, moveConfirmed = 2 };
    static constexpr std::array<std::string, 3> MoveStatusStrings{"idle", "moveStarted",
                                                                  "moveConfirmed"};
    MoveStatus                                  moveStatus{MoveStatus::idle};
    sf::Vector2i                                mousePosOriginal;
    sf::Vector2f                                mousePosLast;

    void setViewDefault() {
        vsScale =
            static_cast<float>(std::min(window.getSize().x, window.getSize().y)) / defaultViewSize;

        view = window.getDefaultView();
        view.zoom(1 / vsScale);
        view.setCenter(view.getSize() / 2.0f + sf::Vector2f{-1, -1});
        window.setView(view);
    }

  public:
    EditorRenderer(Block& block_, sf::RenderWindow& window_, const sf::Font& font_)
        : font(font_), editor(block_), block(block_), window(window_),
          gridVertecies(block.size * block.size * 4 + 8), name(block.name, font) {
        // make grid
        for (std::size_t x = 0; x < block.size; ++x) {
            for (std::size_t y = 0; y < block.size; ++y) {
                std::size_t  index = (x + y * block.size) * 4;
                sf::Vector2f pos{static_cast<float>(x), static_cast<float>(y)};
                gridVertecies[index]     = {pos - sf::Vector2f{-crossSize, 0.0f}, crossCol};
                gridVertecies[index + 1] = {pos - sf::Vector2f{crossSize, 0.0f}, crossCol};
                gridVertecies[index + 2] = {pos - sf::Vector2f{0.0f, -crossSize}, crossCol};
                gridVertecies[index + 3] = {pos - sf::Vector2f{0.0f, crossSize}, crossCol};
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
        coordHl.setFillColor(crossCol);
        coordHl.setOrigin({hlRad, hlRad});

        // set up name
        name.setScale(sf::Vector2f{1.0f, 1.0f} *
                      (nameScale / static_cast<float>(name.getCharacterSize())));
        name.setPosition({-0.8f, -1.0f - (nameScale * 1.5f)});

        // set up view
        setViewDefault();
    }

    void event(const sf::Event& event, const sf::Vector2i& mousePixPos) {
        sf::Vector2f mousePos = window.mapPixelToCoords(mousePixPos);
        if (ImGui::GetIO().WantCaptureMouse) return;
        if (event.type == sf::Event::MouseWheelMoved) {
            float        zoom = static_cast<float>(std::pow(zoomFact, -event.mouseWheel.delta));
            sf::Vector2f diff = mousePos - view.getCenter();
            view.zoom(zoom);
            view.move(diff * (1 - zoom));
            window.setView(view);
        }
        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            moveStatus       = MoveStatus::moveStarted;
            mousePosOriginal = mousePixPos;
            mousePosLast     = mousePos;
        }
        if (event.type == sf::Event::MouseButtonReleased &&
            event.mouseButton.button == sf::Mouse::Left) {
            if (moveStatus != MoveStatus::moveConfirmed) {
                editor.event(event, mousePos);
            }
            moveStatus = MoveStatus::idle;
        }
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
        draw(mousePos);
        editor.frame(mousePos);
    }

  private:
    void draw(const sf::Vector2f& mousePos) {
        sf::Vector2i mouseCoord = editor.snapToGrid(mousePos);

        // DEBUG
        ImGui::Begin("Debug", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Mouse pos: (%F, %F)", mousePos.x, mousePos.y);
        ImGui::Text("Closest coord: (%d, %d)", mouseCoord.x, mouseCoord.y);
        ImGui::Text("View size: (%F, %F)", view.getSize().x, view.getSize().y);
        ImGui::End();

        window.draw(gridVertecies.data(), gridVertecies.size(), sf::PrimitiveType::Lines);

        if (editor.state == Editor::BlockState::Connecting) { // draw new connection
            std::array<sf::Vertex, 2> conLine{
                sf::Vertex{sf::Vector2f{editor.conStartPos}, newConColour},
                sf::Vertex{sf::Vector2f{editor.conEndPos}, newConColour}};
            window.draw(conLine.data(), 2, sf::PrimitiveType::Lines);
            switch (typeOf(editor.conStartObjVar)) {
            case ObjAtCoordType::Con:
            case ObjAtCoordType::Empty: {
                sf::CircleShape newNode{1.5f * nodeRad};
                newNode.setFillColor(newConColour);
                newNode.setPosition(sf::Vector2f{editor.conStartPos} -
                                    (1.5f * sf::Vector2f{nodeRad, nodeRad}));
                window.draw(newNode);
            }
            default: // errors
                break;
            }
        }
        // draw connections
        std::vector<sf::Vertex> lineVerts;
        for (const auto& net: block.conNet.nets) {
            sf::Color col = conColour;
            if ((editor.conStartCloNet &&
                 editor.conStartCloNet.value() == net.ind) || // if needs highlighting
                (editor.conEndCloNet && editor.conEndCloNet.value() == net.ind)) {
                col = highlightConColour;
            }
            for (const auto& portPair: net.obj.getMap()) {
                lineVerts.emplace_back(sf::Vector2f(editor.getPort(portPair.first).portPos), col);
                lineVerts.emplace_back(sf::Vector2f(editor.getPort(portPair.second).portPos), col);
            }
        }
        window.draw(lineVerts.data(), lineVerts.size(), sf::PrimitiveType::Lines);
        // draw nodes
        for (const auto& node: block.nodes) {
            ImGui::Text("Node connection count: %zu", block.conNet.getNodeConCount(node.ind));
            if (block.conNet.getNodeConCount(node.ind) != 2) {
                sf::CircleShape circ{nodeRad};
                circ.setFillColor(nodeColour);
                circ.setPosition(sf::Vector2f{node.obj.pos} - sf::Vector2f{nodeRad, nodeRad});
                window.draw(circ);
            }
        }

        coordHl.setPosition(sf::Vector2f(mouseCoord));
        window.draw(coordHl);
        window.draw(name);
    }
};