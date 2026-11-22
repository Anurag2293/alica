{
  "blackboard": [],
  "comment": "",
  "entryPoints": [
    {
      "comment": "",
      "id": 5665057711436701927,
      "isDynamic": false,
      "maxCardinality": 2147483647,
      "minCardinality": 0,
      "name": "",
      "plan": 6097175671294860706,
      "positionWeb": {
        "x": 100,
        "y": 100
      },
      "state": 895379419706551432,
      "successRequired": false,
      "task": "TaskRepository.tsk#8140138606383906905"
    }
  ],
  "frequency": 0,
  "id": 6097175671294860706,
  "implementationName": "SimTestPlan",
  "inheritBlackboard": false,
  "isInterface": false,
  "libraryName": "alica_sim_test_team",
  "masterPlan": true,
  "name": "TestMaster",
  "preCondition": null,
  "runtimeCondition": null,
  "states": [
    {
      "comment": "",
      "confAbstractPlanWrappers": [
        {
          "abstractPlan": "SucceedFirst.beh#2002112507878850417",
          "comment": "",
          "configuration": null,
          "id": 4460914957621015401,
          "keyMapping": {
            "input": [],
            "output": []
          },
          "name": ""
        }
      ],
      "entryPoint": 5665057711436701927,
      "id": 895379419706551432,
      "inTransitions": [],
      "name": "Start",
      "outTransitions": [
        3423026927451420749
      ],
      "parentPlan": 6097175671294860706,
      "positionWeb": {
        "x": 300,
        "y": 100
      },
      "type": "State",
      "variableBindings": []
    },
    {
      "comment": "",
      "confAbstractPlanWrappers": [
        {
          "abstractPlan": "Walk.beh#4423979581029728602",
          "comment": "",
          "configuration": null,
          "id": 385341329421918254,
          "keyMapping": {
            "input": [],
            "output": []
          },
          "name": ""
        }
      ],
      "entryPoint": null,
      "id": 9021064231331676884,
      "inTransitions": [
        3423026927451420749
      ],
      "name": "Move",
      "outTransitions": [
        2506994189846189017
      ],
      "parentPlan": 6097175671294860706,
      "positionWeb": {
        "x": 520,
        "y": 100
      },
      "type": "State",
      "variableBindings": []
    },
    {
      "comment": "",
      "confAbstractPlanWrappers": [
        {
          "abstractPlan": "Rest.beh#4828889529646951272",
          "comment": "",
          "configuration": null,
          "id": 7772305115759220500,
          "keyMapping": {
            "input": [],
            "output": []
          },
          "name": ""
        }
      ],
      "entryPoint": null,
      "id": 1442066625669275451,
      "inTransitions": [
        2506994189846189017
      ],
      "name": "Done",
      "outTransitions": [],
      "parentPlan": 6097175671294860706,
      "positionWeb": {
        "x": 740,
        "y": 100
      },
      "type": "State",
      "variableBindings": []
    }
  ],
  "synchronisations": [],
  "transitions": [
    {
      "comment": "",
      "condition": "ConditionRepository.cnd#546003087934931146",
      "id": 3423026927451420749,
      "inState": 895379419706551432,
      "keyMapping": {
        "input": [],
        "output": []
      },
      "name": "",
      "outState": 9021064231331676884,
      "pointsWeb": [],
      "preCondition": {
        "comment": "",
        "conditionString": "",
        "enabled": true,
        "id": 6470428469806614510,
        "name": "",
        "quantifiers": [],
        "variables": []
      },
      "synchronisation": null
    },
    {
      "comment": "",
      "condition": "ConditionRepository.cnd#1491996900913883048",
      "id": 2506994189846189017,
      "inState": 9021064231331676884,
      "keyMapping": {
        "input": [],
        "output": []
      },
      "name": "",
      "outState": 1442066625669275451,
      "pointsWeb": [],
      "preCondition": {
        "comment": "",
        "conditionString": "",
        "enabled": true,
        "id": 3845939151815028655,
        "name": "",
        "quantifiers": [],
        "variables": []
      },
      "synchronisation": null
    }
  ],
  "utilityThreshold": 0.1,
  "variables": []
}
