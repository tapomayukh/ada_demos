#include <aikido/rviz/WorldInteractiveMarkerViewer.hpp>
#include <aikido/statespace/Rn.hpp>
#include <mcheck.h>
#include <pr_tsr/plate.hpp>
#include <ros/ros.h>
#include <libada/util.hpp>

#include "feeding/FTThresholdHelper.hpp"
#include "feeding/FeedingDemo.hpp"
#include "feeding/util.hpp"
#include "feeding/perception/Perception.hpp"
#include "feeding/DataCollector.hpp"

#include "experiments.hpp"

using ada::util::getRosParam;
using ada::util::waitForUser;


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
  using namespace feeding;
  // ===== STARTUP =====

  // Is the real robot used or simulation?
  bool adaReal = false;

  // Should the demo continue without asking for human input at each step?
  bool autoContinueDemo = false;

  // the FT sensing can stop trajectories if the forces are too big
  bool useFTSensingToStopTrajectories = false;

  bool TERMINATE_AT_USER_PROMPT = true;

  // True if actual camera is running.
  bool perceptionReal = false;

  std::string demoType{"nips"};

  // Arguments for data collection.
  std::string foodName{"strawberry"};
  std::string dataCollectorPath{"/home/gilwoo/ada_data/"};
  std::size_t directionIndex{0};
  std::size_t trialIndex{0};

  handleArguments(argc, argv,
    adaReal, autoContinueDemo, useFTSensingToStopTrajectories, perceptionReal,
    demoType, foodName, directionIndex, trialIndex, dataCollectorPath);

  if (!adaReal)
    ROS_INFO_STREAM("Simulation Mode: " << !adaReal);

  ROS_INFO_STREAM("DemoType: " << demoType);
  ROS_INFO_STREAM("perception " << perceptionReal);
  ROS_INFO_STREAM("useFTSensingToStopTrajectories " << useFTSensingToStopTrajectories);

#ifndef REWD_CONTROLLERS_FOUND
  ROS_WARN_STREAM(
      "Package rewd_controllers not found. The F/T sensor connection is not "
      "going to work.");
#endif

  // start node
  ros::init(argc, argv, "feeding");
  ros::NodeHandle nodeHandle("~");
  nodeHandle.setParam("/feeding/facePerceptionOn", false);
  ros::AsyncSpinner spinner(2); // 2 threads
  spinner.start();

  auto ftThresholdHelper = std::make_shared<FTThresholdHelper>(
    adaReal && useFTSensingToStopTrajectories, nodeHandle);

  // start demo
  auto feedingDemo = std::make_shared<FeedingDemo>(
    adaReal,
    nodeHandle,
    useFTSensingToStopTrajectories,
    ftThresholdHelper,
    autoContinueDemo);

  auto perception = std::make_shared<Perception>(
      feedingDemo->getWorld(),
      feedingDemo->getAda()->getMetaSkeleton(),
      nodeHandle);

  ftThresholdHelper->init();
  feedingDemo->getAda()->closeHand();

  feedingDemo->setPerception(perception);

  waitForUser("Startup complete.", TERMINATE_AT_USER_PROMPT);

  feedingDemo->moveToStartConfiguration();

  if (demoType == "nips")
  {
    demo(*feedingDemo, nodeHandle);
  }
  else
  {
    ROS_INFO_STREAM("Data will be saved at " << dataCollectorPath << "." << std::endl);
    DataCollector dataCollector(
      feedingDemo, nodeHandle, autoContinueDemo, adaReal, perceptionReal, dataCollectorPath);

    if (demoType == "collect_push_skewer")
      dataCollector.collect(Action::PUSH_AND_SKEWER, foodName, directionIndex, trialIndex);
    else if (demoType == "collect_skewer")
      dataCollector.collect(Action::SKEWER, foodName, directionIndex, trialIndex);
    else if (demoType == "collect_scoop")
      dataCollector.collect(Action::SCOOP, foodName, directionIndex, trialIndex);
    else
      throw std::invalid_argument(demoType + "not recognized.");
  }

  return 0;
}
