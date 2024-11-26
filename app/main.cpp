#include <cmath>

#include "imgui-SFML.h"
#include "imgui.h"

#include "SFML/Graphics/RenderWindow.hpp"
#include "SFML/System/Clock.hpp"
#include "SFML/Window/Event.hpp"

#include "BlockRenderer.hpp"

int main() {
  sf::Font font;
  if (!font.loadFromFile("resources/arial.ttf")) {
    throw std::runtime_error("failed to load arial.ttf");
  }

  sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
  sf::RenderWindow window(desktop, "ImGui + SFML = <3", sf::Style::Fullscreen);
  window.setFramerateLimit(60);
  assert(ImGui::SFML::Init(window));

  Block block{};
  block.name = "Example";
  block.description = "This is an example block :)";
  BlockRenderer rend{block, window, font};

  sf::Clock deltaClock;
  while (window.isOpen()) {
    sf::Vector2i mousePixPos = sf::Mouse::getPosition(window);
    sf::Vector2f mousePos = window.mapPixelToCoords(mousePixPos);
    mousePos.y = mousePos.y;

    sf::Event event;
    while (window.pollEvent(event)) {
      ImGui::SFML::ProcessEvent(window, event);
      rend.event(event, mousePos);
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    ImGui::SFML::Update(window, deltaClock.restart());

    window.clear();
    rend.frame(mousePos);
    ImGui::SFML::Render(window);
    window.display();
  }

  ImGui::SFML::Shutdown();
}