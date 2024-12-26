#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
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
        sf::Font font("resources/arial.ttf");

        sf::RenderWindow window(sf::VideoMode({1440, 1080}), "Techno Logic");
        sf::Image        app_icon("resources/techno_logic_icon.png");

        // set app icon: works in windows, linux needs a .desktop file for such integration
        window.setIcon(app_icon.getSize(), app_icon.getPixelsPtr());

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
        ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::FocusLost{}));
        ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::FocusGained{}));

        Block block{"Example", 200};
        block.description = "This is an example block :)";
        Editor         editor{block};
        EditorRenderer rend{editor, window, font};

        sf::Clock deltaClock;
        while (window.isOpen()) {

            sf::Vector2i mousePixPos   = sf::Mouse::getPosition(window);
            sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixPos);
            // sf::Vector2i mousePos      = editor.snapToGrid(mouseWorldPos);

            while (auto maybe_event = window.pollEvent()) {
                auto& e = *maybe_event;
                ImGui::SFML::ProcessEvent(window, e);
                if (ImGui::GetIO().WantCaptureMouse && (e.is<sf::Event::MouseButtonPressed>() ||
                                                        e.is<sf::Event::MouseButtonReleased>()))
                    break;
                if (rend.event(e, mousePixPos)) break;
                editor.event(e);
                if (e.is<sf::Event::Closed>()) {
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
