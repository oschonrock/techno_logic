#pragma once

#include <cstddef>
#include <string>

#include "SFML/Window/Window.hpp"
#include "SFML/System/Vector2.hpp"

#include "details/StableVector.hpp"

enum class Direction{
  up = 0,
  down = 1,
  left = 2,
  right = 3
};

class Node{
  sf::Vector2i pos;
  // std::array<std::variant<>, 4>
};

class Block {
public:
  const std::size_t size = 100;
  std::string name;
  std::string description;

  void draw(sf::Window &window) {
    // draw connections, blocks, and gates
  }
};