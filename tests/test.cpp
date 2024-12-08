#include <ranges>

#include "gtest/gtest.h"

#include "block/Block.hpp"
#include "details/StableVector.hpp"

// Block
class BlockTest : public testing::Test {
  protected:
    Block block{"test", 50};
};

TEST_F(BlockTest, constructor) {
    EXPECT_TRUE(block.nodes.empty());
    EXPECT_TRUE(block.nets.empty());
    EXPECT_TRUE(block.blockInstances.empty());
}

TEST_F(BlockTest, makeNewPortAtEmpty) {
    sf::Vector2i pos        = {21, 21};
    auto         emptyPoint = block.whatIsAtCoord(pos);
    EXPECT_EQ(ObjAtCoordType::Empty, typeOf(emptyPoint));
    auto newPort = block.makeNewPortRef(pos, Direction::up);
    EXPECT_EQ(PortObjType::Node, typeOf(newPort));
    EXPECT_EQ(Direction::up, Direction(newPort.portNum));
    auto nodePoint = block.whatIsAtCoord(pos);
    EXPECT_EQ(ObjAtCoordType::Node, typeOf(nodePoint));
    auto nodeRef = std::get<Ref<Node>>(nodePoint);
    EXPECT_EQ(nodeRef, std::get<Ref<Node>>(newPort.ref));
    EXPECT_EQ(block.nodes.size(), 1);
    EXPECT_TRUE(block.nodes.contains(nodeRef));
}

TEST_F(BlockTest, makeEmptyToEmptyCon) {
    sf::Vector2i pos        = {21, 21};
    auto         firstPort  = block.makeNewPortRef(pos, Direction::up);
    sf::Vector2i secondPos  = {21, 13};
    auto         secondPort = block.makeNewPortRef(secondPos, Direction::up); // wrong dir
    EXPECT_ANY_THROW(block.insertCon({firstPort, secondPort}, {}, {}));
    secondPort = block.makeNewPortRef(secondPos, Direction::down); // right dir
    Connection con{firstPort, secondPort};
    block.insertCon(con, {}, {});
    auto net = block.getClosNetRef(con);
    ASSERT_TRUE(net.has_value());
    EXPECT_EQ(net.value(), block.getClosNetRef(firstPort).value());
    EXPECT_EQ(net.value(), block.getClosNetRef(secondPort).value());
    auto conPoint = block.whatIsAtCoord((secondPos + pos) / 2);
    EXPECT_EQ(ObjAtCoordType::Con, typeOf(conPoint));
    EXPECT_EQ(std::get<Connection>(conPoint), con);
}

TEST_F(BlockTest, makeNewPortAtNode) {
    sf::Vector2i startPos  = {0, 0};
    auto         startPort = block.makeNewPortRef(startPos, Direction::down);
    EXPECT_EQ(startPort, block.makeNewPortRef(startPos, Direction::down)); // not made yet so ok
    auto       endPort = block.makeNewPortRef({0, 10}, Direction::up);
    Connection con{startPort, endPort};
    block.insertCon(con, {}, {});
    EXPECT_ANY_THROW(std::ignore = block.makeNewPortRef(startPos, Direction::down));
    auto       startPort2 = block.makeNewPortRef(startPos, Direction::right);
    auto       endPort2   = block.makeNewPortRef({10, 0}, Direction::left);
    Connection con2{startPort2, endPort2};
    block.insertCon(con2, block.getClosNetRef(startPort), {});
    EXPECT_EQ(block.nets.size(), 1);
    EXPECT_EQ(block.nodes.size(), 3);
    EXPECT_EQ(block.getNodeConCount(std::get<Ref<Node>>(startPort.ref)), 2);
    auto& net = *block.nets.begin();
    EXPECT_TRUE(net.obj.isConnected(endPort, endPort2));
}

TEST_F(BlockTest, splitNode) {
    PortRef    con1A = block.makeNewPortRef({0, 0}, Direction::right);
    PortRef    con1B = block.makeNewPortRef({5, 0}, Direction::left);
    Connection con1  = {con1A, con1B};
    block.insertCon(con1, {}, {});
    PortRef    con2B = block.makeNewPortRef({5, 5}, Direction::left);
    Connection con2{block.makeNewPortRef({0, 5}, Direction::right), con2B};
    block.insertCon(con2, {}, {});
    auto       net1          = block.getClosNetRef(con1).value();
    auto       net2          = block.getClosNetRef(con2).value();
    PortRef    splitConPort1 = block.makeNewPortRef({2, 0}, Direction::down);
    Connection splitCon{splitConPort1, block.makeNewPortRef({2, 5}, Direction::up)};
    EXPECT_EQ(block.nodes.size(), 6);
    EXPECT_FALSE(block.nets[net1].contains(con1));
    EXPECT_TRUE(block.nets[net1].contains(
        {con1A, {splitConPort1.ref, static_cast<size_t>(Direction::left)}}));
    EXPECT_TRUE(block.nets[net1].contains(
        {con1B, {splitConPort1.ref, static_cast<size_t>(Direction::right)}}));
    EXPECT_TRUE(block.nets[net1].isConnected(con1A, con1B));
    block.insertCon(splitCon, net1, net2);
    EXPECT_EQ(block.nets.size(), 1);
    EXPECT_TRUE(block.nets.begin()->obj.contains(splitCon));
    EXPECT_TRUE(block.nets.begin()->obj.isConnected(con1A, con2B));
}

TEST_F(BlockTest, overlapNode) {
    PortRef    con1A = block.makeNewPortRef({0, 2}, Direction::right);
    PortRef    con1B = block.makeNewPortRef({5, 2}, Direction::left);
    Connection con1  = {con1A, con1B};
    block.insertCon(con1, {}, {});
    PortRef    con2A = block.makeNewPortRef({2, 0}, Direction::down);
    PortRef    con2B = block.makeNewPortRef({2, 5}, Direction::up);
    Connection con2  = {con2A, con2B};
    block.insertCon(con2, {}, {});
    EXPECT_EQ(block.nets.size(), 2);
    EXPECT_EQ(block.nodes.size(), 4);
    block.insertOverlap(con1, con2, {2, 2});
    EXPECT_EQ(block.nets.size(), 1);
    EXPECT_EQ(block.nodes.size(), 5);
    auto& net  = block.nets.begin()->obj;
    auto  node = std::get<Ref<Node>>(block.whatIsAtCoord({2, 2}));
    EXPECT_TRUE(net.contains({con1A, {node, static_cast<std::size_t>(Direction::left)}}));
    EXPECT_TRUE(net.contains({con1B, {node, static_cast<std::size_t>(Direction::right)}}));
    EXPECT_TRUE(net.contains({con2A, {node, static_cast<std::size_t>(Direction::up)}}));
    EXPECT_TRUE(net.contains({con2B, {node, static_cast<std::size_t>(Direction::down)}}));
    EXPECT_TRUE(net.isConnected(con1A, con2B));
}

TEST_F(BlockTest, eraseConSplitNet) {
    PortRef    con1A = block.makeNewPortRef({0, 0}, Direction::right);
    PortRef    con1B = block.makeNewPortRef({5, 0}, Direction::left);
    Connection con1  = {con1A, con1B};
    block.insertCon(con1, {}, {});
    PortRef    con2A = block.makeNewPortRef({0, 5}, Direction::right);
    PortRef    con2B = block.makeNewPortRef({5, 5}, Direction::left);
    Connection con2  = {con2A, con2B};
    block.insertCon(con2, {}, {});
    PortRef    con3A = block.makeNewPortRef({5, 0}, Direction::down);
    PortRef    con3B = block.makeNewPortRef({5, 5}, Direction::up);
    Connection con3  = {con3A, con3B};
    block.insertCon(con3, block.getClosNetRef(con1), block.getClosNetRef(con2));
    auto& net = block.nets[block.getClosNetRef(con3).value()];
    EXPECT_TRUE(net.isConnected(con1A, con2B));
    EXPECT_EQ(block.nodes.size(), 4);
    EXPECT_EQ(block.nets.size(), 1);
    block.eraseCon(con3);
    EXPECT_EQ(block.nets.size(), 2);
    EXPECT_FALSE(block.getClosNetRef(con3).has_value());
    auto& net1 = block.nets[block.getClosNetRef(con1).value()];
    EXPECT_TRUE(net1.isConnected(con1A, con1B));
    EXPECT_FALSE(net1.isConnected(con1A, con3B));
    auto& net2 = block.nets[block.getClosNetRef(con2).value()];
    EXPECT_TRUE(net2.isConnected(con2A, con2B));
    EXPECT_FALSE(net1.isConnected(con2A, con3A));
}

// StableVector
template <typename T, typename Q>
void equalityCheck(T subj, const std::vector<Q>& vec) { // subj is taken by value
    EXPECT_EQ(subj.size(), vec.size());
    for (auto v: vec) {
        EXPECT_TRUE(subj.contains(v.first));
        EXPECT_TRUE(subj[v.first] == v.second);
        subj.erase(v.first);
    }
    EXPECT_EQ(subj.size(), 0);
}

template <typename T>
class StableVectors : public testing::Test {
  public:
    T vec;
};

using TestedTypes = testing::Types<PepperedVector<int>, CompactMap<int>>;
TYPED_TEST_SUITE(StableVectors, TestedTypes);

TYPED_TEST(StableVectors, IsEmptyIntially) { EXPECT_TRUE(this->vec.empty()); }

TYPED_TEST(StableVectors, IterateEmpty) {
    auto& vec = this->vec;
    EXPECT_EQ(vec.begin(), vec.end());
    for (auto& _: vec) EXPECT_TRUE(false);
    for (const auto& _: vec) EXPECT_TRUE(false);
    auto ref = vec.insert(0);
    vec.erase(ref);
    ASSERT_TRUE(vec.empty());
    EXPECT_EQ(vec.begin(), vec.end());
    for (auto& _: vec) EXPECT_TRUE(false);
    for (const auto& _: vec) EXPECT_TRUE(false);
}

TYPED_TEST(StableVectors, Adding) {
    auto& vec     = this->vec;
    auto  newItem = vec.insert(0);
    EXPECT_TRUE(vec.contains(newItem));
    EXPECT_EQ(vec[newItem], 0);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_FALSE(vec.empty());

    std::vector<std::pair<decltype(newItem), int>> curr{{newItem, 0}};
    for (auto i: std::views::iota(1, 10)) {
        curr.emplace_back(vec.insert(i), i);
    }
    equalityCheck(vec, curr);
}

TYPED_TEST(StableVectors, Iterating) {
    auto& vec = this->vec;
    for (auto i: std::views::iota(0, 10)) vec.insert(i);

    int sum{};
    for (const auto& i: vec) {
        EXPECT_EQ(i.obj, vec[i.ind]);
        sum += i.obj;
    }
    EXPECT_EQ(sum, 45);
}

TYPED_TEST(StableVectors, RemovingSimple) {
    auto& vec     = this->vec;
    auto  newItem = vec.insert(0);
    vec.erase(newItem);
    EXPECT_FALSE(vec.contains(newItem));
    EXPECT_EQ(vec.size(), 0);
    newItem = vec.insert(0);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_TRUE(vec.contains(newItem));
    auto newItem2 = vec.insert(1);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_TRUE(vec.contains(newItem));
    EXPECT_TRUE(vec.contains(newItem2));
}

TYPED_TEST(StableVectors, RemovingAndAdding) {
    auto& vec     = this->vec;
    auto  newItem = vec.insert(0);
    vec.erase(newItem);

    std::vector<std::pair<decltype(newItem), int>> curr{};
    for (auto i: std::views::iota(0, 10)) {
        curr.emplace_back(vec.insert(i), i);
    }
    equalityCheck(vec, curr); // curr = 0,1,2,3,4,5,6,7,8,9

    for (auto i: std::vector<std::size_t>{7, 4, 3, 5, 5}) {
        vec.erase(curr.at(i).first);
        curr.erase(curr.begin() + static_cast<signed long long>(i));
    }
    equalityCheck(vec, curr); // curr = 0,1,2,5,6

    for (auto i: std::views::iota(10, 15)) {
        curr.emplace_back(vec.insert(i), i);
    }
    equalityCheck(vec, curr); // curr = 0,1,2,5,6,10,11,12,13,14

    std::vector<decltype(newItem)> removes;
    for (auto i: std::vector<std::size_t>{9, 0, 3, 2, 5}) {
        removes.push_back(curr.at(i).first);
        curr.erase(curr.begin() + static_cast<long long int>(i));
    }
    vec.erase(removes);
    equalityCheck(vec, curr); // cur = 1,2,10,11,12

    for (int i: std::views::iota(0, 201)) {
        curr.emplace_back(vec.insert(i), i);
    }
    equalityCheck(vec, curr);

    int sum = std::accumulate(vec.begin(), vec.end(), 0,
                              [](auto total, const auto& e) { return total += e.obj; });
    EXPECT_EQ(sum, 20136);

    removes = {};
    for (auto i: curr) {
        removes.push_back(i.first);
    }
    vec.erase(removes);
    EXPECT_EQ(vec.size(), 0);
}