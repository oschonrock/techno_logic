#include "Editor.hpp"
#include <SFML/Window/Event.hpp>
#include <imgui.h>

ObjAtCoordVar Editor::whatIsAtCoord(const sf::Vector2i& coord) {
    for (const auto& node: block.nodes) {
        if (coord == node.obj.pos) return node.ind; // check for nodes
    }
    // for (const auto& net: conNetworks) {
    //     for (const auto& con: net.net) {
    //         if (con.collisionCheck(*this, coord)) return con; // check connections
    //     }
    // }
    return {};
}

PortInst& Editor::getPort(const PortRef& port) {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return block.nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return block.gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return block.blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}
const PortInst& Editor::getPort(const PortRef& port) const {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return block.nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return block.gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return block.blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}

// TODO potential problem when there are two collisions with connection
bool Editor::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}

void Editor::checkConLegal() {
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

[[nodiscard]] PortRef Editor::makeNewPortRef(const ObjAtCoordVar& var, const sf::Vector2i& pos,
                                             Direction dirIntoPort) {
    switch (typeOf(var)) {
    case ObjAtCoordType::Empty: { // make new node
        std::cout << "new node made" << std::endl;
        Ref<Node> node = block.nodes.insert(Node{pos});
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

sf::Vector2i Editor::snapToGrid(const sf::Vector2f& pos) const {
    return {std::clamp(static_cast<int>(std::round(pos.x)), 0, static_cast<int>(block.size - 1)),
            std::clamp(static_cast<int>(std::round(pos.y)), 0, static_cast<int>(block.size - 1))};
}

bool Editor::event(const sf::Event& event, const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Left) {
        auto clickedObj = whatIsAtCoord(mouseGridPos);
        switch (state) {
        case BlockState::Idle: {                              // new connection started
            if (typeOf(clickedObj) <= ObjAtCoordType::Node) { // connectable
                //
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
            block.conNet.insert({startPort, endPort}, isNewClosNet);
            break;
        }
        }
    }
    return false;
}

void Editor::frame(const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    switch (state) {
    case BlockState::Idle: {
        conStartPos    = mouseGridPos;
        conStartObjVar = whatIsAtCoord(conStartPos);
        // if network component highlight network
        switch (typeOf(conStartObjVar)) {
        case ObjAtCoordType::Node: {
            conStartCloNet = block.conNet.getClosNetRef(std::get<Ref<Node>>(conStartObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conStartCloNet =
                block.conNet.getClosNetRef(std::get<Connection>(conStartObjVar).portRef1);
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
            if (block.conNet.getClosNetRef(PortRef{std::get<Ref<Node>>(conStartObjVar),
                                                   static_cast<std::size_t>(vecToDir(diff))})) {
                break;
            } // else default
            conEndPos = conStartPos + diff;
            break;
        }
        default:
            conEndPos = conStartPos + diff;
            break;
        }
        conEndObjVar = whatIsAtCoord(conEndPos);
        checkConLegal();
        // if network component highlight network
        switch (typeOf(conEndObjVar)) {
        case ObjAtCoordType::Node: {
            conEndCloNet = block.conNet.getClosNetRef(std::get<Ref<Node>>(conEndObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conEndCloNet = block.conNet.getClosNetRef(std::get<Connection>(conEndObjVar).portRef1);
            break;
        }
        default:
            conEndCloNet.reset();
        }

        break;
    }
    }
}
