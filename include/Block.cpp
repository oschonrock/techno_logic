#include "Block.hpp"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Window/Event.hpp>

sf::Vector2i dirToVec(Direction dir) {
    switch (dir) {
    case Direction::up:
        return {0, -1};
    case Direction::down:
        return {0, 1};
    case Direction::left:
        return {-1, 0};
    case Direction::right:
        return {1, 0};
    }
}

Direction reverseDirection(Direction dir) {
    int val = static_cast<int>(dir);
    val += val % 2 == 0 ? 1 : -1;
    return Direction(val);
}

// note 0,0 = false
bool isVecHoriVert(sf::Vector2i vec) { return ((vec.x != 0) != (vec.y != 0)); }

int dot(sf::Vector2i a, sf::Vector2i b) { return a.x * b.x + a.y * b.y; }

bool isVecInDir(sf::Vector2i vec, Direction dir) {
    assert(isVecHoriVert(vec));
    return dot(vec, dirToVec(dir)) > 0; // if dot prouct > 0 then must be in same dir
}

// Node
size_t Node::noOfCons() const {
    std::size_t count{};
    for (const auto& p: ports) {
        if (p.con) {
            ++count;
        }
    }
    return count;
}

// Block
VariantRef Block::whatIsAtCoord(const sf::Vector2i& coord) const {}

bool Block::event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2f& mousePos) {
    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left) {
        switch (state) {
        case BlockState::idle: // new action begun
            break;
        case BlockState::buildingConnection: // connection finished
            break;
        }
    }
    return false;
}

void Block::frame(sf::RenderWindow& window, const sf::Vector2f& mousePos) {}

void Block::draw(sf::RenderWindow& window) {
    auto circ = sf::CircleShape(nodeRad);
    circ.setOrigin({-nodeRad, -nodeRad});
    for (const auto& node: nodes) {
        if (node.obj.noOfCons() != 2) {
            circ.setPosition(sf::Vector2f(node.obj.pos));
            window.draw(circ);
        }
    }
}

PortInst& Block::getPort(VariantRef ref, std::size_t portNum) {
    switch (ObjType(ref.index())) {
    case ObjType::Node:
        return nodes[std::get<Ref<Node>>(ref)].ports[portNum];
    case ObjType::Gate:
        return gates[std::get<Ref<Gate>>(ref)].ports[portNum];
    case ObjType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(ref)].ports[portNum];
    }
}

// Connection
PortInst& Connection::getDestPort(Block& block) const { return block.getPort(ref, portNum); }

// PortInst
void PortInst::setDest(Block& block, VariantRef destRef, std::size_t portNum) {
    if (connected) {
        reset(block); // remove other half of original connection
    }
    connected          = true;
    con                = Connection{destRef, portNum};
    PortInst& destPort = con.value().getDestPort(block);
    if (destPort.connected) {
        destPort.reset(block); // remove other half of original connection to dest
    }
    destPort.connected = true;
    destPort.con       = Connection{objRef, originPortNum};
}
void PortInst::reset(Block& block) {
    if (!connected) {
        return;
    }
    PortInst& destPort = con.value().getDestPort(block);
    connected          = false;
    destPort.connected = false;
    con.reset();
    destPort.con.reset();
}
