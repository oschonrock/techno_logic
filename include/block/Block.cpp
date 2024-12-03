#include "Block.hpp"

// Node
Node::Node(const sf::Vector2i& pos_) : pos(pos_) {
    ports[0] = {Direction::up, pos};
    ports[1] = {Direction::down, pos};
    ports[2] = {Direction::left, pos};
    ports[3] = {Direction::right, pos};
}

// ClosedNet
void ClosedNet::maintainIOVecs(bool isInsert, const PortRef& portRef, const PortType& portType) {
    if (portType == PortType::node) return;
    if (portType == PortType::input) {
        if (isInsert) {
            if (input) throw std::logic_error("Tried to add two inputs to closed graph");
        } else {
            input.reset();
        }
    } else {
        if (isInsert) {
            outputs.push_back(portRef);
        } else {
            auto it = std::find(outputs.begin(), outputs.end(), portRef);
            if (it != outputs.end())
                throw std::logic_error("Tried to remove output that isn't in closed graph");
            std::erase(outputs, *it);
        }
    }
}

void ClosedNet::insert(const Connection& con, const std::pair<PortType, PortType>& portTypes) {
    conMap.insert({con.portRef1, con.portRef2});
    conMap2.insert({con.portRef2, con.portRef1});
    ++size;
    maintainIOVecs(true, con.portRef1, portTypes.first);
    maintainIOVecs(true, con.portRef2, portTypes.second);
}

void ClosedNet::erase(const Connection& con, const std::pair<PortType, PortType>& portTypes) {
    conMap.erase(con.portRef1);
    conMap.erase(con.portRef2);
    conMap2.erase(con.portRef1);
    conMap2.erase(con.portRef2);
    --size;
    maintainIOVecs(false, con.portRef1, portTypes.first);
    maintainIOVecs(false, con.portRef2, portTypes.second);
}

[[nodiscard]] bool ClosedNet::contains(const PortRef& port) const {
    return conMap.contains(port) || conMap2.contains(port);
}

[[nodiscard]] bool ClosedNet::contains(const Connection& con) const {
    return (conMap.contains(con.portRef1) && conMap.find(con.portRef1)->second == con.portRef2) ||
           (conMap.contains(con.portRef2) && conMap.find(con.portRef2)->second == con.portRef1);
}

// prefer call contains() on port
[[nodiscard]] bool ClosedNet::contains(const Ref<Node> node) const {
    return contains(PortRef{node, 0}) || contains(PortRef{node, 1}) || contains(PortRef{node, 2}) ||
           contains(PortRef{node, 3});
}

[[nodiscard]] Connection ClosedNet::getCon(const PortRef& port) const {
    if (conMap.contains(port)) {
        return Connection{port, conMap.find(port)->second};
    } else if (conMap2.contains(port)) {
        return Connection{port, conMap2.find(port)->second};
    }
    throw std::logic_error("Port not connected to closed net. Did you call contains?");
}

// ConnectionNetwork
// void ConnectionNetwork::copyConnectedPorts(Block& block, const ClosedNet& sourceNet,
//                                            ClosedNet& destNet, const PortRef& port) {
//     if (destNet.contains(port)) return;
//     switch (typeOf(port)) {
//         case ObjAtCoordType:
//     }
// }

void ConnectionNetwork::insert(const Connection& con, const std::optional<Ref<ClosedNet>>& net1,
                               const std::optional<Ref<ClosedNet>>& net2,
                               const std::pair<PortType, PortType>& portTypes) {
    if (!net1 && !net2) { // make new closed network
        auto netRef = nets.insert(ClosedNet{});
        nets[netRef].insert(con, portTypes);
        return;
    }
    if (net1 && net2) { // connecting two existing networks
        auto net1Ref = net1.value();
        auto net2Ref = net2.value();
        if (net1Ref == net2Ref) { // making loop within closed network
            nets[net1Ref].insert(con, portTypes);
            return;
        } else { // connecting two closed networks
            if (nets[net1Ref].getSize() < nets[net2Ref].getSize()) std::swap(net1Ref, net2Ref);
            // net1 is now the bigger of the two
            nets[net1Ref] += nets[net2Ref];
            nets.erase(net2Ref);
            nets[net1Ref].insert(con, portTypes);
            return;
        }
    }
    // extending network
    nets[net1 ? net1.value() : net2.value()].insert(con, portTypes);
    return;
}

[[nodiscard]] std::size_t ConnectionNetwork::getNodeConCount(const Ref<Node>& node) const {
    const auto& net   = nets[getClosNetRef(node).value()]; // connection assumed because node
    std::size_t count = 0;
    for (std::size_t port = 0; port < 4; ++port) {
        if (net.contains(PortRef{node, port})) ++count;
    }
    return count;
}

// Block
PortInst& Block::getPort(const PortRef& port) {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}
const PortInst& Block::getPort(const PortRef& port) const {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}

PortType Block::getPortType(const PortRef& port) const {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return PortType::node;
    case PortObjType::Gate:
        throw std::runtime_error("Block::getPortType not implemented for Gate yet");
    case PortObjType::BlockInst:
        throw std::runtime_error("Block::getPortType not implemented for BlockInst yet");
    }
}

std::pair<PortType, PortType> Block::getPortType(const Connection& con) const {
    return std::make_pair(getPortType(con.portRef1), getPortType(con.portRef1));
}

bool Block::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}

void Block::splitCon(const Connection& oldCon, Ref<Node> node) {
    assert(conNet.contains(oldCon) && !conNet.contains(node));
    auto&     net = conNet.nets[conNet.getClosNetRef(oldCon.portRef1).value()];
    Direction dir = vecToDir(nodes[node].pos - getPort(oldCon.portRef1).portPos);
    // TODO implement by swapping order and calling conNet instead
    net.erase(oldCon, getPortType(oldCon));
    auto con = Connection(oldCon.portRef1, PortRef{node, static_cast<std::size_t>(dir)});
    net.insert(con, getPortType(con));
    con = Connection(oldCon.portRef2, PortRef{node, static_cast<std::size_t>(reverseDir(dir))});
    net.insert(con, getPortType(con));
}

void Block::makeOverlapNodes(const Connection& con, Ref<ClosedNet> netRef) {
    std::vector<std::pair<Connection, sf::Vector2i>> intersections{};
    auto&                                            net          = conNet.nets[netRef];
    std::pair<sf::Vector2i, sf::Vector2i>            conPositions = {getPort(con.portRef1).portPos,
                                                                     getPort(con.portRef2).portPos};
    for (const auto& netCon: net) {
        std::pair<sf::Vector2i, sf::Vector2i> netConPositions{getPort(netCon.portRef1).portPos,
                                                              getPort(netCon.portRef2).portPos};
        auto intersec = getLineIntersection(conPositions, netConPositions);
        if (intersec) intersections.emplace_back(netCon, intersec.value());
    }
    for (const auto& intersec: intersections) {
        std::cout << "Intersection found at (" << intersec.second.x << "," << intersec.second.y
                  << ")\n";
        auto netCon = intersec.first;
        auto node   = nodes.insert(Node{intersec.second});
        splitCon(netCon, node);
        splitCon(con, node);
    }
}

// Returns ref to port at location
// If there isn't one creates one according to what's currently there;
// Note takes var by ref and may invalidate it (in case of deleting redundant point)
[[nodiscard]] PortRef Block::makeNewPortRef(ObjAtCoordVar& var, const sf::Vector2i& pos,
                                            Direction dirIntoPort) { // TODO swap port dir
    switch (typeOf(var)) {
    case ObjAtCoordType::Empty: { // make new node
        Ref<Node> node = nodes.insert(Node{pos});
        return {node, static_cast<std::size_t>(reverseDir(dirIntoPort))};
    }
    case ObjAtCoordType::Con: { // make new node and split connection
        auto node   = nodes.insert(Node{pos});
        auto oldCon = std::get<Connection>(var);
        splitCon(oldCon, node);
        return {node, static_cast<std::size_t>(reverseDir(dirIntoPort))};
    }
    case ObjAtCoordType::Port: { // return port
        return std::get<PortRef>(var);
    }
    case ObjAtCoordType::Node: { // if redundant delete node else return port
        Ref<Node> node = std::get<Ref<Node>>(var);
        PortRef   port{node, static_cast<std::size_t>(dirIntoPort)};
        auto      parralelPortNet = conNet.getClosNetRef(port);
        if (parralelPortNet && conNet.getNodeConCount(node) == 1) { // if node is redundant
            auto& net          = conNet.nets[parralelPortNet.value()];
            auto  redundantCon = net.getCon(port);
            net.erase(redundantCon, getPortType(redundantCon));
            nodes.erase(node);
            var = {}; // prevents deleted node ref being used
            return redundantCon.portRef2;
        }
        return {node, static_cast<std::size_t>(reverseDir(dirIntoPort))};
    }
    default:
        throw std::logic_error("Cannot make connection to location which isn't viable");
    }
}

ObjAtCoordVar Block::whatIsAtCoord(const sf::Vector2i& coord) const {
    // check for nodes
    for (const auto& node: nodes) {
        if (coord == node.obj.pos) return node.ind;
    }
    // check connections
    std::vector<Connection> cons;
    for (const auto& net: conNet.nets) {
        for (const auto& con: net.obj) {
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