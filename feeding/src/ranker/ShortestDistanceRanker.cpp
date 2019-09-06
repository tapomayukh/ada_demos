#include "feeding/ranker/ShortestDistanceRanker.hpp"

#include <dart/common/StlHelpers.hpp>
#include <iterator>
#include "feeding/AcquisitionAction.hpp"
#include "feeding/util.hpp"

namespace feeding {

//==============================================================================
void ShortestDistanceRanker::sort(
    std::vector<std::unique_ptr<FoodItem>>& items) const
{
  // Ascending since score is the distance.
  TargetFoodRanker::sort(items, SORT_ORDER::ASCENDING);
}

//==============================================================================
std::unique_ptr<FoodItem> ShortestDistanceRanker::createFoodItem(
    const aikido::perception::DetectedObject& item,
    const Eigen::Isometry3d& forqueTransform) const
{
  TiltStyle tiltStyle(TiltStyle::NONE);

  // Get Ideal Action Per Food Item
  auto it = std::find(FOOD_NAMES.begin(), FOOD_NAMES.end(), item.getName());
  if (it != FOOD_NAMES.end())
  {
    int actionNum = BEST_ACTIONS[std::distance(FOOD_NAMES.begin(), it)];
    switch (actionNum / 2) {
      case 1:
      tiltStyle = TiltStyle::VERTICAL;
      break;
      case 2:
      tiltStyle = TiltStyle::ANGLED;
      break;
      default:
      tiltStyle = TiltStyle::NONE;
    }
  }

  // TODO: check if rotation and tilt angle should change
  AcquisitionAction action(tiltStyle, 0.0, 0.0, Eigen::Vector3d(0, 0, -1));

  auto itemPose = item.getMetaSkeleton()->getBodyNode(0)->getWorldTransform();
  double distance = getDistance(itemPose, forqueTransform);

  return std::make_unique<FoodItem>(
      item.getName(), item.getUid(), item.getMetaSkeleton(), action, distance);
}

} // namespace feeding
