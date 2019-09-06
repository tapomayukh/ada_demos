#include "feeding/action/DetectAndMoveAboveFood.hpp"
#include <chrono>
#include <thread>
#include "feeding/util.hpp"
#include <libada/util.hpp>

#include "feeding/action/MoveAboveFood.hpp"

using ada::util::getRosParam;

namespace feeding {
namespace action {

std::unique_ptr<FoodItem> detectAndMoveAboveFood(
    const std::shared_ptr<ada::Ada>& ada,
    const aikido::constraint::dart::CollisionFreePtr& collisionFree,
    const std::shared_ptr<Perception>& perception,
    const std::string& foodName,
    double heightAboveFood,
    double horizontalTolerance,
    double verticalTolerance,
    double rotationTolerance,
    double tiltTolerance,
    double planningTimeout,
    int maxNumTrials,
    std::vector<double> velocityLimits,
    FeedingDemo* feedingDemo)
{
  std::vector<std::unique_ptr<FoodItem>> candidateItems;
  while (true)
  {
    // Perception returns the list of good candidates, any one of them is good.
    // Multiple candidates are preferrable since planning may fail.
    candidateItems = perception->perceiveFood(foodName);

    if (candidateItems.size() == 0)
    {
      talk("I can't find that food. Try putting it on the plate.");
      ROS_WARN_STREAM(
          "Failed to detect any food. Please place food on the plate.");
    }
    else
      break;

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  ROS_INFO_STREAM("Detected " << candidateItems.size() << " " << foodName);

  bool moveAboveSuccessful = false;
  int actionOverride = 0;
  if (!getRosParam<bool>("/humanStudy/autoAcquisition", feedingDemo->getNodeHandle()))
  {
      // Read Action from Topic
      talk("How should I pick up the food?", false);
      std::string actionName;
      std::string actionTopic;
      feedingDemo->getNodeHandle().param<std::string>("/humanStudy/actionTopic", actionTopic, "/study_action_msgs");
      actionName = getInputFromTopic(actionTopic, feedingDemo->getNodeHandle(), true, -1);
      talk("Alright, let me use " + actionName, true);

      if (actionName == "cross_skewer") {
        actionOverride = 1;
      } else if (actionName == "tilt") {
        actionOverride = 2;
      } else if (actionName == "cross_tilt") {
        actionOverride = 3;
      } else if (actionName == "angle") {
        actionOverride = 4;
      } else if (actionName == "cross_angle"){
        actionOverride = 5;
      } else {
        actionOverride = 0;
      }
  }
  for (auto& item : candidateItems)
  {

    if (!getRosParam<bool>("/humanStudy/autoAcquisition", feedingDemo->getNodeHandle()))
    {
      // Overwrite action in item
      item->setAction(actionOverride);
    }

    auto action = item->getAction();

    std::cout << "Tilt style " << action->getTiltStyle() << std::endl;
    if (!moveAboveFood(
            ada,
            collisionFree,
            item->getName(),
            item->getPose(),
            action->getRotationAngle(),
            action->getTiltStyle(),
            heightAboveFood,
            horizontalTolerance,
            verticalTolerance,
            rotationTolerance,
            tiltTolerance,
            planningTimeout,
            maxNumTrials,
            velocityLimits,
            feedingDemo))
    {
      ROS_INFO_STREAM("Failed to move above " << item->getName());
      talk("Sorry, I'm having a little trouble moving. Let's try again.");
      return nullptr;
    }
    moveAboveSuccessful = true;

    perception->setFoodItemToTrack(item.get());
    return std::move(item);
  }

  if (!moveAboveSuccessful)
  {
    ROS_ERROR("Failed to move above any food.");
    talk(
        "Sorry, I'm having a little trouble moving. Mind if I get a little "
        "help?");
    return nullptr;
  }
}
} // namespace action
} // namespace feeding