#include <imgui.h>

#include "Editor.hpp"

ObjAtCoordVar Editor::whatIsAtCoord(const sf::Vector2i& coord) const {
    // check for nodes
    for (const auto& node: block.nodes) {
        if (coord == node.obj.pos) return node.ind;
    }
    // check connections
    std::vector<Connection> cons;
    for (const auto& net: block.conNet.nets) {
        for (const auto& con: net.obj) {
            if (block.collisionCheck(con, coord)) cons.push_back(con);
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

// checks if pos is a legal final target for conccection
bool Editor::isPosLegalEnd(const sf::Vector2i& end) const {
    if (conStartPos == end) return true; // no-op case
    auto obj = whatIsAtCoord(end);
    if (!isCoordConType(obj)) {
        ImGui::SetTooltip("Target obj invalid");
        return false;
    }
    if (typeOf(obj) == ObjAtCoordType::Node) {
        auto node    = std::get<Ref<Node>>(obj);
        auto portNum = static_cast<std::size_t>(vecToDir(conStartPos - end));
        if (block.conNet.contains(PortRef{node, portNum})) {
            ImGui::SetTooltip("Node already has connection in this direction");
            return false;
        }
    }
    if (typeOf(obj) == ObjAtCoordType::Con) {
        auto con     = std::get<Connection>(obj);
        auto conDir  = block.getPort(con.portRef1).portDir;
        auto propDir = vecToDir(end - conStartPos);
        if (propDir == conDir || propDir == reverseDir(conDir)) {
            ImGui::SetTooltip("Illegal connection overlap");
            return false;
        }
    }
    for (const auto& node: block.nodes) {
        auto pos = node.obj.pos;
        if (isVecBetween(pos, conStartPos, end)) {
            ImGui::SetTooltip("Illegal connection overlap");
            return false;
        }
    }
    return true;
}

bool Editor::isPosLegalStart(const sf::Vector2i& start) const {
    auto obj = whatIsAtCoord(start);
    if (!isCoordConType(obj)) {
        ImGui::SetTooltip("Target obj invalid");
        return false;
    }
    if (typeOf(obj) == ObjAtCoordType::Node &&
        block.conNet.getNodeConCount(std::get<Ref<Node>>(obj)) == 4) {
        ImGui::SetTooltip("Node already has 4 connections");
        return false;
    }
    return true;
}

// Returns ref to port at location
// If there isn't one creates one according to what's currently there;
// Note takes var by ref and may invalidate it (in case of deleting redundant point)
[[nodiscard]] PortRef Editor::makeNewPortRef(ObjAtCoordVar& var, const sf::Vector2i& pos,
                                             Direction dirIntoPort) { // TODO swap port dir
    switch (typeOf(var)) {
    case ObjAtCoordType::Empty: { // make new node
        Ref<Node> node = block.nodes.insert(Node{pos});
        return {node, static_cast<std::size_t>(reverseDir(dirIntoPort))};
    }
    case ObjAtCoordType::Con: { // make new node and split connection
        auto node   = block.nodes.insert(Node{pos});
        auto oldCon = std::get<Connection>(var);
        assert(block.conNet.contains(oldCon) && !block.conNet.contains(node));
        auto&     net = block.conNet.nets[block.conNet.getClosNetRef(oldCon.portRef1).value()];
        Direction dir = vecToDir(block.nodes[node].pos - block.getPort(oldCon.portRef1).portPos);
        // TODO implement by swapping order and calling conNet instead
        net.erase(oldCon, block.getPortType(oldCon));
        auto con = Connection(oldCon.portRef1, PortRef{node, static_cast<std::size_t>(dir)});
        net.insert(con, block.getPortType(con));
        con = Connection(oldCon.portRef2, PortRef{node, static_cast<std::size_t>(reverseDir(dir))});
        net.insert(con, block.getPortType(con));
        return {node, static_cast<std::size_t>(reverseDir(dirIntoPort))};
    }
    case ObjAtCoordType::Port: { // return port
        return std::get<PortRef>(var);
    }
    case ObjAtCoordType::Node: { // if redundant delete node else return port
        Ref<Node> node = std::get<Ref<Node>>(var);
        PortRef   port{node, static_cast<std::size_t>(dirIntoPort)};
        auto      parralelPortNet = block.conNet.getClosNetRef(port);
        if (parralelPortNet && block.conNet.getNodeConCount(node) == 1) { // if node is redundant
            auto& net          = block.conNet.nets[parralelPortNet.value()];
            auto  redundantCon = net.getCon(port);
            net.erase(redundantCon, block.getPortType(redundantCon));
            block.nodes.erase(node);
            var = {}; // prevents deleted node ref being used
            return redundantCon.portRef2;
        }
        return {node, static_cast<std::size_t>(reverseDir(dirIntoPort))};
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
void Editor::event(const sf::Event& event, const sf::Vector2i& mousePos) {
    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Left) {
        auto clickedObj = whatIsAtCoord(mousePos);
        switch (state) {
        case EditorState::Idle: { // new connection started
            if (!conStartLegal) break;

            if (isCoordConType(clickedObj)) { // connectable
                state = EditorState::Connecting;
                // reset end... it will contain old data
                conEndPos    = conStartPos;
                conEndObjVar = conStartObjVar;
            } else { // clickedObj was a gate or block
                // TODO
            }
            break;
        }
        case EditorState::Connecting: { // connection finished
            if (!conEndLegal) break;
            if (conEndPos == conStartPos) { // no move click is reset
                state = EditorState::Idle;
                conEndCloNet.reset();
                break;
            }

            PortRef startPort =
                makeNewPortRef(conStartObjVar, conStartPos, vecToDir(conStartPos - conEndPos));
            PortRef endPort =
                makeNewPortRef(conEndObjVar, conEndPos, vecToDir(conEndPos - conStartPos));
            Connection con{startPort, endPort};

            block.conNet.insert(con, conStartCloNet, conEndCloNet, block.getPortType(con));
            state = EditorState::Idle;
            conEndCloNet.reset();
            break;
        }
        }
    }
}

// Called every frame
// Responsible for ensuring correct state of "con" variables according to block state and inputs
void Editor::frame(const sf::Vector2i& mousePos) {
    switch (state) {
    case EditorState::Idle: {
        conStartPos    = mousePos;
        conStartObjVar = whatIsAtCoord(conStartPos);
        conStartLegal  = isPosLegalStart(conStartPos);
        // if network component find network
        switch (typeOf(conStartObjVar)) { // TODO maybe make own function
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
    case EditorState::Connecting: {
        sf::Vector2i diff       = mousePos - conStartPos;
        sf::Vector2i newEndProp = conStartPos + snapToAxis(diff); // default
        conEndCloNet.reset();

        // work out proposed end point based on state
        switch (typeOf(conStartObjVar)) { // from 1, up to dot(diff, portDir)
        case ObjAtCoordType::Port: {
            const auto& port     = block.getPort(std::get<PortRef>(conStartObjVar));
            int         bestDist = std::clamp(dot(port.portDir, diff), 0, INT_MAX);
            newEndProp           = conStartPos + (dirToVec(port.portDir) * bestDist);
        }
        case ObjAtCoordType::Node: { // finds best available direction
            // TODO maybe should just be handled by legal check
            const auto& nodeRef  = std::get<Ref<Node>>(conStartObjVar);
            const auto& node     = block.nodes[nodeRef];
            auto        bestPort = std::max_element(
                node.ports.begin(), node.ports.end(), [&](const auto& max, const auto& elem) {
                    bool isElemPortInUse = block.conNet.contains(
                        PortRef(nodeRef, static_cast<std::size_t>(elem.portDir)));
                    bool isMaxPortInUse = block.conNet.contains(
                        PortRef(nodeRef, static_cast<std::size_t>(max.portDir)));
                    return isMaxPortInUse ||
                           (dot(max.portDir, diff) < dot(elem.portDir, diff) && !isElemPortInUse);
                });
            int bestDist = std::clamp(dot(bestPort->portDir, diff), 0, INT_MAX);
            newEndProp   = conStartPos + (dirToVec(bestPort->portDir) * bestDist);
            break;
        }
        case ObjAtCoordType::Con: {
            Direction oldConDir =
                block.getPort(std::get<Connection>(conStartObjVar).portRef1).portDir;
            Direction newConDir = swapXY(oldConDir);
            auto      dist      = dot(newConDir, diff);
            newEndProp          = conStartPos + (dirToVec(newConDir) * dist);
            break;
        }
        case ObjAtCoordType::Empty: {
            break;
        }
        default:
            throw std::logic_error(
                "Connection start from this object not implemented for this object yet");
            break;
        }

        if (!isPosLegalEnd(newEndProp)) { // if new position illegal
            conEndLegal = false;
            break;
        }
        conEndLegal  = true;
        conEndPos    = newEndProp;
        conEndObjVar = whatIsAtCoord(conEndPos);
        switch (typeOf(conEndObjVar)) { // if network component find closednet
        case ObjAtCoordType::Node: {
            conEndCloNet = block.conNet.getClosNetRef(std::get<Ref<Node>>(conEndObjVar));
            break;
        }
        case ObjAtCoordType::Con: {
            conEndCloNet = block.conNet.getClosNetRef(std::get<Connection>(conEndObjVar).portRef1);
            break;
        }
        default:
            break;
        }
        if (conStartPos != conEndPos && conStartCloNet && conEndCloNet &&
            conStartCloNet.value() == conEndCloNet.value()) {
            ImGui::SetTooltip("Connection proposes loop"); // recomendation only (for now)
        }
        break;
    }
    }
}
