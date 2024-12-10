#include "Block.hpp"

// Block
bool Block::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}

void Block::splitCon(const Connection& oldCon, Ref<Node> node) {
    assert(contains(oldCon));
    auto&     net = nets[getClosNetRef(oldCon.portRef1).value()];
    Direction dir = vecToDir(getPort(oldCon.portRef1).portPos - nodes[node].pos);
    // TODO implement by calling insert first
    net.erase(oldCon, getPortType(oldCon));
    auto con = Connection(oldCon.portRef1, PortRef{node, static_cast<std::size_t>(dir)});
    assert(!contains(con.portRef2));
    net.insert(con, getPortType(con));
    con = Connection(oldCon.portRef2, PortRef{node, static_cast<std::size_t>(reverseDir(dir))});
    assert(!contains(con.portRef2));
    net.insert(con, getPortType(con));
}

PortInst& Block::getPort(const PortRef& port) {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
    throw std::logic_error("Port type not handled in getPort");
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
    throw std::logic_error("Port type not handled in getPort");
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
    throw std::logic_error("Port type not handled in getPortType");
}

std::pair<PortType, PortType> Block::getPortType(const Connection& con) const {
    return std::make_pair(getPortType(con.portRef1), getPortType(con.portRef1));
}

ObjAtCoordVar Block::whatIsAtCoord(const sf::Vector2i& coord) const {
    // check for nodes
    for (const auto& node: nodes) {
        if (coord == node.obj.pos) return node.ind;
    }
    // check connections
    std::vector<Connection> cons;
    for (const auto& net: nets) {
        for (const auto& con: net.obj) {
            if (collisionCheck(con, coord)) cons.push_back(con);
        }
    }
    if (cons.size() > 2)
        throw std::logic_error("Should never be more than 2 connections overlapping");
    else if (cons.size() == 2) {
        return std::make_pair(cons[0], cons[1]);
    } else if (cons.size() == 1) {
        return cons[0];
    }
    return {};
}

[[nodiscard]] std::size_t Block::getNodeConCount(const Ref<Node>& node) const {
    auto ref = getClosNetRef(node);
    if (!ref) return 0;
    const auto& net   = nets[ref.value()]; // connection assumed because node
    std::size_t count = 0;
    for (std::size_t port = 0; port < 4; ++port) {
        if (net.contains(PortRef{node, port})) ++count;
    }
    return count;
}

void Block::insertCon(const Connection& con, const std::optional<Ref<ClosedNet>>& net1,
                      const std::optional<Ref<ClosedNet>>& net2) {
    if (getPort(con.portRef1).portDir != reverseDir(getPort(con.portRef2).portDir))
        throw std::logic_error("attempted to insert non opposing ports");
    auto portTypes = getPortType(con);
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

void Block::insertOverlap(const Connection& con1, const Connection& con2, const sf::Vector2i& pos) {
    auto node = nodes.insert(Node(pos));
    splitCon(con1, node);
    auto con1Net = getClosNetRef(con1.portRef1);
    auto con2Net = getClosNetRef(con2.portRef1);
    if (con1Net.value() == con2Net.value()) {
        splitCon(con2, node);
    } else {
        nets[con2Net.value()].erase(
            con2, getPortType(con2)); // no need for net split as will be reconnected
        auto    con2P1Dir = vecToDir(getPort(con2.portRef1).portPos - pos);
        PortRef nodePortRef{node, static_cast<std::size_t>(con2P1Dir)};
        insertCon({nodePortRef, con2.portRef1}, con2Net, con1Net);
        auto newNetRef = getClosNetRef(nodePortRef);
        insertCon({{node, static_cast<std::size_t>(reverseDir(con2P1Dir))}, con2.portRef2},
                  newNetRef, newNetRef);
    }
}

void Block::eraseCon(const Connection& con) {
    auto netRef = getClosNetRef(con);
    if (!netRef.has_value()) throw std::logic_error("tried to delete non existant con");
    auto& net = nets[netRef.value()];
    net.erase(con, getPortType(con));
    if (!net.isConnected(con.portRef1, con.portRef2)) { // network split
        std::cout << "nets split";
        auto newNet = net.splitNet(con.portRef1);
        if (newNet.getSize() != 0) { // add new net
            std::cout << "new net added \n";
            nets.insert(newNet);
        }
        if (net.getSize() == 0) { // delete old net
            std::cout << "old net deleted \n";
            nets.erase(netRef.value());
        }
        // delete now disconnected nodes
        if (typeOf(con.portRef1) == PortObjType::Node) {
            auto node = std::get<Ref<Node>>(con.portRef1.ref);
            if (getNodeConCount(node) == 0) {
                nodes.erase(node);
            }
        }
        if (typeOf(con.portRef2) == PortObjType::Node) {
            auto node = std::get<Ref<Node>>(con.portRef2.ref);
            if (getNodeConCount(node) == 0) {
                nodes.erase(node);
            }
        }
    }
}

// Returns ref to port at location
// If there isn't one creates one according to what's currently there;
// Note takes var by ref and may invalidate it (in case of deleting redundant point)
[[nodiscard]] PortRef Block::makeNewPortRef(const sf::Vector2i& pos, Direction portDir) {
    ObjAtCoordVar var = whatIsAtCoord(pos);
    switch (typeOf(var)) {
    case ObjAtCoordType::Empty: { // make new node
        Ref<Node> node = nodes.insert(Node{pos});
        return {node, static_cast<std::size_t>(portDir)};
    }
    case ObjAtCoordType::Con: { // make new node and split connection
        auto node   = nodes.insert(Node{pos});
        auto oldCon = std::get<Connection>(var);
        splitCon(oldCon, node);
        return {node, static_cast<std::size_t>(portDir)};
    }
    case ObjAtCoordType::Port: { // return port
        return std::get<PortRef>(var);
    }
    case ObjAtCoordType::Node: { // if redundant delete node else return port
        Ref<Node> node = std::get<Ref<Node>>(var);
        PortRef   newPort{node, static_cast<std::size_t>(portDir)};
        if (contains(newPort))
            throw std::logic_error("makeNewPortRef... port direction already connected");
        PortRef redundantPort{node, static_cast<std::size_t>(reverseDir(portDir))};
        auto    redundantPortNet = getClosNetRef(redundantPort);
        if (redundantPortNet && getNodeConCount(node) == 1) { // if node is redundant
            auto& net          = nets[redundantPortNet.value()];
            auto  redundantCon = net.getCon(redundantPort);
            net.erase(redundantCon, getPortType(redundantCon));
            nodes.erase(node);
            var = {}; // prevents deleted node ref being used
            return redundantCon.portRef2;
        }
        return newPort;
    }
    default:
        throw std::logic_error("Cannot make connection to location which isn't viable");
    }
}