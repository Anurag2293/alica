#pragma once

#include <engine/IAlicaCommunication.h>
#include <engine/containers/AgentAnnouncement.h>
#include <engine/containers/AgentQuery.h>
#include <engine/containers/AlicaEngineInfo.h>
#include <engine/containers/AllocationAuthorityInfo.h>
#include <engine/containers/PlanTreeInfo.h>
#include <engine/containers/RoleSwitch.h>
#include <engine/containers/SolverResult.h>
#include <engine/containers/SyncReady.h>
#include <engine/containers/SyncTalk.h>

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace alica::sim
{

using AlicaMessage =
        std::variant<AllocationAuthorityInfo, AlicaEngineInfo, PlanTreeInfo, RoleSwitch, SyncReady, SyncTalk, SolverResult, AgentQuery, AgentAnnouncement>;

class MessageBus
{
public:
    int join(const AlicaCommunicationHandlers& handlers, const std::string& agentName = "");
    void leave(int member);
    void post(int sender, AlicaMessage msg);
    void deliver();

    size_t pending() const { return _queue.size(); }

private:
    struct Member
    {
        AlicaCommunicationHandlers handlers;
        std::string agentName;
    };

    std::vector<std::shared_ptr<const Member>> _members;
    std::deque<std::pair<int, AlicaMessage>> _queue;
};

class SimCommunication : public IAlicaCommunication
{
public:
    SimCommunication(const AlicaCommunicationHandlers& handlers, MessageBus& bus, std::string agentName = "")
            : IAlicaCommunication(handlers)
            , _bus(bus)
            , _agentName(std::move(agentName))
    {
    }

    void sendAllocationAuthority(const AllocationAuthorityInfo& m) const override { post(m); }
    void sendAlicaEngineInfo(const AlicaEngineInfo& m) const override { post(m); }
    void sendPlanTreeInfo(const PlanTreeInfo& m) const override { post(m); }
    void sendRoleSwitch(const RoleSwitch& m, AgentId) const override { post(m); }
    void sendSyncReady(const SyncReady& m) const override { post(m); }
    void sendSyncTalk(const SyncTalk& m) const override { post(m); }
    void sendSolverResult(const SolverResult& m) const override { post(m); }
    void sendAgentQuery(const AgentQuery& m) const override { post(m); }
    void sendAgentAnnouncement(const AgentAnnouncement& m) const override { post(m); }

    void startCommunication() override;
    void stopCommunication() override;

private:
    void post(AlicaMessage msg) const;

    MessageBus& _bus;
    std::string _agentName;
    int _member = -1;
};

} // namespace alica::sim
