#include "block/Block.hpp"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Window/Event.hpp>

// Block
std::variant<std::monostate, Connection, PortRef, VariantRef>
Block::whatIsAtCoord(const sf::Vector2i& coord) const {
    for (const auto& node: nodes) {
        if (coord == node.obj.pos) return node.ind; // check for nodes
    }
    // for (const auto& net: conNetworks) {
    //     for (const auto& con: net.net) {
    //         if (con.collisionCheck(*this, coord)) return con; // check connections
    //     }
    // }
    return {};
}

PortInst& Block::getPort(const PortRef& port) {
    switch (typeOf(port.ref)) {
    case VariantType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case VariantType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case VariantType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}
const PortInst& Block::getPort(const PortRef& port) const {
    switch (typeOf(port.ref)) {
    case VariantType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case VariantType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case VariantType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}

// Connection
bool Block::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}

bool Block::event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2i& mousePos) {
    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left) {
        switch (state) {
        case BlockState::Idle: { // new action begun
            connectionStart = mousePos;
            auto clickedObj = whatIsAtCoord(mousePos);
            if (clickedObj.index() < 3) { // matches NewConType enum
                conType = NewConType{clickedObj.index()};
            } else { // clickedObj was a VariantRef
                VariantRef ref = std::get<VariantRef>(clickedObj);
                if (typeOf(ref) == VariantType::Node) {
                    conType = NewConType::FromNode;
                }
            }

            break;
        }
        case BlockState::NewConStart: { // connection finished
            state = BlockState::Idle;
            break;
        }
        }
    }
    return false;
}

void Block::frame(sf::RenderWindow& window, const sf::Vector2i& mousePos) {}

void Block::draw(sf::RenderWindow& window) {
    auto circ = sf::CircleShape(nodeRad);
    circ.setOrigin({-nodeRad, -nodeRad});
}
