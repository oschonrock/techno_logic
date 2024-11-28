#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include "BlockInternals.hpp"

class Block {
    static constexpr float nodeRad = 0.1f;
    enum struct BlockState { Idle, NewConStart };
    enum struct NewConType : std::size_t { FromEmpty = 0, FromCon = 1, FromPort = 2, FromNode = 3 };
    BlockState state = BlockState::Idle;
    NewConType conType{};

    std::variant<std::monostate, Connection, PortRef, VariantRef>
                    whatIsAtCoord(const sf::Vector2i& coord) const;
    PortInst&       getPort(const PortRef& port);
    const PortInst& getPort(const PortRef& port) const;
    bool            collisionCheck(const Connection& connection, const sf::Vector2i& coord) const;

  public:
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;
    ClosedNet               conNet;

    sf::Vector2i connectionStart;

    // returns wether it wants to capture the event (not allow other things to interpret it)
    bool event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2i& mousePos);
    void frame(sf::RenderWindow& window, const sf::Vector2i& mousePos);
    void draw(sf::RenderWindow& window);
};