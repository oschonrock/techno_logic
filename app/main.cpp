#include <cmath>

#include "imgui-SFML.h"

#include "SFML/System/Clock.hpp"
#include "SFML/Window/Event.hpp"

#include "block/EditorRenderer.hpp"

int main() {
    sf::Font font;
    if (!font.loadFromFile("resources/arial.ttf")) {
        throw std::runtime_error("failed to load arial.ttf");
    }

    sf::VideoMode    desktop = sf::VideoMode::getDesktopMode();
    // sf::ContextSettings cont{};
    // cont.hasFocus = true;
    sf::RenderWindow window(desktop, "Techo Logic", sf::Style::Fullscreen);
    window.requestFocus(); // fix windows imgui no response bug
    window.setFramerateLimit(60);
    assert(ImGui::SFML::Init(window));

    Block block{};
    block.name        = "Example";
    block.description = "This is an example block :)";
    Editor         editor{block};
    EditorRenderer rend{editor, window, font};

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Vector2i mousePixPos = sf::Mouse::getPosition(window);
        sf::Vector2i mousePos    = editor.snapToGrid(window.mapPixelToCoords(mousePixPos));

        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (rend.event(event, mousePixPos)) break;
            editor.event(event, mousePos);
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::SetTooltip(window.hasFocus() ? "Has focue" : "No focus");
        window.clear();
        editor.frame(mousePos); // update state
        rend.frame(mousePixPos);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
}