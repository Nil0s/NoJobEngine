/** @file TrainingTelemetry.h @brief Read-only per-step data for external training viewers. */
#pragma once
#include "ObstacleLearning.h"
namespace NoJob::RL {
/** @brief A copy of an actual environment transition, never an illustrative replay. */
struct TrainingStep {
    int phase=0,episode=0,step=0,action=0,collisions=0;
    GridCell position,goal;
    float reward=0,totalReward=0,epsilon=1,loss=0;
    bool terminal=false,obstacles=true;
    float axisPosition=0,axisGoal=0;
    std::vector<float> observations,q;
};
}
