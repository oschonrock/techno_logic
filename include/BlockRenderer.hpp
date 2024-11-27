#pragma once

#include <cmath>
#include <imGUI.h>
#include <iostream>
#include <optional>

#include "SFML/Graphics.hpp"

#include "Block.hpp"

class BlockRenderer {
    static constexpr float        defaultViewSize = 35;
    static constexpr float        crossSize       = 0.1f;
    inline static const sf::Color crossCol{255, 255, 255, 70};
    static constexpr float        hlRad     = 0.1f;
    static constexpr float        zoomFact  = 1.05f;
    static constexpr float        nameScale = 1.5f;

    const sf::Font& font;

    float vsScale;

    Block&            block;
    sf::RenderWindow& window;
    sf::View          view;

    std::vector<sf::Vertex> gridVertecies;
    sf::CircleShape         coordHl{hlRad};
    sf::Text                name;

    std::optional<sf::Vector2f> mousePosLast;

    void setViewDefault() {
        vsScale =
            static_cast<float>(std::min(window.getSize().x, window.getSize().y)) / defaultViewSize;

        view = window.getDefaultView();
        view.zoom(1 / vsScale);
        view.setCenter(view.getSize() / 2.0f + sf::Vector2f{-1, -1});
        window.setView(view);
    }

  public:
    BlockRenderer(Block& blck_, sf::RenderWindow& window_, const sf::Font& font_)
        : font(font_), block(blck_), window(window_),
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
        coordHl.setFillColor(sf::Color{255, 0, 0, 100});
        coordHl.setOrigin({hlRad, hlRad});

        // set up name
        name.setScale(sf::Vector2f{1.0f, 1.0f} *
                      (nameScale / static_cast<float>(name.getCharacterSize())));
        name.setPosition({-0.8f, -1.0f - (nameScale * 1.5f)});

        // set up view
        setViewDefault();
    }

    void event(const sf::Event& event, const sf::Vector2f& mousePos) {
        if (block.event(event, window, mousePos) || ImGui::GetIO().WantCaptureMouse) return;
        if (event.type == sf::Event::MouseWheelMoved) {
            float        zoom = static_cast<float>(std::pow(zoomFact, -event.mouseWheel.delta));
            sf::Vector2f diff = mousePos - view.getCenter();
            view.zoom(zoom);
            view.move(diff * (1 - zoom));
            window.setView(view);
            // std::cout << event.mouseWheel.delta << "\n";
        } else if (event.type == sf::Event::MouseButtonReleased) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                mousePosLast.reset();
            }
        }
    }

    // called every visual frame
    void frame(const sf::Vector2f& mousePos) {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (!mousePosLast)
                mousePosLast = mousePos;
            else {
                // moves grabbed point underneath cursor
                ImGui::SetTooltip("Moving"); // DEBUG
                view.move(*mousePosLast - mousePos);
                window.setView(view);
            }
        }
        draw(mousePos);
    }

  private:
    void draw(const sf::Vector2f& mousePos) {
        sf::Vector2i closestCoord = {std::clamp(static_cast<int>(std::round(mousePos.x)), 0,
                                                static_cast<int>(block.size - 1)),
                                     std::clamp(static_cast<int>(std::round(mousePos.y)), 0,
                                                static_cast<int>(block.size - 1))};

        ImGui::BeginTooltip();
        ImGui::Text("Mouse pos: (%F, %F)", mousePos.x, mousePos.y); // DEBUG
        ImGui::Text("Closest coord: (%d, %d)", closestCoord.x,
                    closestCoord.y); // DEBUG
        ImGui::Text("View size: (%F, %F)", view.getSize().x, view.getSize().y);
        ImGui::EndTooltip();

        window.draw(gridVertecies.data(), gridVertecies.size(), sf::PrimitiveType::Lines);

        coordHl.setPosition(sf::Vector2f(closestCoord));
        window.draw(coordHl);
        window.draw(name);

        block.draw(window);
    }
};