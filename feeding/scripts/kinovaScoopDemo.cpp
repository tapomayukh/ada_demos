
#include <ros/ros.h>
#include <libada/util.hpp>

#include "feeding/FeedingDemo.hpp"
#include "feeding/util.hpp"
#include "feeding/action/KinovaScoop.hpp"

using ada::util::waitForUser;

namespace feeding {

void kinovaScoopDemo(
    FeedingDemo& feedingDemo,
    ros::NodeHandle nodeHandle)
{

  ROS_INFO_STREAM("========== Kinova Scoop DEMO ==========");

  auto ada = feedingDemo.getAda();
  auto workspace = feedingDemo.getWorkspace();
  auto collisionFree = feedingDemo.getCollisionConstraint();
  auto plate = workspace->getPlate()->getRootBodyNode()->getWorldTransform();

  while (true)
  {
    waitForUser("next step?", ada);

    // is this two param necessary?
    // nodeHandle.setParam("/deep_pose/forceFood", false);
    // nodeHandle.setParam("/deep_pose/publish_spanet", (true));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ROS_INFO_STREAM("Running Kinova Scoop demo");

    action::kinovaScoop(
    ada,
    collisionFree,
    plate,
    feedingDemo.getPlateEndEffectorTransform(),
    feedingDemo.mPlateTSRParameters["horizontalTolerance"],
    feedingDemo.mPlateTSRParameters["verticalTolerance"],
    feedingDemo.mPlateTSRParameters["rotationTolerance"],
    feedingDemo.mPlanningTimeout,
    feedingDemo.mMaxNumTrials,
    feedingDemo.mVelocityLimits);

    workspace.reset();
  }

  // ===== DONE =====
  ROS_INFO("Demo finished.");
}
};
