#ifndef FEEDING_ACTION_SKEWER_HPP_
#define FEEDING_ACTION_SKEWER_HPP_

#include <libada/Ada.hpp>
#include "feeding/FTThresholdHelper.hpp"
#include "feeding/perception/Perception.hpp"
#include "feeding/Workspace.hpp"

namespace feeding {
namespace action {

void skewer(
  const std::shared_ptr<ada::Ada>& ada,
  const std::shared_ptr<Workspace>& workspace,
  const aikido::constraint::dart::CollisionFreePtr& collisionFree,
  const std::shared_ptr<Perception>& perception,
  const ros::NodeHandle* nodeHandle,
  const std::string& foodName,
  const Eigen::Isometry3d& plate,
  const Eigen::Isometry3d& plateEndEffectorTransform,
  const std::unordered_map<std::string, double>& foodSkeweringForces,
  double heightAbovePlate,
  double horizontalToleranceAbovePlate,
  double verticalToleranceAbovePlate,
  double rotationToleranceAbovePlate,
  double heightAboveFood,
  double horizontalToleranceForFood,
  double verticalToleranceForFood,
  double rotationToleranceForFood,
  double tiltToleranceForFood,
  double moveOutofFoodLength,
  double endEffectorOffsetPositionTolerance,
  double endEffectorOffsetAngularTolerance,
  std::chrono::milliseconds waitTimeForFood,
  double planningTimeout,
  int maxNumTrials,
  std::vector<double> velocityLimits,
  const std::shared_ptr<FTThresholdHelper>& ftThresholdHelper);

}
}

#endif