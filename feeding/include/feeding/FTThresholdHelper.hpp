#ifndef FEEDING_FTTHRESHOLDHELPER_HPP_
#define FEEDING_FTTHRESHOLDHELPER_HPP_

#include <rewd_controllers/FTThresholdClient.hpp>
#include <geometry_msgs/WrenchStamped.h>
#include <ros/ros.h>
#include <Eigen/Geometry>
#include <mutex>

namespace feeding {

enum FTThreshold
{
  STANDARD_FT_THRESHOLD,
  GRAB_FOOD_FT_THRESHOLD,
  AFTER_GRAB_FOOD_FT_THRESHOLD,
  PUSH_FOOD_FT_THRESHOLD
};

/// The FTThresholdHelper configures the MoveUntilTouchController's
/// thresholds.
/// When those thresholds are exceeded, the controller stops the movement.
class FTThresholdHelper
{

public:
  /// Constructor.
  /// \param[in] useThresholdControl You can turn this whole object on and off.
  /// Useful if you don't use the MoveUntilTouchController and don't need to set
  /// these thresholds.
  /// \param[in] nodeHandle Handle of the ros node.
  FTThresholdHelper(bool useThresholdControl, ros::NodeHandle nodeHandle);

  /// Needs to be called before setting the first thresholds.
  /// Blocks until the threshold could be set successfully.
  /// Can be aborted with Ctrl-C.
  void init();

  /// Sets the MoveUntilTouchControllers Thresholds accordingly.
  /// Throws a runtime_error if we useThresholdControl and we are unable to set
  /// because of an error.
  /// \return True if the thresholds were set successfully or false if we
  /// experienced a timeout.
  bool setThresholds(FTThreshold);

  bool setThresholds(double forces, double torques);

  bool startDataCollection(int numberOfDataPoints);
  bool isDataCollectionFinished(Eigen::Vector3d& forces, Eigen::Vector3d& torques);

private:
  bool mUseThresholdControl;
  ros::NodeHandle mNodeHandle;

  int mDataPointsToCollect = 0;
  std::mutex mDataCollectionMutex;
  std::vector<Eigen::Vector3d> mCollectedForces;
  std::vector<Eigen::Vector3d> mCollectedTorques;

  // \brief Gets data from the force/torque sensor
  ros::Subscriber mForceTorqueDataSub;

  std::unique_ptr<rewd_controllers::FTThresholdClient> mFTThresholdClient;

  std::pair<double, double> getThresholdValues(FTThreshold threshold);

  /**
   * \brief Called whenever a new Force/Torque message arrives on the ros topic
   */
  void forceTorqueDataCallback(const geometry_msgs::WrenchStamped& msg);
};
}

#endif
