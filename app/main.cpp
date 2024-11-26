#include "imgui-SFML.h"
#include "imgui.h"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <cmath>

int main() {
  sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
  sf::RenderWindow window(desktop, "ImGui + SFML = <3", sf::Style::Fullscreen);
  window.setFramerateLimit(60);
  assert(ImGui::SFML::Init(window));

  const float crossSize = 0.1f;

  // make grid
  std::size_t size = 15;
  std::vector<sf::Vertex> gridCrosses{size * size * 4 + 8};
  for (std::size_t x = 0; x < size; ++x) {
    for (std::size_t y = 0; y < size; ++y) {
      std::size_t index = (x + y * size) * 4;
      gridCrosses[index] = {
          {static_cast<float>(x) - crossSize, static_cast<float>(y)}};
      gridCrosses[index + 1] = {
          {static_cast<float>(x) + crossSize, static_cast<float>(y)}};
      gridCrosses[index + 2] = {
          {static_cast<float>(x), static_cast<float>(y) - crossSize}};
      gridCrosses[index + 3] = {
          {static_cast<float>(x), static_cast<float>(y) + crossSize}};
    }
  }

  // make border
  sf::Vertex bl = {{-1.0f, -1.0f}};
  sf::Vertex tl = {{-1.0f, static_cast<float>(size)}};
  sf::Vertex tr = {{static_cast<float>(size), static_cast<float>(size)}};
  sf::Vertex br = {{static_cast<float>(size), -1.0f}};

  gridCrosses[gridCrosses.size() - 8] = bl;
  gridCrosses[gridCrosses.size() - 7] = tl;
  gridCrosses[gridCrosses.size() - 6] = tl;
  gridCrosses[gridCrosses.size() - 5] = tr;
  gridCrosses[gridCrosses.size() - 4] = tr;
  gridCrosses[gridCrosses.size() - 3] = br;
  gridCrosses[gridCrosses.size() - 2] = br;
  gridCrosses[gridCrosses.size() - 1] = bl;

  // set up view
  sf::View view = window.getDefaultView();
  const float vsScale =
      static_cast<float>(std::min(desktop.width, desktop.height)) /
      static_cast<float>(size + 2);
  view.zoom(1 / vsScale);
  view.setCenter(sf::Vector2f{1.0f, 1.0f} *
                 (static_cast<float>(size - 1) / 2.0f));
  window.setView(view);
#
  constexpr float radius = 0.1f;
  sf::CircleShape coordHighlighter(radius);
  coordHighlighter.setFillColor(sf::Color{0, 255, 0, 120});
  coordHighlighter.setOrigin({radius, radius});

  sf::Clock deltaClock;
  while (window.isOpen()) {
    sf::Vector2i mousePixPos = sf::Mouse::getPosition(window);
    sf::Vector2f mousePos = window.mapPixelToCoords(mousePixPos);
    mousePos.y = mousePos.y;

    sf::Vector2i closestCoord = {
        std::clamp(static_cast<int>(std::round(mousePos.x)), 0,
                   static_cast<int>(size - 1)),
        std::clamp(static_cast<int>(std::round(mousePos.y)), 0,
                   static_cast<int>(size - 1))};

    sf::Event event;
    while (window.pollEvent(event)) {
      ImGui::SFML::ProcessEvent(window, event);

      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    ImGui::SFML::Update(window, deltaClock.restart());

    ImGui::BeginTooltip();
    ImGui::Text("Mouse pos: (%F, %F)", mousePos.x, mousePos.y);
    ImGui::Text("Closest coord: (%d, %d)", closestCoord.x, closestCoord.y);
    ImGui::EndTooltip();

    window.clear();
    window.draw(gridCrosses.data(), gridCrosses.size(), sf::Lines);
    coordHighlighter.setPosition(sf::Vector2f(closestCoord));
    window.draw(coordHighlighter);
    ImGui::SFML::Render(window);
    window.display();
  }

  ImGui::SFML::Shutdown();
}