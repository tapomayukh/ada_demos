#ifndef FEEDING_FEEDINGDEMO_HPP_
#define FEEDING_FEEDINGDEMO_HPP_

#include <aikido/planner/World.hpp>
#include <ros/ros.h>
#include <libada/Ada.hpp>
#include "feeding/Workspace.hpp"

namespace feeding {

enum TrajectoryPostprocessType
{
  RETIME,
  SMOOTH,
};

/// The FeedingDemo class is responsible for
/// - The robot (loading + control)
/// - The workspace
///
/// It contains not only generalized functions, for example moveArmToTSR(...),
/// but also some functions that are very specialized for the feeding demo,
/// like moveInFrontOfPerson(). It uses the robot, workspace info and ros
/// parameters
/// to accomplish these tasks.
class FeedingDemo
{

public:
  FeedingDemo(bool adaReal, ros::NodeHandle nodeHandle);
  ~FeedingDemo();

  aikido::planner::WorldPtr getWorld();
  Workspace& getWorkspace();
  ada::Ada& getAda();

  Eigen::Isometry3d getDefaultFoodTransform();

  /// Checks robot and workspace collisions.
  /// the result string is filled with information about collisions.
  bool isCollisionFree(std::string& result);

  void printRobotConfiguration();

  /// Controlling the hand only.
  void openHand();
  void closeHand();

  /// Convencience functions to simulate food-forque interaction.
  void grabFoodWithForque();
  void ungrabAndDeleteFood();

  /// Convenience functions to move the robot in the feeding demo.
  void moveToStartConfiguration();
  void moveAbovePlate();
  void moveAboveFood(const Eigen::Isometry3d& foodTransform);
  void moveIntoFood();
  void moveOutOfFood();
  void moveInFrontOfPerson();
  void moveTowardsPerson();
  void moveAwayFromPerson();

  /// General functions for robot movement.
  /// They return a bool if the movement could be completed successfully.
  /// Throws a runtime_error if they couldn't find a trajectory.
  bool moveArmToTSR(const aikido::constraint::dart::TSR& tsr);
  bool moveWithEndEffectorOffset(
      const Eigen::Vector3d& direction, double length);
  bool moveArmToConfiguration(const Eigen::Vector6d& configuration);

  /// Postprocesses and executes a trjectory.
  /// Throws runtime_error if the trajectory is empty.
  bool moveArmOnTrajectory(
      aikido::trajectory::TrajectoryPtr trajectory,
      TrajectoryPostprocessType postprocessType = SMOOTH);

private:
  bool adaReal;
  ros::NodeHandle nodeHandle;
  aikido::planner::WorldPtr world;

  std::unique_ptr<ada::Ada> ada;
  aikido::statespace::dart::MetaSkeletonStateSpacePtr armSpace;
  std::unique_ptr<Workspace> workspace;
  aikido::constraint::dart::CollisionFreePtr collisionFreeConstraint;
};
}

#endif
