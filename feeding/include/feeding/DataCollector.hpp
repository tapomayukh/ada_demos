#ifndef FEEDING_DATACOLLECTOR_HPP_
#define FEEDING_DATACOLLECTOR_HPP_

#include <fstream>
#include <iostream>
#include <aikido/rviz/WorldInteractiveMarkerViewer.hpp>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <pr_tsr/plate.hpp>
#include <ros/ros.h>
#include <rosbag/bag.h>
#include <sensor_msgs/CameraInfo.h>

#include "feeding/FTThresholdHelper.hpp"
#include "feeding/FeedingDemo.hpp"
#include "feeding/perception/Perception.hpp"
#include "feeding/util.hpp"

namespace feeding {

// Action types for data collection
enum Action
{
  PUSH_AND_SKEWER,
  SKEWER,
  SCOOP
};

enum ImageType
{
  COLOR,
  DEPTH
};

class DataCollector
{
public:
  /// Constructor
  /// \param[in] dataCollectionPath Directory to save all data collection.
  explicit DataCollector(
      std::shared_ptr<FeedingDemo> feedingDemo,
      ros::NodeHandle nodeHandle,
      bool autoContinueDemo,
      bool adaReal,
      bool perceptionReal,
      const std::string& dataCollectionPath = "/home/herb/feeding/data_collection/");

  /// Collect data.
  /// \param[in] action Action to execute
  /// \param[in] foodName Name of food for the data collection
  /// \param[in] directionIndex Index of the direction as suggested by config file
  /// \param[in] trialIndex Index of trial
  void collect(
    Action action,
    const std::string& foodName,
    std::size_t directionIndex,
    std::size_t trialIndex);

private:
  void setDataCollectionParams(
      bool pushCompleted, int foodId, int pushDirectionId, int trialId);

  void pushAndSkewer(
      const std::string& foodName, int mode, float rotAngle, float tiltAngle);

  void infoCallback(
      const sensor_msgs::CameraInfoConstPtr& msg, ImageType imageType);

  void imageCallback(
      const sensor_msgs::ImageConstPtr& msg, ImageType imageType);

  void recordSuccess();

  std::string getCurrentDateTime();

  std::shared_ptr<FeedingDemo> mFeedingDemo;

  ros::NodeHandle mNodeHandle;
  const bool mAutoContinueDemo;
  const bool mAdaReal;
  const bool mPerceptionReal;
  const std::string mDataCollectionPath;

  int mNumTrials;
  std::vector<std::string> mFoods;
  std::vector<double> mTiltAngles;
  std::vector<int> mTiltModes;
  std::vector<double> mDirections;
  std::vector<std::string> mAngleNames;

  image_transport::Subscriber sub;
  image_transport::Subscriber sub2;
  ros::Subscriber sub3;
  ros::Subscriber sub4;

  std::atomic<bool> mShouldRecordImage;
  std::atomic<bool> mShouldRecordInfo;
  std::atomic<bool> isAfterPush;
  std::atomic<int> mCurrentFood;
  std::atomic<int> mCurrentDirection;
  std::atomic<int> mCurrentTrial;
};

} // namespace feeding

#endif