#include "alica_sim/MessageBus.h"
#include "alica_sim/SimLogger.h"

#include <memory>

namespace alica::sim
{

namespace
{

struct Dispatch
{
    const AlicaCommunicationHandlers& h;

    void operator()(const AllocationAuthorityInfo& m) const { call(h.incomingAuthorityMessageHandler, m); }
    void operator()(const AlicaEngineInfo&) const {}
    void operator()(const PlanTreeInfo& m) const { callShared(h.planTreeInfohandler, m); }
    void operator()(const RoleSwitch&) const {}
    void operator()(const SyncReady& m) const { callShared(h.onSyncReadyHandler, m); }
    void operator()(const SyncTalk& m) const { callShared(h.onSyncTalkHandler, m); }
    void operator()(const SolverResult& m) const { call(h.onSolverResultHandler, m); }
    void operator()(const AgentQuery& m) const { call(h.agentQueryHandler, m); }
    void operator()(const AgentAnnouncement& m) const { call(h.agentAnnouncementHandler, m); }

    template <class F, class M>
    static void call(const F& f, const M& m)
    {
        if (f) {
            f(m);
        }
    }

    template <class F, class M>
    static void callShared(const F& f, const M& m)
    {
        if (f) {
            f(std::make_shared<M>(m));
        }
    }
};

} // namespace

int MessageBus::join(const AlicaCommunicationHandlers& handlers, const std::string& agentName)
{
    _members.push_back(std::make_shared<const Member>(Member{handlers, agentName}));
    return static_cast<int>(_members.size()) - 1;
}

void MessageBus::leave(int member)
{
    if (member >= 0 && member < static_cast<int>(_members.size())) {
        _members[member].reset();
    }
}

void MessageBus::post(int sender, AlicaMessage msg)
{
    _queue.emplace_back(sender, std::move(msg));
}

void MessageBus::deliver()
{
    std::deque<std::pair<int, AlicaMessage>> batch;
    batch.swap(_queue);
    for (const auto& [sender, msg] : batch) {
        for (size_t i = 0; i < _members.size(); ++i) {
            if (static_cast<int>(i) == sender || !_members[i]) {
                continue;
            }
            std::shared_ptr<const Member> member = _members[i];
            SimLogger::AgentScope scope(member->agentName);
            std::visit(Dispatch{member->handlers}, msg);
        }
    }
}

void SimCommunication::startCommunication()
{
    if (_member < 0) {
        _member = _bus.join(_callbacks, _agentName);
    }
}

void SimCommunication::stopCommunication()
{
    _bus.leave(_member);
    _member = -1;
}

void SimCommunication::post(AlicaMessage msg) const
{
    if (_member >= 0) {
        _bus.post(_member, std::move(msg));
    }
}

} // namespace alica::sim
