#include "feeding/Perception.hpp"
#include <aikido/perception/ObjectDatabase.hpp>
#include <tf_conversions/tf_eigen.h>
#include "feeding/util.hpp"

namespace feeding {

//==============================================================================
Perception::Perception(
    aikido::planner::WorldPtr world,
    dart::dynamics::MetaSkeletonPtr adasMetaSkeleton,
    ros::NodeHandle nodeHandle)
  : mWorld(world), mNodeHandle(nodeHandle)
{
  std::string detectorDataURI
      = getRosParam<std::string>("/perception/detectorDataUri", mNodeHandle);
  std::string referenceFrameName
      = getRosParam<std::string>("/perception/referenceFrameName", mNodeHandle);
  std::string foodDetectorTopicName = getRosParam<std::string>(
      "/perception/foodDetectorTopicName", mNodeHandle);
  std::string faceDetectorTopicName = getRosParam<std::string>(
      "/perception/faceDetectorTopicName", mNodeHandle);

  const auto resourceRetriever
      = std::make_shared<aikido::io::CatkinResourceRetriever>();

  mObjectDatabase = std::make_shared<aikido::perception::ObjectDatabase>(
      resourceRetriever, detectorDataURI);

  mFoodDetector = std::unique_ptr<aikido::perception::PoseEstimatorModule>(
      new aikido::perception::PoseEstimatorModule(
          mNodeHandle,
          foodDetectorTopicName,
          mObjectDatabase,
          resourceRetriever,
          referenceFrameName,
          aikido::robot::util::getBodyNodeOrThrow(
              *adasMetaSkeleton, referenceFrameName)));

  mFaceDetector = std::unique_ptr<aikido::perception::PoseEstimatorModule>(
      new aikido::perception::PoseEstimatorModule(
          mNodeHandle,
          faceDetectorTopicName,
          mObjectDatabase,
          resourceRetriever,
          referenceFrameName,
          aikido::robot::util::getBodyNodeOrThrow(
              *adasMetaSkeleton, referenceFrameName)));

  mPerceptionTimeout
      = getRosParam<double>("/perception/timeoutSeconds", mNodeHandle);
  mFoodNameToPerceive
      = getRosParam<std::string>("/perception/foodName", mNodeHandle);
  mPerceivedFaceName
      = getRosParam<std::string>("/perception/faceName", mNodeHandle);

  mLastPerceivedFoodTransform.translation().z() = 0.28;

  mFoodNames = getRosParam<std::vector<std::string>>("/foodItems/names", nodeHandle);
}

Eigen::Isometry3d Perception::getOpticalToWorld()
{
  tf::StampedTransform tfStampedTransform;
  try
  {
    mTFListener.lookupTransform(
        "/world",
        "/camera_color_optical_frame",
        ros::Time(0),
        tfStampedTransform);
  }
  catch (tf::TransformException ex)
  {
    throw std::runtime_error(
        "Failed to get TF Transform: " + std::string(ex.what()));
  }
  Eigen::Isometry3d cameraLensPointInWorldFrame;
  tf::transformTFToEigen(tfStampedTransform, cameraLensPointInWorldFrame);
  return cameraLensPointInWorldFrame;
}

Eigen::Isometry3d Perception::getForqueTransform()
{
  tf::StampedTransform tfStampedTransform;
  try
  {
    mTFListener.lookupTransform(
        "/map",
        "/j2n6s200_forque_end_effector",
        ros::Time(0),
        tfStampedTransform);
  }
  catch (tf::TransformException ex)
  {
    throw std::runtime_error(
        "Failed to get TF Transform: " + std::string(ex.what()));
  }
  Eigen::Isometry3d forqueTransformInWorldFrame;
  tf::transformTFToEigen(tfStampedTransform, forqueTransformInWorldFrame);
  return forqueTransformInWorldFrame;
}

Eigen::Isometry3d Perception::getCameraToWorldTransform()
{
  tf::StampedTransform tfStampedTransform;
  try
  {
    mTFListener.lookupTransform(
        "/camera_color_optical_frame",
        "/map",
        ros::Time(0),
        tfStampedTransform);
  }
  catch (tf::TransformException ex)
  {
    throw std::runtime_error(
        "Failed to get TF Transform: " + std::string(ex.what()));
  }
  Eigen::Isometry3d forqueTransformInWorldFrame;
  tf::transformTFToEigen(tfStampedTransform, forqueTransformInWorldFrame);
  return forqueTransformInWorldFrame;
}

bool Perception::setFoodName(std::string foodName) {
  std::vector<std::string> foodNames = getRosParam<std::vector<std::string>>("/foodItems/names", mNodeHandle);
    if (std::find(foodNames.begin(), foodNames.end(), foodName) != foodNames.end()) {
        mFoodNameToPerceive = foodName;
        return true;
    }
    return false;
}

//==============================================================================
bool Perception::perceiveFood(Eigen::Isometry3d& foodTransform)
{
  return perceiveFood(foodTransform, false, nullptr);
}

bool Perception::perceiveFood(Eigen::Isometry3d& foodTransform,
                              bool perceiveDepthPlane,
                              aikido::rviz::WorldInteractiveMarkerViewerPtr viewer) {
  std::string foodName;
  return perceiveFood(foodTransform, perceiveDepthPlane, viewer, foodName, false);
}

//==============================================================================
bool Perception::perceiveFood(Eigen::Isometry3d& foodTransform,
                              bool perceiveDepthPlane,
                              aikido::rviz::WorldInteractiveMarkerViewerPtr viewer,
                              std::string& foundFoodName,
                              bool perceiveAnyFood)
{

  mFoodDetector->detectObjects(mWorld, ros::Duration(mPerceptionTimeout));
  Eigen::Isometry3d forqueTransform = getForqueTransform();
  Eigen::Isometry3d cameraToWorldTransform = getCameraToWorldTransform();

  dart::dynamics::SkeletonPtr perceivedFood;

  ROS_INFO("Looking for food items");

  double distFromForque = -1;
  std::string fullFoodName;
  for (std::string foodName : mFoodNames)
  {
    if (!perceiveAnyFood && mFoodNameToPerceive != foodName) {
      // ROS_INFO_STREAM(mFoodNameToPerceive << "  " << foodName << "  " << perceiveAnyFood); 
      continue;
    }

    for (int skeletonFrameIdx=0; skeletonFrameIdx<5; skeletonFrameIdx++) {
      int index = 0;
      while (true)
      {
        index++;
        std::string currentFoodName
            = foodName + "_" + std::to_string(index) + "_" + std::to_string(skeletonFrameIdx);
        auto currentPerceivedFood = mWorld->getSkeleton(currentFoodName);

        if (!currentPerceivedFood)
        {
          // ROS_INFO_STREAM(currentFoodName << " not in aikido world");
          if (index > 10) {
            break;
          } else {
            continue;
          }
        }
        Eigen::Isometry3d currentFoodTransform = currentPerceivedFood->getBodyNode(0)
                                                ->getWorldTransform();


        if (!perceiveDepthPlane) {
          // ROS_WARN_STREAM("Food transform before: " << currentFoodTransform.translation().matrix().transpose());
          Eigen::Vector3d start(currentFoodTransform.translation());
          Eigen::Vector3d end(getOpticalToWorld().translation());
          Eigen::ParametrizedLine<double, 3> line(
              start, (end - start).normalized());
          Eigen::Vector3d intersection = line.intersectionPoint(depthPlane);
          currentFoodTransform.translation() = intersection;
          // ROS_WARN_STREAM("Food transform after: " << currentFoodTransform.translation().matrix().transpose());
        } else {
          if (currentFoodTransform.translation().z() < 0.21) {
            ROS_INFO_STREAM("discarding " << currentFoodName << " because of depth");
            continue;
          }
        }

        double currentDistFromForque = 0;
        {
          Eigen::Vector3d start(forqueTransform.translation());
          Eigen::Vector3d end(forqueTransform.translation() + forqueTransform.linear() * Eigen::Vector3d(0,0,1));
          Eigen::ParametrizedLine<double, 3> line(
              start, (end - start).normalized());
          Eigen::Vector3d intersection = line.intersectionPoint(depthPlane);
          currentDistFromForque = (currentFoodTransform.translation() - intersection)
                  .norm();
        }

        // ROS_INFO_STREAM("dist from forque: " << currentDistFromForque << ", " << currentFoodName);
        if (distFromForque < 0 || currentDistFromForque < distFromForque)
        {
          distFromForque = currentDistFromForque;
          foodTransform = currentFoodTransform;
          perceivedFood = currentPerceivedFood;
          foundFoodName = foodName;
          fullFoodName = currentFoodName;
        }
      }
    }
  }


  if (perceivedFood != nullptr)
  {
    double distFromLastTransform = (mLastPerceivedFoodTransform.translation() - foodTransform.translation()).norm();
    if (!perceiveDepthPlane && distFromLastTransform > 0.02) {
      ROS_WARN("food transform too far from last one!");
      return false;
    }

    foodTransform.linear() = cameraToWorldTransform.linear() * foodTransform.linear();
    // dart::dynamics::SimpleFramePtr foodFrame = std::make_shared<dart::dynamics::SimpleFrame>(dart::dynamics::Frame::World(), "foodFrame", foodTransform);
    // frames.push_back(foodFrame);
    // frameMarkers.push_back(viewer->addFrame(foodFrame.get(), 0.07, 0.007));
    foodTransform.linear() = forqueTransform.linear()  * foodTransform.linear();
    // ROS_WARN_STREAM("Food transform: " << foodTransform.translation().matrix());

    if (perceiveDepthPlane) {
      Eigen::Vector3d cameraDirection = cameraToWorldTransform.linear() * Eigen::Vector3d(0,0,1);
      depthPlane = Eigen::Hyperplane<double, 3>(cameraDirection, foodTransform.translation());
    }

    mLastPerceivedFoodTransform = foodTransform;
    // Eigen::Vector3d foodTranslation = foodTransform.translation();
    // foodTranslation += Eigen::Vector3d(0,0,-0.01);
    // foodTransform.translation() = foodTranslation;
    ROS_INFO_STREAM("food perception successful for " << fullFoodName);
    return true;
  }
  else
  {
    ROS_WARN("food perception failed");
    return false;
  }
}

bool Perception::perceiveFace(Eigen::Isometry3d& faceTransform)
{
  mFaceDetector->detectObjects(mWorld, ros::Duration(mPerceptionTimeout));

  // just choose one for now
  for (int skeletonFrameIdx=0; skeletonFrameIdx<5; skeletonFrameIdx++) {
    auto perceivedFace = mWorld->getSkeleton(mPerceivedFaceName + "_" + std::to_string(skeletonFrameIdx));
    if (perceivedFace != nullptr)
    {
      faceTransform = perceivedFace->getBodyNode(0)->getWorldTransform();
      faceTransform.translation().y() = 0.19;
      faceTransform.translation().z() -= 0.01;
       faceTransform.translation().z() = faceTransform.translation().z() + mFaceZOffset;
      ROS_INFO_STREAM("perceived Face: " << faceTransform.matrix());
      return true;
    }
  }
  ROS_WARN("face perception failed");
  return false;
}

void Perception::setFaceZOffset(float faceZOffset) {
  mFaceZOffset = faceZOffset;
}

bool Perception::isMouthOpen()
{
  // return mObjectDatabase->mObjData["faceStatus"].as<bool>();
  return true;
}
}
