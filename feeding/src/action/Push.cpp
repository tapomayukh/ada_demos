#include "feeding/action/Push.hpp"
#include "feeding/AcquisitionAction.hpp"
#include "feeding/action/DetectAndMoveAboveFood.hpp"
#include "feeding/action/Grab.hpp"
#include "feeding/action/MoveAbovePlate.hpp"
#include "feeding/action/MoveInto.hpp"
#include "feeding/action/MoveOutOf.hpp"
#include "feeding/util.hpp"

#include <aikido/statespace/dart/MetaSkeletonStateSpace.hpp>
#include <aikido/trajectory/Interpolated.hpp>
#include <libada/util.hpp>
#include "aikido/robot/util.hpp"
#include "aikido/trajectory/util.hpp"

#include <iostream>
#include <vector>
#include <Eigen/Dense>
#include <math.h>

using ada::util::waitForUser;
static const std::vector<std::string> optionPrompts{"(1) success", "(2) fail"};

using namespace std;
using namespace Eigen;
using aikido::trajectory::concatenate;
using aikido::trajectory::Interpolated;
using aikido::trajectory::TrajectoryPtr;
using State = aikido::statespace::dart::MetaSkeletonStateSpace::State;
using aikido::statespace::dart::MetaSkeletonStateSpace;
// using magi::solution::PlanSolution;
using ada::util::createIsometry;

namespace feeding {
namespace action {

//==============================================================================
bool push(
    const std::shared_ptr<ada::Ada>& ada,
    const aikido::constraint::dart::CollisionFreePtr& collisionFree,
    const Eigen::Isometry3d& plate,
    const Eigen::Isometry3d& plateEndEffectorTransform,
    double horizontalToleranceAbovePlate,
    double verticalToleranceAbovePlate,
    double rotationToleranceAbovePlate,
    double planningTimeout,
    int maxNumTrials,
    std::vector<double> velocityLimits)
{
  // ===== MOVE ABOVE PLATE =====
  ROS_INFO_STREAM("Move above plate");
  bool abovePlaceSuccess = moveAbovePlate(
      ada,
      collisionFree,
      plate,
      plateEndEffectorTransform,
      horizontalToleranceAbovePlate,
      verticalToleranceAbovePlate,
      rotationToleranceAbovePlate,
      planningTimeout,
      maxNumTrials,
      velocityLimits);

  if (!abovePlaceSuccess)
  {
    // talk("Sorry, I'm having a little trouble moving. Mind if I get a little
    // help?");
    ROS_WARN_STREAM("Move above plate failed. Please restart");
    return false;
  }
  else
  {
    std::cout << "Move above Place Success" << std::endl;
    // talk("Move above Place Success", true);
  }

  // ===== PUSH FOOD ====
  // if (!waitForUser("Push food"))
  // {
  //   return 0;
  // }
}

} // namespace action
} // namespace action
