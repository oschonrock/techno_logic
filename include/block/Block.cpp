#include "block/Block.hpp"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Window/Event.hpp>
#include <imgui.h>

ObjAtCoordType typeOf(const ObjAtCoordVar& ref) { return ObjAtCoordType{ref.index()}; }

// Block
ObjAtCoordVar Block::whatIsAtCoord(const sf::Vector2i& coord) {
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

// TODO potential problem when there are two collisions with connection
bool Block::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}

[[nodiscard]] PortRef Block::makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                            Direction dirIntoPort) {
    switch (typeOf(var)) {
    case ObjAtCoordType::Empty: { // make new node
        Ref<Node> node = nodes.insert(Node{pos});
        return {node, static_cast<std::size_t>(reverseDirection(dirIntoPort))};
        break;
    }
    case ObjAtCoordType::Con: { // make new node and split connection
        break;
    }
    case ObjAtCoordType::Port: { // return portRef
        break;
    }
    case ObjAtCoordType::Node: { // return portRef
        break;
    }
    default:
        throw std::logic_error("Cannot make connection to location which isn't");
    }
}

sf::Vector2i Block::snapToGrid(const sf::Vector2f& pos) const {
    return {std::clamp(static_cast<int>(std::round(pos.x)), 0, static_cast<int>(size - 1)),
            std::clamp(static_cast<int>(std::round(pos.y)), 0, static_cast<int>(size - 1))};
}

bool Block::event(const sf::Event& event, sf::RenderWindow& window, const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    if (event.type ==
            sf::Event::MouseButtonPressed && // maybe should be release and if view has not moved
        event.mouseButton.button == sf::Mouse::Left) {
        auto clickedObj = whatIsAtCoord(mouseGridPos);
        switch (state) {
        case BlockState::Idle: {                              // new connection started
            if (typeOf(clickedObj) <= ObjAtCoordType::Node) { // connectable
                state = BlockState::NewConStart;
                // reset end... it will contain old data
                conEndPos    = conStartPos;
                conEndObjVar = conStartObjVar;
            } else { // clickedObj was a gate or block
                // TODO
            }
            break;
        }
        case BlockState::NewConStart: {                    // connection finished
            if (typeOf(clickedObj) > ObjAtCoordType::Node) // not connectable
                break;

            PortRef startPort =
                makeNewPortRef(conStartObjVar, conStartPos, vecToDir(conStartPos - conEndPos));
            PortRef endPort =
                makeNewPortRef(conEndObjVar, conEndPos, vecToDir(conEndPos - conStartPos));
            std::cout << "There are " << nodes.size() << "node(s)" << std::endl;
            state = BlockState::Idle;
            conEndCloNet.reset();
            conNet.insert({startPort, endPort});
            std::cout << "made it here" << std::endl;
            break;
        }
        }
    }
    return false;
}

void Block::frame(sf::RenderWindow& window, const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    ImGui::Begin("Debug");
    ImGui::Text("Number of closed networks: %zu", conNet.nets.size());
    switch (state) {
    case BlockState::Idle: {
        conStartPos    = mouseGridPos;
        conStartObjVar = whatIsAtCoord(conStartPos);
        ImGui::Text("Hovering: %s at (%d,%d)", ObjAtCoordStrings[conStartObjVar.index()].c_str(),
                    conStartPos.x, conStartPos.y);
        // if network component highlight network
        switch (typeOf(conStartObjVar)) {
        case ObjAtCoordType::Node: {
            conStartCloNet = conNet.getNet(std::get<Ref<Node>>(conStartObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conStartCloNet = conNet.getNet(std::get<Connection>(conStartObjVar).portRef1);
            break;
        }
        default:
            conStartCloNet.reset();
        }
        break;
    }
    case BlockState::NewConStart: {
        conEndPos    = conStartPos + snapToAxis(mouseGridPos - conStartPos);
        conEndObjVar = whatIsAtCoord(conEndPos);
        ImGui::Text("Connection started from %s at (%d,%d)",
                    ObjAtCoordStrings[conStartObjVar.index()].c_str(), conStartPos.x,
                    conStartPos.y);
        ImGui::Text("Hovering %s at (%d,%d)", ObjAtCoordStrings[conEndObjVar.index()].c_str(),
                    conEndPos.x, conEndPos.y);
        // if network component highlight network
        switch (typeOf(conStartObjVar)) {
        case ObjAtCoordType::Node: {
            conEndCloNet = conNet.getNet(std::get<Ref<Node>>(conEndObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conEndCloNet = conNet.getNet(std::get<Connection>(conEndObjVar).portRef1);
            break;
        }
        default:
            conEndCloNet.reset();
        }
        break;
    }
    }
    ImGui::End();
    draw(window);
}

void Block::draw(sf::RenderWindow& window) {
    if (state == BlockState::NewConStart) {
        std::array<sf::Vertex, 2> conLine{sf::Vertex{sf::Vector2f{conStartPos}, newConColour},
                                          sf::Vertex{sf::Vector2f{conEndPos}, newConColour}};
        window.draw(conLine.data(), 2, sf::PrimitiveType::Lines);
        switch (typeOf(conStartObjVar)) {
        case ObjAtCoordType::Con:
        case ObjAtCoordType::Empty: {
            sf::CircleShape newNode{1.5f * nodeRad};
            newNode.setFillColor(newConColour);
            newNode.setPosition(sf::Vector2f{conStartPos} -
                                (1.5f * sf::Vector2f{nodeRad, nodeRad}));
            window.draw(newNode);
        }
        default: // errors
            break;
        }
    }
}
