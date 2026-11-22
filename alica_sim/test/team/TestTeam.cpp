#include "../TestIO.h"

#include <engine/BasicBehaviour.h>
#include <engine/BasicPlan.h>
#include <engine/BasicUtilityFunction.h>
#include <engine/RunningPlan.h>
#include <engine/blackboard/Blackboard.h>
#include <engine/model/State.h>

#include <boost/dll/alias.hpp>

#include <any>
#include <memory>

namespace alica_sim_test
{

TestIO& io(const alica::Blackboard& globalBlackboard)
{
    alica::LockedBlackboardRO bb(globalBlackboard);
    return *std::any_cast<TestIO*>(bb.get<std::any>("io"));
}

class LoggingBehaviour : public alica::BasicBehaviour
{
public:
    LoggingBehaviour(alica::BehaviourContext& context, double speed)
            : BasicBehaviour(context)
            , _speed(speed)
    {
    }

protected:
    void log(const std::string& what)
    {
        io(*getGlobalBlackboard()).events.emplace_back(getName() + "." + what, getPlanContext()->getAlicaClock().now().inNanoseconds());
    }

    void initialiseParameters() override
    {
        log("init");
        io(*getGlobalBlackboard()).vx = _speed;
    }

    double _speed;
};

class SucceedFirst : public LoggingBehaviour
{
public:
    SucceedFirst(alica::BehaviourContext& context)
            : LoggingBehaviour(context, 0)
    {
    }
    void run() override
    {
        log("run");
        setSuccess();
    }
    static std::unique_ptr<SucceedFirst> create(alica::BehaviourContext& context) { return std::make_unique<SucceedFirst>(context); }
};

class Walk : public LoggingBehaviour
{
public:
    Walk(alica::BehaviourContext& context)
            : LoggingBehaviour(context, 1)
    {
    }
    void run() override {}
    static std::unique_ptr<Walk> create(alica::BehaviourContext& context) { return std::make_unique<Walk>(context); }
};

class Rest : public LoggingBehaviour
{
public:
    Rest(alica::BehaviourContext& context)
            : LoggingBehaviour(context, 0)
    {
    }
    void run() override {}
    static std::unique_ptr<Rest> create(alica::BehaviourContext& context) { return std::make_unique<Rest>(context); }
};

class SimTestPlan : public alica::BasicPlan
{
public:
    SimTestPlan(alica::PlanContext& context)
            : BasicPlan(context)
    {
    }
    static std::unique_ptr<SimTestPlan> create(alica::PlanContext& context) { return std::make_unique<SimTestPlan>(context); }
};

bool ChildSucceeded(const alica::Blackboard*, const alica::RunningPlan* rp, const alica::Blackboard*)
{
    for (const alica::RunningPlan* child : rp->getChildren()) {
        if (child->getStatus() == alica::PlanStatus::Success) {
            return true;
        }
    }
    return false;
}

bool OneSecondPassed(const alica::Blackboard*, const alica::RunningPlan* rp, const alica::Blackboard*)
{
    return rp->getAlicaClock().now() >= alica::AlicaTime::seconds(1);
}

} // namespace alica_sim_test

BOOST_DLL_ALIAS(alica_sim_test::SucceedFirst::create, SucceedFirst)
BOOST_DLL_ALIAS(alica_sim_test::Walk::create, Walk)
BOOST_DLL_ALIAS(alica_sim_test::Rest::create, Rest)
BOOST_DLL_ALIAS(alica_sim_test::SimTestPlan::create, SimTestPlan)
BOOST_DLL_ALIAS(alica::BasicUtilityFunction::create, SimTestPlanUtilityFunction)
BOOST_DLL_ALIAS(alica_sim_test::ChildSucceeded, ChildSucceeded)
BOOST_DLL_ALIAS(alica_sim_test::OneSecondPassed, OneSecondPassed)
