#include "feeding/FTThresholdHelper.hpp"
#include "feeding/FeedingDemo.hpp"
#include "feeding/Perception.hpp"
#include "feeding/util.hpp"
#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <aikido/rviz/WorldInteractiveMarkerViewer.hpp>
#include <image_transport/image_transport.h>

namespace feeding {

std::atomic<bool> shouldRecordImage{false};

void imageCallback(const sensor_msgs::ImageConstPtr& msg)
{

  if (shouldRecordImage.load()) {
    ROS_ERROR("recording image!");

    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    // cv::namedWindow("image", WINDOW_AUTOSIZE);
    // cv::imshow("image", cv_ptr->image);
    // cv::waitKey(30);

    static int image_count = 0;
    // std::stringstream sstream;
    std::string imageFile = "/home/herb/Images/my_image" + std::to_string(image_count) + ".png";
    // sstream << imageFile;
    bool worked = cv::imwrite( imageFile,  cv_ptr->image );
    image_count++;
    ROS_INFO_STREAM("image saved to " << imageFile << ", worked: " << worked);
    shouldRecordImage.store(false);
  }
}

int bltmain(FeedingDemo& feedingDemo,
                FTThresholdHelper& ftThresholdHelper,
                Perception& perception,
                aikido::rviz::WorldInteractiveMarkerViewerPtr viewer,
                ros::NodeHandle nodeHandle,
                bool autoContinueDemo,
                bool adaReal) {


    image_transport::ImageTransport it(nodeHandle);
    // ros::Subscriber sub_info = nodeHandle.subscribe("/camera/color/camera_info", 1, cameraInfo);
    image_transport::Subscriber sub = it.subscribe("/data_collection/target_image", 1, imageCallback/*, image_transport::TransportHints("compressed")*/);

    if (!autoContinueDemo)
    {
      if (!waitForUser("Ready to start."))
      {
        return 0;
      }
    }

  for (int trial=0; trial<10; trial++) {
    std::cout << "\033[1;33mSTARTING TRIAL " << trial << "\033[0m" << std::endl;

    // ===== ABOVE PLATE =====
    bool stepSuccessful = false;
    while (!stepSuccessful) {
      try {
        feedingDemo.moveAbovePlateAnywhere(viewer);
        stepSuccessful = true;
      } catch (std::runtime_error) {
        if (!waitForUser("Trajectory execution failed. Try again?")) {continue;}
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    shouldRecordImage.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    while (shouldRecordImage.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    // ===== INTO FOOD =====
    if (!autoContinueDemo)
    {
      if (!waitForUser("Move forque into food"))
      {
        return 0;
      }
    }
    if (!ftThresholdHelper.setThresholds(25, 0.5))
    {
      return 1;
    }
    feedingDemo.moveIntoFood();

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
    for (float dist = 0.08; dist>0.01; dist-=0.02) {
     try {
       feedingDemo.moveOutOfFood(dist);
     } catch (std::runtime_error) {
       continue;
     }
    }
    if (!ftThresholdHelper.setThresholds(STANDARD_FT_THRESHOLD))
    {
      return 1;
    }
  }

  // ===== DONE =====
  waitForUser("Demo finished.");
}

};
