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

void Block::checkConLegal() {
    conEndLegal = typeOf(conEndObjVar) <= ObjAtCoordType::Node;
    if (conStartPos == conEndPos)
        conEndLegal = false;

    else if (conStartCloNet && conEndCloNet &&
             conStartCloNet.value() == conEndCloNet.value()) { // recomendation
        ImGui::SetTooltip("Connection proposes loop");
    }
    if (!conEndLegal) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    }
}

[[nodiscard]] PortRef Block::makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                            Direction dirIntoPort) {
    switch (typeOf(var)) {
    case ObjAtCoordType::Empty: { // make new node
        std::cout << "new node made" << std::endl;
        Ref<Node> node = nodes.insert(Node{pos});
        return {node, static_cast<std::size_t>(reverseDirection(dirIntoPort))};
    }
    case ObjAtCoordType::Con: { // make new node and split connection
        break;
    }
    case ObjAtCoordType::Port: { // return portRef
        break;
    }
    case ObjAtCoordType::Node: { // return portRef
        Ref<Node> node = std::get<Ref<Node>>(var);
        return {node, static_cast<std::size_t>(reverseDirection(dirIntoPort))};
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
    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Left) {
        auto clickedObj = whatIsAtCoord(mouseGridPos);
        switch (state) {
        case BlockState::Idle: {                              // new connection started
            if (typeOf(clickedObj) <= ObjAtCoordType::Node) { // connectable
                state = BlockState::Connecting;
                // reset end... it will contain old data
                conEndPos    = conStartPos;
                conEndObjVar = conStartObjVar;
            } else { // clickedObj was a gate or block
                // TODO
            }
            break;
        }
        case BlockState::Connecting: { // connection finished
            if (!conEndLegal) break;

            PortRef startPort =
                makeNewPortRef(conStartObjVar, conStartPos, vecToDir(conStartPos - conEndPos));
            PortRef endPort =
                makeNewPortRef(conEndObjVar, conEndPos, vecToDir(conEndPos - conStartPos));

            // MAKING NEW NETWORK
            bool isNewClosNet = (!conStartCloNet && !conEndCloNet);

            state = BlockState::Idle;
            conEndCloNet.reset();
            conNet.insert({startPort, endPort}, isNewClosNet);
            break;
        }
        }
    }
    return false;
}

void Block::frame(sf::RenderWindow& window, const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    ImGui::Begin("Debug");
    ImGui::Text("Proposed connection %slegal", conEndLegal ? "" : "il");
    ImGui::Text("Number of nodes: %zu", nodes.size());
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
            conStartCloNet = conNet.getClosNetRef(std::get<Ref<Node>>(conStartObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conStartCloNet = conNet.getClosNetRef(std::get<Connection>(conStartObjVar).portRef1);
            break;
        }
        default:
            conStartCloNet.reset();
        }
        break;
    }
    case BlockState::Connecting: {
        sf::Vector2i diff = snapToAxis(mouseGridPos - conStartPos);
        if (diff == sf::Vector2i{}) {
            conEndLegal = false;
            break;
        }
        switch (typeOf(conStartObjVar)) {
        case ObjAtCoordType::Port: {
            const PortInst& port{getPort(std::get<PortRef>(conStartObjVar))};
            conEndPos =
                dirToVec(port.portDir) * std::clamp(dot(dirToVec(port.portDir), diff), 0, INT_MAX);
            break;
        }
        case ObjAtCoordType::Node: { // if node port already in use in this direction use old
            if (conNet.getClosNetRef(PortRef{std::get<Ref<Node>>(conStartObjVar),
                                             static_cast<std::size_t>(vecToDir(diff))})) {
                break;
            } // else default
        }
        default:
            conEndPos = conStartPos + diff;
            break;
        }
        conEndObjVar = whatIsAtCoord(conEndPos);
        checkConLegal();
        ImGui::Text("Connection started from %s at (%d,%d)",
                    ObjAtCoordStrings[conStartObjVar.index()].c_str(), conStartPos.x,
                    conStartPos.y);
        ImGui::Text("Hovering %s at (%d,%d)", ObjAtCoordStrings[conEndObjVar.index()].c_str(),
                    conEndPos.x, conEndPos.y);
        // if network component highlight network
        switch (typeOf(conEndObjVar)) {
        case ObjAtCoordType::Node: {
            conEndCloNet = conNet.getClosNetRef(std::get<Ref<Node>>(conEndObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conEndCloNet = conNet.getClosNetRef(std::get<Connection>(conEndObjVar).portRef1);
            break;
        }
        default:
            conEndCloNet.reset();
        }

        break;
    }
    }
    draw(window);
    ImGui::End();
}

void Block::draw(sf::RenderWindow& window) {
    if (state == BlockState::Connecting) { // draw new connection
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
    // draw connections
    std::vector<sf::Vertex> lineVerts;
    for (const auto& net: conNet.nets) {
        sf::Color col = conColour;
        if ((conStartCloNet && conStartCloNet.value() == net.ind) || // if needs highlighting
            (conEndCloNet && conEndCloNet.value() == net.ind)) {
            col = highlightConColour;
        }
        for (const auto& portPair: net.obj.map) {
            lineVerts.emplace_back(sf::Vector2f(getPort(portPair.first).portPos), col);
            lineVerts.emplace_back(sf::Vector2f(getPort(portPair.second).portPos), col);
        }
    }
    window.draw(lineVerts.data(), lineVerts.size(), sf::PrimitiveType::Lines);
    // draw nodes
    for (const auto& node: nodes) {
        ImGui::Text("Node connection count: %zu", conNet.getNodeConCount(node.ind));
        if (conNet.getNodeConCount(node.ind) != 2) {
            sf::CircleShape circ{nodeRad};
            circ.setFillColor(nodeColour);
            circ.setPosition(sf::Vector2f{node.obj.pos} - sf::Vector2f{nodeRad, nodeRad});
            window.draw(circ);
        }
    }
}
