
#include <ros/ros.h>
#include <aikido/rviz/WorldInteractiveMarkerViewer.hpp>
#include "feeding/FTThresholdHelper.hpp"
#include "feeding/FeedingDemo.hpp"
#include "feeding/Perception.hpp"
#include "feeding/util.hpp"

namespace feeding {

int nipsmain(FeedingDemo& feedingDemo, FTThresholdHelper& ftThresholdHelper,
             Perception& perception,
             aikido::rviz::WorldInteractiveMarkerViewerPtr viewer,
             ros::NodeHandle nodeHandle, bool autoContinueDemo, bool adaReal) {
  std::cout << std::endl
            << "\033[1;32m      ***** BITE TRANSFER NIPS DEMO *****\033[0m"
            << std::endl;
  std::cout << std::endl
            << "\033[1;32mWhich food item do you want?\033[0m" << std::endl;
  std::cout << "\033[0;32m1) Strawberry\033[0m" << std::endl;
  std::cout << "\033[0;32m2) Cantaloupe\033[0m" << std::endl;
  std::cout << "\033[0;32m3) Celery\033[0m" << std::endl;
  std::cout << "\033[0;32m4) Carrot\033[0m" << std::endl;

  std::string foodName = "";
  while (foodName == "") {
    std::cout << "> ";
    std::string idString;
    std::cin >> idString;
    try {
      int id = std::stoi(idString);
      if (id < 1 || id > 5) {
        throw std::invalid_argument("");
      }
      switch (id) {
        case 1:
          foodName = "strawberry";
          break;
        case 2:
          foodName = "cantaloupe";
          break;
        case 3:
          foodName = "celery";
          break;
        case 4:
          foodName = "carrot";
          break;
      }
    } catch (const std::invalid_argument& ia) {
      std::cout << "\033[1;31mInvalid argument. Try again.\033[0m" << std::endl;
    }
  }

  std::cout << std::endl
            << "\033[1;32mWhat step should I proceed with (1-5)?\033[0m"
            << std::endl;

  int stepIdx = -1;
  while (stepIdx < 0) {
    std::cout << "> ";
    std::string stepIdxString;
    std::cin >> stepIdxString;
    try {
      int idx = std::stoi(stepIdxString);
      if (idx < 1 || idx > 5) {
        throw std::invalid_argument("");
      }
      stepIdx = idx;
    } catch (const std::invalid_argument& ia) {
      std::cout << "\033[1;31mInvalid argument. Try again.\033[0m" << std::endl;
    }
  }

  nodeHandle.setParam("/deep_pose/forceFood", true);
  nodeHandle.setParam("/deep_pose/forceFoodName", foodName);
  nodeHandle.setParam("/deep_pose/publish_spnet", (true));
  nodeHandle.setParam("/deep_pose/spnet_food_name", foodName);
  nodeHandle.setParam("/deep_pose/invertSPNetDirection", stepIdx == 5);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::cout << std::endl
            << "\033[1;32mRunning bite transfer study for " << foodName
            << " beginning on step " << stepIdx << ".\033[0m" << std::endl;

  bool skipSkewering = getRosParam<bool>("/study/skipSkewering", nodeHandle);

  if (!skipSkewering) {
    bool angledSkewering = (stepIdx == 2);
    bool foodPickedUp = false;
    int tries = 1;
    while (!foodPickedUp) {
      // ===== ABOVE PLATE =====
      if (!autoContinueDemo) {
        if (!waitForUser("Move forque above plate")) {
          return 0;
        }
      }
      feedingDemo.moveAbovePlate();
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

      // ===== PERCEPTION =====
      std::vector<std::string> foodNames =
          getRosParam<std::vector<std::string>>("/foodItems/names", nodeHandle);
      std::vector<double> skeweringForces =
          getRosParam<std::vector<double>>("/foodItems/forces", nodeHandle);
      std::unordered_map<std::string, double> foodSkeweringForces;
      for (int i = 0; i < foodNames.size(); i++) {
        foodSkeweringForces[foodNames[i]] = skeweringForces[i];
      }

      Eigen::Isometry3d foodTransform;
      if (adaReal) {
        bool foodFound = false;
        perception.setFoodName(foodName);
        while (!foodFound) {
          foodFound = perception.perceiveFood(foodTransform, true, viewer);
          if (!foodFound) {
            std::cout << "\033[1;33mI can't see the " << foodName << "\033[0m"
                      << std::endl;
            if (!waitForUser("Try perception again?")) {
              return 0;
            }
          }
        }
      } else {
        foodTransform = feedingDemo.getDefaultFoodTransform();
      }
      std::cout << "\033[1;32mAlright! Let's get the " << foodName
                << "!\033[0;32m  (Gonna skewer with "
                << foodSkeweringForces[foodName] << "N)\033[0m" << std::endl
                << std::endl;

      // ===== ABOVE FOOD =====
      if (!autoContinueDemo) {
        if (!waitForUser("Move forque above food")) {
          return 0;
        }
      }
      if (angledSkewering) {
        feedingDemo.moveAboveFood(foodTransform, 0.25 * M_PI, viewer, true);
      } else {
        feedingDemo.moveAboveFood(foodTransform, 0, viewer, true);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(800));
      bool perceptionSuccessful =
          perception.perceiveFood(foodTransform, true, viewer);
      if (!perceptionSuccessful) {
        std::cout << "\033[1;33mI can't see the " << foodName
                  << " anymore...\033[0m" << std::endl;
      } else {
        if (angledSkewering) {
          feedingDemo.moveAboveFood(foodTransform, 0.25 * M_PI, viewer, true);
        } else {
          feedingDemo.moveAboveFood(foodTransform, 0, viewer, true);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      double zForceBeforeSkewering = 0;
      if (adaReal && ftThresholdHelper.startDataCollection(20)) {
        Eigen::Vector3d currentForce, currentTorque;
        while (!ftThresholdHelper.isDataCollectionFinished(currentForce,
                                                           currentTorque)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        zForceBeforeSkewering = currentForce.z();
      }

      // ===== INTO FOOD =====
      if (!autoContinueDemo) {
        if (!waitForUser("Move forque into food")) {
          return 0;
        }
      }
      double torqueThreshold = 2;
      if (!ftThresholdHelper.setThresholds(foodSkeweringForces[foodName],
                                           torqueThreshold)) {
        return 1;
      }
      if (adaReal) {
        feedingDemo.moveIntoFood(&perception, viewer);
      } else {
        feedingDemo.moveIntoFood();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(
          getRosParam<int>("/feedingDemo/waitMillisecsAtFood", nodeHandle)));
      feedingDemo.grabFoodWithForque();

      // ===== OUT OF FOOD =====
      if (!autoContinueDemo) {
        if (!waitForUser("Move forque out of food")) {
          return 0;
        }
      }
      if (!ftThresholdHelper.setThresholds(AFTER_GRAB_FOOD_FT_THRESHOLD)) {
        return 1;
      }
      feedingDemo.moveOutOfFood();
      if (!ftThresholdHelper.setThresholds(STANDARD_FT_THRESHOLD)) {
        return 1;
      }

      // Check if we have food
      double forceDifference = 100;
      if (adaReal && ftThresholdHelper.startDataCollection(20)) {
        Eigen::Vector3d currentForce, currentTorque;
        while (!ftThresholdHelper.isDataCollectionFinished(currentForce,
                                                           currentTorque)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        forceDifference = std::fabs(zForceBeforeSkewering - currentForce.z());
      }
      ROS_WARN_STREAM("force difference: " << forceDifference);

      if (forceDifference > 0.022) {
        foodPickedUp = true;
      } else {
        if (tries == 1) {
          std::cout
              << "\033[1;32mOoops! I think I didn't manage to pick up the "
              << foodName << ". Let me try again!\033[0;32m" << std::endl;
        } else if (tries == 2) {
          std::cout
              << "\033[1;32mOoops! I think I didn't manage to pick up the "
              << foodName
              << ". Let me try with a different skewering technique!\033[0;32m"
              << std::endl;
          angledSkewering = !angledSkewering;
        } else if (tries == 3) {
          std::cout
              << "\033[1;32mOoops! I think I didn't manage to pick up the "
              << foodName
              << ". Maybe we should try a different food item...!\033[0;32m"
              << std::endl;
          angledSkewering = !angledSkewering;

          // Re-select food type
          foodName = "";
          while (foodName == "") {
            std::cout << "> ";
            std::string idString;
            std::cin >> idString;
            try {
              int id = std::stoi(idString);
              if (id < 1 || id > 5) {
                throw std::invalid_argument("");
              }
              switch (id) {
                case 1:
                  foodName = "strawberry";
                  break;
                case 2:
                  foodName = "cantaloupe";
                  break;
                case 3:
                  foodName = "celery";
                  break;
                case 4:
                  foodName = "carrot";
                  break;
              }
            } catch (const std::invalid_argument& ia) {
              std::cout << "\033[1;31mInvalid argument. Try again.\033[0m"
                        << std::endl;
            }
          }
          nodeHandle.setParam("/deep_pose/forceFoodName", foodName);
          nodeHandle.setParam("/deep_pose/spnet_food_name", foodName);
          tries = 0;
        } // end tries == 3
      } // end food difference < threshold
      tries++;
    }  // end while (!foodPickedUp)
  }    // end if (!skipSkewering)

  // ===== IN FRONT OF PERSON =====
  if (!autoContinueDemo) {
    if (!waitForUser("Move forque in front of person")) {
      return 0;
    }
  }
  feedingDemo.moveInFrontOfPerson();

  bool tilted = (stepIdx != 3);
  feedingDemo.moveDirectlyToPerson(tilted, viewer);

  // ===== EATING =====
  ROS_WARN("Human is eating");
  std::this_thread::sleep_for(std::chrono::milliseconds(
      getRosParam<int>("/feedingDemo/waitMillisecsAtPerson", nodeHandle)));
  feedingDemo.ungrabAndDeleteFood();

  if (!autoContinueDemo) {
    if (!waitForUser("Move away from person 1")) {
      return 0;
    }
    if (!waitForUser("Move away from person 2")) {
      return 0;
    }
  }

  // ===== BACK TO PLATE =====
  if (!autoContinueDemo) {
    if (!waitForUser("Move back to plate")) {
      return 0;
    }
  }
  feedingDemo.moveAbovePlate();

  // ===== DONE =====
  waitForUser("Demo finished.");
}

};  // namespace feeding