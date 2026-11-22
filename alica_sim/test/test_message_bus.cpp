#include <alica_sim/MessageBus.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace alica::sim
{
namespace
{

struct Inbox
{
    std::vector<std::string> received;

    AlicaCommunicationHandlers handlers(const std::string& name)
    {
        AlicaCommunicationHandlers h;
        h.agentQueryHandler = [this, name](const AgentQuery& q) { received.push_back(name + ":query:" + std::to_string(q.senderID)); };
        h.planTreeInfohandler = [this, name](std::shared_ptr<PlanTreeInfo> p) { received.push_back(name + ":tree:" + std::to_string(p->senderID)); };
        return h;
    }
};

AgentQuery query(AgentId sender)
{
    AgentQuery q;
    q.senderID = sender;
    return q;
}

TEST(MessageBusTest, DeliversInSendOrderToEveryoneButTheSender)
{
    MessageBus bus;
    Inbox inbox;
    int a = bus.join(inbox.handlers("a"));
    int b = bus.join(inbox.handlers("b"));
    bus.join(inbox.handlers("c"));
    PlanTreeInfo tree;
    tree.senderID = 2;
    bus.post(a, query(1));
    bus.post(b, tree);
    EXPECT_TRUE(inbox.received.empty());
    bus.deliver();
    EXPECT_EQ(inbox.received, (std::vector<std::string>{"b:query:1", "c:query:1", "a:tree:2", "c:tree:2"}));
}

TEST(MessageBusTest, MessagesPostedDuringDeliveryWaitForTheNextCall)
{
    MessageBus bus;
    std::vector<std::string> received;
    int a = 0;
    int b = 0;
    AlicaCommunicationHandlers ha;
    ha.agentQueryHandler = [&](const AgentQuery& q) { received.push_back("a:" + std::to_string(q.senderID)); };
    AlicaCommunicationHandlers hb;
    hb.agentQueryHandler = [&](const AgentQuery& q) {
        received.push_back("b:" + std::to_string(q.senderID));
        bus.post(b, query(q.senderID + 10));
    };
    a = bus.join(ha);
    b = bus.join(hb);
    bus.post(a, query(1));
    bus.deliver();
    EXPECT_EQ(received, (std::vector<std::string>{"b:1"}));
    EXPECT_EQ(bus.pending(), 1u);
    bus.deliver();
    EXPECT_EQ(received, (std::vector<std::string>{"b:1", "a:11"}));
}

TEST(MessageBusTest, TeamsAreIsolatedAndLeftMembersHearNothing)
{
    MessageBus blue;
    MessageBus red;
    Inbox inbox;
    int b1 = blue.join(inbox.handlers("b1"));
    int b2 = blue.join(inbox.handlers("b2"));
    red.join(inbox.handlers("r1"));
    blue.post(b1, query(1));
    blue.leave(b2);
    blue.deliver();
    red.deliver();
    EXPECT_TRUE(inbox.received.empty());
    int b3 = blue.join(inbox.handlers("b3"));
    blue.post(b3, query(3));
    blue.deliver();
    red.deliver();
    EXPECT_EQ(inbox.received, (std::vector<std::string>{"b1:query:3"}));
}

TEST(MessageBusTest, SimCommunicationPostsOnlyWhileStarted)
{
    MessageBus bus;
    Inbox inbox;
    bus.join(inbox.handlers("listener"));
    SimCommunication comm(inbox.handlers("self"), bus);
    comm.sendAgentQuery(query(5));
    EXPECT_EQ(bus.pending(), 0u);
    comm.startCommunication();
    comm.sendAgentQuery(query(6));
    comm.sendRoleSwitch(RoleSwitch(), 6);
    bus.deliver();
    EXPECT_EQ(inbox.received, (std::vector<std::string>{"listener:query:6"}));
    comm.stopCommunication();
    comm.sendAgentQuery(query(7));
    EXPECT_EQ(bus.pending(), 0u);
}

} // namespace
} // namespace alica::sim
