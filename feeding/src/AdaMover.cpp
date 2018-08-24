#include "feeding/AdaMover.hpp"
#include <aikido/constraint/TestableIntersection.hpp>
#include "feeding/util.hpp"
#include "aikido/robot/util.hpp"

namespace feeding {

//==============================================================================
AdaMover::AdaMover(
    ada::Ada& ada,
    aikido::statespace::dart::MetaSkeletonStateSpacePtr armSpace,
    aikido::constraint::dart::CollisionFreePtr collisionFreeConstraint,
    ros::NodeHandle nodeHandle)
  : mAda(ada)
  , mArmSpace(armSpace)
  , mCollisionFreeConstraint(collisionFreeConstraint)
  , mNodeHandle(nodeHandle)
{
}

//==============================================================================
bool AdaMover::moveArmToTSR(const aikido::constraint::dart::TSR& tsr)
{
  auto goalTSR = std::make_shared<aikido::constraint::dart::TSR>(tsr);

  ROS_WARN_STREAM("Collision constraint for moveArmToTSR: " << mCollisionFreeConstraint.get());
  auto trajectory = mAda.planToTSR(
      mArmSpace,
      mAda.getArm()->getMetaSkeleton(),
      mAda.getHand()->getEndEffectorBodyNode(),
      goalTSR,
      mCollisionFreeConstraint,
      getRosParam<double>("/planning/timeoutSeconds", mNodeHandle),
      getRosParam<int>("/planning/maxNumberOfTrials", mNodeHandle));

  return moveArmOnTrajectory(trajectory);
}

//==============================================================================
bool AdaMover::moveToEndEffectorOffset(
    const Eigen::Vector3d& direction, double length)
{
  return moveArmOnTrajectory(
      planToEndEffectorOffset(direction, length), TRYOPTIMALRETIME);
}

//==============================================================================
bool AdaMover::moveWithEndEffectorTwist(
    const Eigen::Vector6d& transform, double duration, double timelimit)
{
  return moveArmOnTrajectory(
      planWithEndEffectorTwist(transform, duration, timelimit), RETIME);
}

//==============================================================================
aikido::trajectory::TrajectoryPtr AdaMover::planToEndEffectorOffset(
    const Eigen::Vector3d& direction, double length)
{
  ROS_INFO_STREAM("Plan to end effector offset state: " << mAda.getArm()->getMetaSkeleton()->getPositions().matrix().transpose());
  ROS_INFO_STREAM("Plan to end effector offset direction: " << direction.matrix().transpose() << ",  length: " << length);

  return mAda.planToEndEffectorOffset(
      mArmSpace,
      mAda.getArm()->getMetaSkeleton(),
      mAda.getHand()->getEndEffectorBodyNode(),
      nullptr,
      direction,
      length,
      getRosParam<double>("/planning/timeoutSeconds", mNodeHandle),
      getRosParam<double>(
          "/planning/endEffectorOffset/positionTolerance", mNodeHandle),
      getRosParam<double>(
          "/planning/endEffectorOffset/angularTolerance", mNodeHandle));
}

//==============================================================================
aikido::trajectory::TrajectoryPtr AdaMover::planWithEndEffectorTwist(
    const Eigen::Vector6d& transform, double duration, double timelimit)
{

    aikido::robot::util::VectorFieldPlannerParameters vfParams(
      0.2, 0.001, 0.004, 0.001, 1e-3, 1e-3, 1.0, 0.2, 0.1);

  return aikido::robot::util::planWithEndEffectorTwist(
    mArmSpace,
    mAda.getArm()->getMetaSkeleton(),
    mAda.getHand()->getEndEffectorBodyNode(),
    transform,
    duration,
    nullptr,
    timelimit,
    1, 10, vfParams);
}

//==============================================================================
bool AdaMover::moveArmToConfiguration(const Eigen::Vector6d& configuration)
{
  auto trajectory = mAda.planToConfiguration(
      mArmSpace,
      mAda.getArm()->getMetaSkeleton(),
      configuration,
      mCollisionFreeConstraint,
      getRosParam<double>("/planning/timeoutSeconds", mNodeHandle));

  return moveArmOnTrajectory(trajectory, SMOOTH);
}

//==============================================================================
bool AdaMover::moveArmOnTrajectory(
    aikido::trajectory::TrajectoryPtr trajectory,
    TrajectoryPostprocessType postprocessType)
{
  if (!trajectory)
  {
    throw std::runtime_error("Trajectory execution failed: Empty trajectory.");
  }

  std::vector<aikido::constraint::ConstTestablePtr> constraints;
  if (mCollisionFreeConstraint)
  {
    constraints.push_back(mCollisionFreeConstraint);
  }
  auto testable = std::make_shared<aikido::constraint::TestableIntersection>(
      mArmSpace, constraints);

  aikido::trajectory::TrajectoryPtr timedTrajectory;
  switch (postprocessType)
  {
    case RETIME:
      timedTrajectory
          = mAda.retimePath(mAda.getArm()->getMetaSkeleton(), trajectory.get());
      break;

    case SMOOTH:
      timedTrajectory = mAda.smoothPath(
          mAda.getArm()->getMetaSkeleton(), trajectory.get(), testable);
      break;

    case TRYOPTIMALRETIME:
      timedTrajectory = mAda.retimeTimeOptimalPath(
          mAda.getArm()->getMetaSkeleton(), trajectory.get());

      if (!timedTrajectory)
      {
        // If using time-optimal retining failed, back to parabolic timing
        timedTrajectory = mAda.retimePath(
            mAda.getArm()->getMetaSkeleton(), trajectory.get());
      }
      break;

    default:
      throw std::runtime_error(
          "Feeding demo: Unexpected trajectory post processing type!");
  }

  auto future = mAda.executeTrajectory(std::move(timedTrajectory));
  try
  {
    future.get();
  }
  catch (const std::exception& e)
  {
    ROS_INFO_STREAM("trajectory execution failed: " << e.what());
    return false;
  }
  return true;
}
}
