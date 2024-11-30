#include "Editor.hpp"
#include <SFML/Window/Event.hpp>
#include <imgui.h>

ObjAtCoordVar Editor::whatIsAtCoord(const sf::Vector2i& coord) {
    // check for nodes
    for (const auto& node: block.nodes) {
        if (coord == node.obj.pos) return node.ind;
    }
    // check connections
    std::vector<Connection> cons;
    for (const auto& net: block.conNet.nets) {
        for (const auto& portPair: net.obj.getMap()) {
            Connection con(portPair.first, portPair.second);
            if (collisionCheck(con, coord)) cons.push_back(con);
        }
    }
    if (cons.size() > 2)
        throw std::logic_error("Should never be more than 2 connections overlapping");
    else if (cons.size() == 2) {
        return std::make_pair(cons[0], cons[0]);
    } else if (cons.size() == 1) {
        return cons[0];
    }

    return {};
}

bool Editor::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, block.getPort(con.portRef1).portPos,
                        block.getPort(con.portRef2).portPos);
}

void Editor::checkConEndLegal() {
    conEndLegal = isValConTarget(conEndObjVar);
    if (conStartPos == conEndPos)
        conEndLegal = false;

    else if (conStartCloNet && conEndCloNet && conStartCloNet.value() == conEndCloNet.value()) {
        ImGui::SetTooltip("Connection proposes loop"); // recomendation only (for now)
    }
    if (!conEndLegal) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    }
}

// Returns ref to port at location
// if there isn't one creates one according to what's currently there;
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
        return std::get<PortRef>(var);
    }
    case ObjAtCoordType::Node: {
        Ref<Node> node = std::get<Ref<Node>>(var);
        return {node, static_cast<std::size_t>(reverseDirection(dirIntoPort))};
    }
    default:
        throw std::logic_error("Cannot make connection to location which isn't viable");
    }
}

sf::Vector2i Editor::snapToGrid(const sf::Vector2f& pos) const {
    return {std::clamp(static_cast<int>(std::round(pos.x)), 0, static_cast<int>(block.size - 1)),
            std::clamp(static_cast<int>(std::round(pos.y)), 0, static_cast<int>(block.size - 1))};
}

// Called every event
// Primarily responsible for excectution of actions
// E.g. create destroy objects
void Editor::event(const sf::Event& event, const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Left) {
        auto clickedObj = whatIsAtCoord(mouseGridPos);
        switch (state) {
        case BlockState::Idle: {             // new connection started
            if (isValConTarget(clickedObj)) { // connectable
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

            block.conNet.insert({startPort, endPort}, conStartCloNet, conEndCloNet);
            state = BlockState::Idle;
            conEndCloNet.reset();
            break;
        }
        }
    }
}

// Called every frame
// Responsible for ensuring correct state of "con" variables according to block state and inputs
void Editor::frame(const sf::Vector2f& mousePos) {
    sf::Vector2i mouseGridPos = snapToGrid(mousePos);
    switch (state) {
    case BlockState::Idle: {
        conStartPos    = mouseGridPos;
        conStartObjVar = whatIsAtCoord(conStartPos);
        // if network component highlight network
        switch (typeOf(conStartObjVar)) { // maybe make own function
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
            const PortInst& port{block.getPort(std::get<PortRef>(conStartObjVar))};
            conEndPos =
                dirToVec(port.portDir) * std::clamp(dot(dirToVec(port.portDir), diff), 0, INT_MAX);
            break;
        }
        case ObjAtCoordType::Node: { // if node port already in use in this direction don't update
            if (!block.conNet.getClosNetRef(PortRef{std::get<Ref<Node>>(conStartObjVar),
                                                    static_cast<std::size_t>(vecToDir(diff))})) {
                conEndPos = conStartPos + diff;
            }
            break; // TODO maybe change kinda klunky atm... even more so for con
        }
        case ObjAtCoordType::Con: {
            Direction dir = block.getPort(std::get<Connection>(conStartObjVar).portRef1).portDir;
            if (dot(diff, dirToVec(dir)) == 0) { // if not in connection direction update
                conEndPos = conStartPos + diff;
            }
            break; // else don't update
        }
        default:
            conEndPos = conStartPos + diff;
            break;
        }
        conEndObjVar = whatIsAtCoord(conEndPos);
        checkConEndLegal();
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
