#pragma once

#include <cstddef>
#include <string>

#include "SFML/System/Vector2.hpp"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "details/StableVector.hpp"

class Block;
class Gate;
class Node;

enum struct Direction : std::size_t { up = 0, down = 1, left = 2, right = 3 };

template <typename T>
concept connectable = std::same_as<T, Block> || std::same_as<T, Gate>;

template <connectable T>
struct FinalConnection {
    Ref<T>      ref;
    std::size_t inputNum;
};

struct EmptyConnection {};

using connection =
    std::variant<EmptyConnection, Ref<Node>, FinalConnection<Block>, FinalConnection<Gate>>;

enum struct connectionType : std::size_t { Empty = 0, Node = 1, Block = 2, Gate = 3 };

class Node {
  public:
    sf::Vector2i pos;
    // list of connection to nodes/final points. In order of Direction enum ^^
    std::array<connection, 4> connections;

    unsigned noOfCons() const {
        unsigned count{};
        for (const auto& x: connections) {
            if (connectionType(x.index()) != connectionType::Empty) {
                ++count;
            }
        }
        return count;
    }
};

class Block {
    static constexpr float nodeRad = 0.1f;

  public:
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<Node> nodes;
    //   StableVector<blockInstance> blockInstances;
    //   StableVector<Gate> gates;

    std::optional<sf::Vector2i> start;

    //   void proposeConnection(sf::Vector2i start, sf::Vector2i end) {
    //     assert((start.x == end.x) !=
    //            (start.y == end.y)); // proposed con is vert or hori
    //   }

    void event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2f& mousePos) {}

    void frame(sf::RenderWindow& window) {}

    void draw(sf::RenderWindow& window) {
        auto circ = sf::CircleShape(nodeRad);
        circ.setOrigin({-nodeRad, -nodeRad});
        for (const auto& node: nodes) {
            if (node.obj.noOfCons() != 2) {
                circ.setPosition(sf::Vector2f(node.obj.pos));
                window.draw(circ);
            }
        }
    }
};