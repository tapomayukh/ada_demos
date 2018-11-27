#ifndef FEEDING_FEEDINGDEMO_HPP_
#define FEEDING_FEEDINGDEMO_HPP_

#include <aikido/planner/World.hpp>
#include <aikido/rviz/WorldInteractiveMarkerViewer.hpp>
#include <ros/ros.h>
#include <libada/Ada.hpp>
#include "feeding/AdaMover.hpp"
#include "feeding/Perception.hpp"
#include "feeding/PerceptionServoClient.hpp"
#include "feeding/Workspace.hpp"
#include <aikido/rviz/TSRMarker.hpp>

namespace feeding {

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
  /// Constructor for the Feeding Demo.
  /// Takes care of setting up the robot and the workspace
  /// \param[in] adaReal True if the real robot is used, false it's running in
  /// simulation.
  /// \param[in] useFTSensing turns the FTSensor and the
  /// MoveUntilTouchController on and off
  /// \param[in] nodeHandle Handle of the ros node.
  FeedingDemo(bool adaReal, bool useFTSensing, ros::NodeHandle nodeHandle);

  /// Destructor for the Feeding Demo.
  /// Also shuts down the trajectory controllers.
  ~FeedingDemo();

  /// Gets the aikido world
  aikido::planner::WorldPtr getWorld();

  /// Gets the workspace
  Workspace& getWorkspace();

  /// Gets Ada
  ada::Ada& getAda();

  /// Gets the transform of the default food object (defined in Workspace)
  Eigen::Isometry3d getDefaultFoodTransform();

  /// Checks robot and workspace collisions.
  /// \param[out] result Contains reason for collision.
  /// \return True if no collision was detected.
  bool isCollisionFree(std::string& result);

  /// Prints the configuration of the robot joints.
  void printRobotConfiguration();

  /// Opens Ada's hand
  void openHand();

  /// Closes Ada's hand
  void closeHand();

  /// Attach food to forque
  void grabFoodWithForque();

  /// Detach food from forque and remove it from the aikido world.
  void ungrabAndDeleteFood();

  /// Moves the robot to the start configuration as defined in the ros
  /// parameter.
  void moveToStartConfiguration();

  void moveAboveForque();
  void moveIntoForque();
  void moveOutOfForque();

  /// Moves the forque above the plate.
  bool moveAbovePlate(aikido::rviz::WorldInteractiveMarkerViewerPtr viewer = nullptr);

  void moveAbovePlateAnywhere(aikido::rviz::WorldInteractiveMarkerViewerPtr viewer = nullptr);

  /// Moves the forque above the food item using the values in the ros
  /// parameters.
  /// \param[in] foodTransform the transform of the food which the robot should
  /// move over.
  void moveAboveFood(const Eigen::Isometry3d& foodTransform, int pickupAngleMode, aikido::rviz::WorldInteractiveMarkerViewerPtr viewer, bool useAngledTranslation = true);

  /// Moves the forque downwards into the food.
  /// This function does not throw an exception if the trajectory is aborted,
  /// because we expect that.
  bool moveIntoFood();

  bool moveIntoFood(
      Perception* perception,
      aikido::rviz::WorldInteractiveMarkerViewerPtr viewer);

  /// Moves the forque upwards above the food.
  void moveOutOfFood();
  void moveOutOfFood(float dist);

  /// Moves the forque to a position ready to approach the person.
  void moveInFrontOfPerson();

  void tiltUpInFrontOfPerson(aikido::rviz::WorldInteractiveMarkerViewerPtr viewer);

  void tiltDownInFrontOfPerson(aikido::rviz::WorldInteractiveMarkerViewerPtr viewer);

  /// Moves the forque towards the person.
  /// This function does not throw an exception if the trajectory is aborted,
  /// because we expect that.
  bool moveTowardsPerson();

  bool moveTowardsPerson(
      Perception* perception,
      aikido::rviz::WorldInteractiveMarkerViewerPtr viewer);

  void moveDirectlyToPerson(bool tilted, aikido::rviz::WorldInteractiveMarkerViewerPtr viewer);

  /// Moves the forque away from the person.
  void moveAwayFromPerson();

  std::unique_ptr<AdaMover> mAdaMover;

  void visualizeTrajectory(aikido::trajectory::TrajectoryPtr trajectory);

  aikido::rviz::WorldInteractiveMarkerViewerPtr getViewer();

private:
  bool mIsFTSensingEnabled = false;
  bool mAdaReal;
  ros::NodeHandle mNodeHandle;
  aikido::planner::WorldPtr mWorld;

  std::unique_ptr<ada::Ada> mAda;
  aikido::statespace::dart::MetaSkeletonStateSpacePtr mArmSpace;
  std::unique_ptr<Workspace> mWorkspace;
  aikido::constraint::dart::CollisionFreePtr mCollisionFreeConstraint;

  std::unique_ptr<PerceptionServoClient> mServoClient;

  std::vector<aikido::rviz::TSRMarkerPtr> tsrMarkers;

  aikido::rviz::WorldInteractiveMarkerViewerPtr mViewer;
  aikido::rviz::FrameMarkerPtr frameMarker;
  aikido::rviz::TrajectoryMarkerPtr trajectoryMarkerPtr;
};
}

#endif
