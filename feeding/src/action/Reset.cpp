#include "feeding/action/Reset.hpp"
#include "feeding/action/MoveAbovePlate.hpp"
#include "feeding/util.hpp"

#include <libada/util.hpp>

static const std::vector<std::string> optionPrompts{"(1) success", "(2) fail"};
const static std::vector<std::string> trajectoryController{"rewd_trajectory_controller"};
const static std::vector<std::string> ftTrajectoryController{"move_until_touch_topic_controller"};

namespace feeding {
namespace action {

//==============================================================================
bool reset(
	const std::shared_ptr<ada::Ada>& ada,
	const aikido::constraint::dart::CollisionFreePtr& collisionFree,
	const Eigen::Isometry3d& plate,
	const Eigen::Isometry3d& plateEndEffectorTransform,
	double horizontalToleranceAbovePlate,
	double verticalToleranceAbovePlate,
	double rotationToleranceAbovePlate,
	double planningTimeout,
	double endEffectorOffsetPositionTolerance,
	double endEffectorOffsetAngularTolerance,
	int maxNumTrials,
	std::vector<double> velocityLimits,
  const std::shared_ptr<FTThresholdHelper>& ftThresholdHelper)
{
  for (int i = 0; i < 2; i++)
  {
    if (ftThresholdHelper)
    {
      ftThresholdHelper->setThresholds(1, 1);
    }

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
      talk("Sorry, I'm having a little trouble moving. Mind if I get a little help?");
      ROS_WARN_STREAM("Move above plate failed. Please restart");
      return false;
    }

    if (i == 1)
    {
      ROS_INFO_STREAM("Twist");
      Eigen::VectorXd twists(6);
      twists << 0, 0, M_PI, 0, 0, 0;
      auto init_traj = ada->planWithEndEffectorTwist(
          twists,
          1,
          collisionFree,
          planningTimeout,
          endEffectorOffsetPositionTolerance,
          endEffectorOffsetAngularTolerance);
      ada->moveArmOnTrajectory(init_traj, collisionFree, ada::KUNZ, velocityLimits);
    }

    ROS_INFO_STREAM("Move to the bar");
    bool moveXSuccess = ada->moveArmToEndEffectorOffset(
        Eigen::Vector3d(1, 0, 0),
        0.1,
        collisionFree,
        planningTimeout,
        endEffectorOffsetPositionTolerance,
        endEffectorOffsetAngularTolerance);
    if (!moveXSuccess)
    {
      if (ftThresholdHelper)
      {
        ROS_WARN_STREAM("Stop FT, start Traj Controller");
        ada->getTrajectoryExecutor()->cancel();
        bool result = ada->switchControllers(trajectoryController, ftTrajectoryController);
        if (!result)
        {
          ROS_WARN_STREAM("Failed to switch; continue with FT controller");
          ftThresholdHelper->setThresholds(AFTER_GRAB_FOOD_FT_THRESHOLD);
        }
      }
      bool moveBack = ada->moveArmToEndEffectorOffset(
          Eigen::Vector3d(-1, 0, 0),
          0.001,
          collisionFree,
          planningTimeout,
          endEffectorOffsetPositionTolerance,
          endEffectorOffsetAngularTolerance);
      if (!moveBack)
      {
        talk("Sorry, I'm having a little trouble moving");
        ROS_WARN_STREAM("Move to the bar failed");
        return false;
      }
      if (ftThresholdHelper)
      {
        ROS_WARN_STREAM("Start FT, stop Traj Controller");
        bool result = ada->switchControllers(ftTrajectoryController, trajectoryController);
        if (!result)
        {
          ROS_WARN_STREAM("Failed to switch; continue with traj controller");
        }
        else
        {
          ftThresholdHelper->setThresholds(STANDARD_FT_THRESHOLD);
        }
      }
    }
    bool moveZSuccess = ada->moveArmToEndEffectorOffset(
        Eigen::Vector3d(0, 0, 1),
        0.03,
        collisionFree,
        planningTimeout,
        endEffectorOffsetPositionTolerance,
        endEffectorOffsetAngularTolerance);
    if (!moveZSuccess)
    {
      talk("Sorry, I'm having a little trouble moving");
      ROS_WARN_STREAM("Move to the bar failed");
      return false;
    }
  }
  ROS_INFO_STREAM("Successful");
  return true;
}

}
}
