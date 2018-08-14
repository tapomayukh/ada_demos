
#include <aikido/rviz/WorldInteractiveMarkerViewer.hpp>
#include <pr_tsr/plate.hpp>
#include <ros/ros.h>
#include "feeding/FTThresholdHelper.hpp"
#include "feeding/FeedingDemo.hpp"
#include "feeding/Perception.hpp"
#include "feeding/util.hpp"
#include <mcheck.h>

#include <aikido/statespace/Rn.hpp>

using namespace feeding;

///
/// OVERVIEW OF FEEDING DEMO CODE
///
/// First, everything is initalized.
/// The FeedingDemo object is responsible for robot and the workspace.
/// The FTThresholdController sets the thresholds in the
/// MoveUntilTouchController
/// The Perception object can perceive food.
///
/// Then the demo is run step by step.
///
int main(int argc, char** argv)
{


  // if (mcheck(NULL) != 0) {
  //   std::cout << "MCHECK FAILED!" << std::endl;
  //   return 1;
  // }


  // ===== STARTUP =====

  // Is the real robot used or simulation?
  bool adaReal = false;

  // Should the demo continue without asking for human input at each step?
  bool autoContinueDemo = false;

  // the FT sensing can stop trajectories if the forces are too big
  bool useFTSensing = false;

  handleArguments(argc, argv, adaReal, autoContinueDemo, useFTSensing);
  ROS_INFO_STREAM("Simulation Mode: " << !adaReal);

  // start node
  ros::init(argc, argv, "feeding");
  ros::NodeHandle nodeHandle("~");
  nodeHandle.setParam("/feeding/facePerceptionOn", false);
  ros::AsyncSpinner spinner(2); // 2 threads
  spinner.start();

  // start demo
  FeedingDemo feedingDemo(adaReal, useFTSensing, nodeHandle);

  FTThresholdHelper ftThresholdHelper(adaReal && useFTSensing, nodeHandle);

  Perception perception(
      feedingDemo.getWorld(),
      feedingDemo.getAda().getMetaSkeleton(),
      nodeHandle);

  // visualization
  aikido::rviz::WorldInteractiveMarkerViewerPtr viewer
      = std::make_shared<aikido::rviz::WorldInteractiveMarkerViewer>(
          feedingDemo.getWorld(),
          getRosParam<std::string>("/visualization/topicName", nodeHandle),
          getRosParam<std::string>("/visualization/baseFrameName", nodeHandle));
  viewer->setAutoUpdate(true);

  std::string collisionCheckResult;
  if (!feedingDemo.isCollisionFree(collisionCheckResult))
  {
    // throw std::runtime_error(collisionCheckResult);
  }

  ftThresholdHelper.init();
  feedingDemo.closeHand();


  if (!waitForUser("Startup complete."))
  {
    return 0;
  }

  feedingDemo.moveToStartConfiguration();

  // ===== ABOVE PLATE =====
  if (!autoContinueDemo)
  {
    if (!waitForUser("Move forque above plate"))
    {
      return 0;
    }
  }
  feedingDemo.moveAbovePlate();

  // ===== ABOVE FOOD =====
  std::vector<std::string> foodNames = getRosParam<std::vector<std::string>>("/foodItems/names", nodeHandle);
  std::vector<double> skeweringForces = getRosParam<std::vector<double>>("/foodItems/forces", nodeHandle);
  std::unordered_map<std::string, double> foodSkeweringForces;
  for (int i=0; i<foodNames.size(); i++) {
    foodSkeweringForces[foodNames[i]] = skeweringForces[i];
  }

  Eigen::Isometry3d foodTransform;
  bool foodFound = false;
  std::string foodName;
  while (!foodFound) {
    std::cout << std::endl << "\033[1;32mWhich food item do you want?\033[0m     > ";
    foodName = "";
    std::cin >> foodName;
    if (!ros::ok()) {return 0;}
    if (!perception.setFoodName(foodName)) {
      std::cout << "\033[1;33mI don't know about any food that's called '" << foodName << ". Wanna get something else?\033[0m" << std::endl;
      continue;
    }

    if (adaReal)
    {
      bool perceptionSuccessful = perception.perceiveFood(foodTransform, false);
      if (!perceptionSuccessful) {
        std::cout << "\033[1;33mI can't see the " << foodName << "... Wanna get something else?\033[0m" << std::endl;
        continue;
      } else {
        foodFound = true;
      }
    }
    else
    {
      foodTransform = feedingDemo.getDefaultFoodTransform();
      foodFound = true;
    }
  }
  std::cout << "\033[1;32mAlright! Let's get the " << foodName << "!\033[0;32m  (Gonna skewer with " << foodSkeweringForces[foodName] << "N)\033[0m" << std::endl << std::endl;


  if (!autoContinueDemo)
  {
    if (!waitForUser("Move forque above food"))
    {
      return 0;
    }
  }
  feedingDemo.moveAboveFood(foodTransform);

  double startForce = 0;
  {
    ftThresholdHelper.startDataCollection(20);
    Eigen::Vector3d currentForce, currentTorque;
    while (!ftThresholdHelper.isDataCollectionFinished(currentForce, currentTorque)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ROS_INFO_STREAM("force before skewering: " << currentForce.matrix().transpose());
    startForce = currentForce.z();
  }

  // ===== INTO FOOD =====
  if (!autoContinueDemo)
  {
    if (!waitForUser("Move forque into food"))
    {
      return 0;
    }
  }
  double torqueThreshold = 2;
  if (!ftThresholdHelper.setThresholds(foodSkeweringForces[foodName], torqueThreshold))
  {
    return 1;
  }
  if (adaReal) {
    feedingDemo.moveIntoFood(&perception, viewer);
  } else {
    feedingDemo.moveIntoFood();
  }
  std::this_thread::sleep_for(
      std::chrono::milliseconds(
          getRosParam<int>("/feedingDemo/waitMillisecsAtFood", nodeHandle)));
  feedingDemo.grabFoodWithForque();

  // ===== OUT OF FOOD =====
  if (!autoContinueDemo)
  {
    if (!waitForUser("Move forque out of food"))
    {
      return 0;
    }
  }
  if (!ftThresholdHelper.setThresholds(AFTER_GRAB_FOOD_FT_THRESHOLD))
  {
    return 1;
  }
  feedingDemo.moveOutOfFood();
  if (!ftThresholdHelper.setThresholds(STANDARD_FT_THRESHOLD))
  {
    return 1;
  }

    {
    ftThresholdHelper.startDataCollection(20);
    Eigen::Vector3d currentForce, currentTorque;
    while (!ftThresholdHelper.isDataCollectionFinished(currentForce, currentTorque)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ROS_INFO_STREAM("force after skewering: " << currentForce.matrix().transpose());
    ROS_WARN_STREAM("difference: " << (startForce - currentForce.z()));
  }

  // ===== IN FRONT OF PERSON =====
  if (!autoContinueDemo)
  {
    if (!waitForUser("Move forque in front of person"))
    {
      return 0;
    }
  }
  feedingDemo.moveInFrontOfPerson();
  nodeHandle.setParam("/feeding/facePerceptionOn", true);

  // while (true) {
  //     Eigen::Isometry3d faceTransform;
  //   bool perceptionSuccessful = perception.perceiveFace(faceTransform);
  // }
  // ===== TOWARDS PERSON =====
  if (!autoContinueDemo)
  {
    if (!waitForUser("Move towards person"))
    {
      return 0;
    }
  }
  feedingDemo.moveTowardsPerson(&perception, viewer);
  nodeHandle.setParam("/feeding/facePerceptionOn", false);
  ROS_WARN("Human is eating");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(
          getRosParam<int>("/feedingDemo/waitMillisecsAtPerson", nodeHandle)));
  feedingDemo.ungrabAndDeleteFood();

  // ===== AWAY FROM PERSON =====
  feedingDemo.moveAwayFromPerson();

  // ===== BACK TO PLATE =====
  if (!autoContinueDemo)
  {
    if (!waitForUser("Move back to plate"))
    {
      return 0;
    }
  }
  feedingDemo.moveAbovePlate();

  // ===== DONE =====
  waitForUser("Demo finished.");
  return 0;
}
