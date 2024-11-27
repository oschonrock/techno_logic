#pragma once

#include <cstddef>
#include <string>

#include "SFML/System/Vector2.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

#include "details/StableVector.hpp"

enum struct Direction : std::size_t { up = 0, down = 1, left = 2, right = 3 };

class PortInst;

class Node {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
    std::size_t           noOfCons() const;
};

class BlockInst {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

class Gate {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

using VariantRef = std::variant<Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct ObjType : std::size_t { Node = 0, Gate = 1, BlockInst = 2 };

class Block {
    static constexpr float nodeRad = 0.1f;
    enum struct BlockState { idle, buildingConnection };
    BlockState state = BlockState::idle;

    VariantRef whatIsAtCoord(const sf::Vector2i& coord) const;

  public:
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;

    sf::Vector2i connectionStart;

    // returns wether it wants to capture the event (not allow other things to interpret it)
    bool event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2f& mousePos);
    void frame(sf::RenderWindow& window, const sf::Vector2f& mousePos);
    void draw(sf::RenderWindow& window);

    PortInst&  getPort(VariantRef ref, std::size_t portNum);
    VariantRef getObjRef(VariantRef ref);
};

class Connection {
  private:
    VariantRef  ref;
    std::size_t portNum;
    PortInst&   getDestPort(Block& block) const;
    Connection(VariantRef ref_, std::size_t portNum_) : ref(ref_), portNum(portNum_) {}
    friend PortInst;
};

class PortInst {
  private:
    VariantRef  objRef;
    std::size_t originPortNum;

  public:
    Direction    portDir;
    sf::Vector2i portPos;
    bool         isOutput;

    bool                      connected;
    std::optional<Connection> con;

    void setDest(Block& block, VariantRef ref, std::size_t portNum);
    void reset(Block& block);
};