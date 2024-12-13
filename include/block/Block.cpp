#include "Block.hpp"

// Block
bool Block::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}

// Returns ref to port at location
// If there isn't one creates one according to what's currently there;
// should only really be used when making a new connection
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

void Block::insertCon(const Connection& con) {
    if (getPort(con.portRef1).portDir != reverseDir(getPort(con.portRef2).portDir))
        throw std::logic_error("attempted to insert non opposing ports");

    std::optional<Ref<ClosedNet>> net1{};
    if (typeOf(con.portRef1) == PortObjType::Node) {
        net1 = getClosNetRef(std::get<Ref<Node>>(con.portRef1.ref));
    }
    std::optional<Ref<ClosedNet>> net2{};
    if (typeOf(con.portRef2) == PortObjType::Node) {
        net2 = getClosNetRef(std::get<Ref<Node>>(con.portRef2.ref));
    }
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

void Block::splitCon(const Connection& oldCon, Ref<Node> nodeRef) {
    assert(contains(oldCon));
    assert(collisionCheck(oldCon, nodes[nodeRef].pos));
    auto& net = nets[getClosNetRef(oldCon).value()];
    net.erase(oldCon, getPortType(oldCon));
    auto dir = getPort(oldCon.portRef1).portDir;
    auto con =
        Connection(oldCon.portRef1, PortRef{nodeRef, static_cast<std::size_t>(reverseDir(dir))});
    net.insert(con, getPortType(con));
    con = Connection(oldCon.portRef2, PortRef{nodeRef, static_cast<std::size_t>(dir)});
    net.insert(con, getPortType(con));
}

// PortInst& Block::getPort(const PortRef& port) {
//     switch (typeOf(port.ref)) {
//     case PortObjType::Node:
//         return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
//     case PortObjType::Gate:
//         return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
//     case PortObjType::BlockInst:
//         return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
//     }
//     throw std::logic_error("Port type not handled in getPort");
// }

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

Connection Block::addConnection(const sf::Vector2i& startPos, const sf::Vector2i& endPos) {
    PortRef    startPort = makeNewPortRef(startPos, vecToDir(endPos - startPos));
    PortRef    endPort   = makeNewPortRef(endPos, vecToDir(startPos - endPos));
    Connection con{startPort, endPort};
    insertCon(con);
    return con;
}

void Block::insertOverlap(const Connection& con1, const Connection& con2, const sf::Vector2i& pos) {
    auto node    = nodes.insert(Node(pos));
    auto con1Net = getClosNetRef(con1).value();
    auto con2Net = getClosNetRef(con2).value();
    splitCon(con1, node);
    if (con1Net != con2Net) {
        // join nets
        // code copied from insertCon
        // can't just call it because erasing of con2 could make nodes float
        if (nets[con1Net].getSize() < nets[con2Net].getSize()) std::swap(con1Net, con2Net);
        nets[con1Net] += nets[con2Net];
        nets.erase(con2Net);
    }
    splitCon(con2, node);
}

void Block::eraseCon(const Connection& con) {
    auto netRef = getClosNetRef(con);
    if (!netRef.has_value()) throw std::logic_error("tried to delete non existant con");
    auto& net = nets[netRef.value()];
    net.erase(con, getPortType(con));
    if (!net.isConnected(con.portRef1, con.portRef2)) { // network split
        std::cout << "nets split" << std::endl;
        auto newNet = net.splitNet(con.portRef1);
        if (newNet.getSize() != 0) { // add new net
            std::cout << "new net added" << std::endl;
            nets.insert(newNet);
            net = nets[netRef.value()]; // net& invalidated by insert
        }
        if (net.getSize() == 0) { // delete old net
            std::cout << "old net deleted" << std::endl;
            nets.erase(netRef.value());
        }
        std::cout << "erase finisehed" << std::endl;
        // delete now disconnected nodes
        if (typeOf(con.portRef1) == PortObjType::Node) {
            auto node = std::get<Ref<Node>>(con.portRef1.ref);
            if (getNodeConCount(node) == 0) {
                std::cout << "erase finisehed" << std::endl;
                nodes.erase(node);
            }
        }
        if (typeOf(con.portRef2) == PortObjType::Node) {
            auto node = std::get<Ref<Node>>(con.portRef2.ref);
            if (getNodeConCount(node) == 0) {
                nodes.erase(node);
            }
        }
        std::cout << "erase func finisehed" << std::endl;
    }
}