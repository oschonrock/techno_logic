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

    sf::RenderWindow window(sf::VideoMode({1440, 1080}), "Techo Logic");
    window.setFramerateLimit(160);
    assert(ImGui::SFML::Init(window));
    // fix windows imgui no response bug
    ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::LostFocus));
    ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::GainedFocus));

    Block block{"Example", 200};
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
            if (ImGui::GetIO().WantCaptureMouse) break;
            if (rend.event(event, mousePixPos)) break;
            editor.event(event);
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        window.clear();
        editor.frame(mousePos); // update state
        rend.frame(mousePixPos);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
}