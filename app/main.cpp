#include <cmath>
#include <cstdlib>
#include <imgui.h>
#include <stdexcept>

#include "imgui-SFML.h"

#include "SFML/System/Clock.hpp"
#include "SFML/Window/Event.hpp"

#include "block/EditorRenderer.hpp"

int main() {
    try {
        sf::Font font;
        if (!font.loadFromFile("resources/arial.ttf")) {
            throw std::runtime_error("failed to load arial.ttf");
        }

        sf::RenderWindow window(sf::VideoMode({1440, 1080}), "Techno Logic");
        auto             app_icon = sf::Image{};
        if (!app_icon.loadFromFile("resources/techno_logic_icon.png")) {
            throw std::runtime_error("failed to load application icon");
        }

        // set app icon: works in windows, linux needs a .desktop file for such integration
        window.setIcon(app_icon.getSize().x, app_icon.getSize().y, app_icon.getPixelsPtr());

        window.setFramerateLimit(160);
        if (!ImGui::SFML::Init(window, false)) { // don't load default font
            throw std::runtime_error("ImGui failed to Init window");
        }

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("resources/arial.ttf", 20.f);

        if (!ImGui::SFML::UpdateFontTexture()) { // important call: updates font texture
            throw std::runtime_error("ImGui failed to update font texture");
        }

        // fix windows imgui no response bug
        ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::LostFocus));
        ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::GainedFocus));

        Block block{"Example", 200};
        block.description = "This is an example block :)";
        Editor         editor{block};
        EditorRenderer rend{editor, window, font};

        sf::Clock deltaClock;
        while (window.isOpen()) {

            sf::Vector2i mousePixPos   = sf::Mouse::getPosition(window);
            sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixPos);
            sf::Vector2i mousePos      = editor.snapToGrid(mouseWorldPos);

            sf::Event event;
            while (window.pollEvent(event)) {
                ImGui::SFML::ProcessEvent(window, event);
                if (ImGui::GetIO().WantCaptureMouse &&
                    (event.type == sf::Event::MouseButtonPressed ||
                     event.type == sf::Event::MouseButtonReleased))
                    break;
                if (rend.event(event, mousePixPos)) break;
                editor.event(event);
                if (event.type == sf::Event::Closed) {
                    window.close();
                }
            }

            ImGui::SFML::Update(window, deltaClock.restart());

            window.clear();
            editor.frame(mouseWorldPos); // update state
            rend.frame(mousePixPos);
            ImGui::SFML::Render(window);
            window.display();
        }

        ImGui::SFML::Shutdown();
    } catch (const std::exception& e) {
        std::cerr << "Something went wrong: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
