#include "feeding/action/DetectAndMoveAboveFood.hpp"
#include <chrono>
#include <thread>
#include "feeding/util.hpp"

#include "feeding/action/MoveAboveFood.hpp"

namespace feeding {
namespace action {

std::unique_ptr<FoodItem> detectAndMoveAboveFood(
    int verbosityLevel,
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

    if (candidateItems.size() == 0) {
      if (verbosityLevel == INTERMEDIATE_VERBOSITY) {
        talk("I can't find that food. Try putting it on the plate.");
      } else if (verbosityLevel == HIGH_VERBOSITY) {
        talk("Food Perception failed");
      }
      ROS_WARN_STREAM("Failed to detect any food. Please place food on the plate.");
    }
    else
      break;

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  ROS_INFO_STREAM("Detected " << candidateItems.size() << " " << foodName);

  bool moveAboveSuccessful = false;
  for (auto& item : candidateItems)
  {
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
      if (verbosityLevel == INTERMEDIATE_VERBOSITY) {
        talk("Sorry, I'm having a little trouble moving. Let's try again.");
      } else if (verbosityLevel == HIGH_VERBOSITY) {
        talk("Move above food failed. Let's try again.");
      }
      
      return nullptr;
    }
    moveAboveSuccessful = true;

    perception->setFoodItemToTrack(item.get());
    return std::move(item);
  }

  if (!moveAboveSuccessful)
  {
    ROS_ERROR("Failed to move above any food.");
    if (verbosityLevel == INTERMEDIATE_VERBOSITY) {
        talk("Sorry, I'm having a little trouble moving. Mind if I get a little help?");
      } else if (verbosityLevel == HIGH_VERBOSITY) {
        talk("Move above food failed.");
      }
    return nullptr;
  }
}
}
}
