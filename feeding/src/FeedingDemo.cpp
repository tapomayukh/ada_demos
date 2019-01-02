#include "feeding/FeedingDemo.hpp"
#include <aikido/rviz/TrajectoryMarker.hpp>
#include <pr_tsr/plate.hpp>
#include <libada/util.hpp>

#include "feeding/util.hpp"

using ada::util::getRosParam;
using ada::util::createBwMatrixForTSR;
using ada::util::createIsometry;

namespace feeding {

//==============================================================================
FeedingDemo::FeedingDemo(
    bool adaReal, bool useFTSensing, ros::NodeHandle nodeHandle)
  : mAdaReal(adaReal), mNodeHandle(nodeHandle)
{

  mWorld = std::make_shared<aikido::planner::World>("feeding");

  std::string armTrajectoryExecutor = useFTSensing
                                          ? "move_until_touch_topic_controller"
                                          : "rewd_trajectory_controller";

  mAda = std::make_shared<ada::Ada>(
      mWorld,
      !mAdaReal,
      getRosParam<std::string>("/ada/urdfUri", mNodeHandle),
      getRosParam<std::string>("/ada/srdfUri", mNodeHandle),
      getRosParam<std::string>("/ada/endEffectorName", mNodeHandle),
      armTrajectoryExecutor);
  mArmSpace
      = std::make_shared<aikido::statespace::dart::MetaSkeletonStateSpace>(
          mAda->getArm()->getMetaSkeleton().get());

  Eigen::Isometry3d robotPose = createIsometry(
      getRosParam<std::vector<double>>("/ada/baseFramePose", mNodeHandle));

  mWorkspace = std::unique_ptr<Workspace>(
      new Workspace(mWorld, robotPose, mAdaReal, mNodeHandle));

  // Setting up collisions
  dart::collision::CollisionDetectorPtr collisionDetector
      = dart::collision::FCLCollisionDetector::create();
  std::shared_ptr<dart::collision::CollisionGroup> armCollisionGroup
      = collisionDetector->createCollisionGroup(
          mAda->getMetaSkeleton().get(),
          mAda->getHand()->getEndEffectorBodyNode());
  std::shared_ptr<dart::collision::CollisionGroup> envCollisionGroup
      = collisionDetector->createCollisionGroup(
          mWorkspace->getTable().get(),
          mWorkspace->getWorkspaceEnvironment().get()
          // mWorkspace->getWheelchair().get()
          );
  mCollisionFreeConstraint
      = std::make_shared<aikido::constraint::dart::CollisionFree>(
          mArmSpace, mAda->getArm()->getMetaSkeleton(), collisionDetector);
  mCollisionFreeConstraint->addPairwiseCheck(
      armCollisionGroup, envCollisionGroup);

  // visualization
  mViewer = std::make_shared<aikido::rviz::WorldInteractiveMarkerViewer>(
      mWorld,
      getRosParam<std::string>("/visualization/topicName", nodeHandle),
      getRosParam<std::string>("/visualization/baseFrameName", nodeHandle));
  mViewer->setAutoUpdate(true);

  if (mAdaReal)
  {
    mAda->startTrajectoryExecutor();
  }
}

//==============================================================================
FeedingDemo::~FeedingDemo()
{
  if (mAdaReal)
  {
    // wait for a bit so controller actually stops moving
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    mAda->stopTrajectoryExecutor();
  }
}

//==============================================================================
aikido::planner::WorldPtr FeedingDemo::getWorld()
{
  return mWorld;
}

//==============================================================================
Workspace& FeedingDemo::getWorkspace()
{
  return *mWorkspace;
}

//==============================================================================
ada::Ada& FeedingDemo::getAda()
{
  return *mAda;
}

//==============================================================================
Eigen::Isometry3d FeedingDemo::getDefaultFoodTransform()
{
  return mWorkspace->getDefaultFoodItem()
      ->getRootBodyNode()
      ->getWorldTransform();
}

//==============================================================================
bool FeedingDemo::isCollisionFree(std::string& result)
{
  auto robotState = mAda->getStateSpace()->getScopedStateFromMetaSkeleton(
      mAda->getMetaSkeleton().get());
  aikido::constraint::dart::CollisionFreeOutcome collisionCheckOutcome;
  if (!mCollisionFreeConstraint->isSatisfied(
          robotState, &collisionCheckOutcome))
  {
    result = "Robot is in collison: " + collisionCheckOutcome.toString();
    return false;
  }
  result = "Robot is not in collision";
  return true;
}

//==============================================================================
void FeedingDemo::printRobotConfiguration()
{
  Eigen::IOFormat CommaInitFmt(
      Eigen::StreamPrecision,
      Eigen::DontAlignCols,
      ", ",
      ", ",
      "",
      "",
      " << ",
      ";");
  auto defaultPose = mAda->getArm()->getMetaSkeleton()->getPositions();
  ROS_INFO_STREAM("Current configuration" << defaultPose.format(CommaInitFmt));
}

//==============================================================================
void FeedingDemo::grabFoodWithForque()
{
  if (!mWorkspace->getDefaultFoodItem())
  {
    mWorkspace->addDefaultFoodItemAtPose(
        mAda->getHand()->getEndEffectorBodyNode()->getTransform());
  }
  mAda->getHand()->grab(mWorkspace->getDefaultFoodItem());
}

//==============================================================================
void FeedingDemo::ungrabAndDeleteFood()
{
  mAda->getHand()->ungrab();
  mWorkspace->deleteFood();
}

//==============================================================================
void FeedingDemo::moveToStartConfiguration()
{
  auto home
      = getRosParam<std::vector<double>>("/ada/homeConfiguration", mNodeHandle);
  if (mAdaReal)
  {
    // We decided to not move to an initial configuration for now
    // mAdaMover->moveArmToConfiguration(Eigen::Vector6d(home.data()));
  }
  else
  {
    mAda->getArm()->getMetaSkeleton()->setPositions(
        Eigen::Vector6d(home.data()));
  }
}

//==============================================================================
void FeedingDemo::moveAboveForque()
{

  double forkHolderAngle
      = getRosParam<double>("/study/forkHolderAngle", mNodeHandle);
  std::vector<double> forkHolderTranslation = getRosParam<std::vector<double>>(
      "/study/forkHolderTranslation", mNodeHandle);

  auto aboveForqueTSR = pr_tsr::getDefaultPlateTSR();
  Eigen::Isometry3d forquePose = Eigen::Isometry3d::Identity();
  // y positive is closer to wheelchair
  // z
  // forquePose.translation() = Eigen::Vector3d{0.57, -0.019, 0.012};
  // forquePose.linear() = Eigen::Matrix3d(Eigen::AngleAxisd(0.15,
  // Eigen::Vector3d::UnitX()));
  forquePose.translation() = Eigen::Vector3d{forkHolderTranslation[0],
                                             forkHolderTranslation[1],
                                             forkHolderTranslation[2]};
  ROS_INFO_STREAM("fork holder angle: " << forkHolderAngle);
  forquePose.linear() = Eigen::Matrix3d(
      Eigen::AngleAxisd(forkHolderAngle, Eigen::Vector3d::UnitX()));
  aboveForqueTSR.mT0_w = forquePose;
  aboveForqueTSR.mTw_e.translation() = Eigen::Vector3d{0, 0, 0};

  aboveForqueTSR.mBw = createBwMatrixForTSR(0.0001, 0.0001, 0, 0);
  aboveForqueTSR.mTw_e.matrix()
      *= mAda->getHand()->getEndEffectorTransform("plate")->matrix();

  bool trajectoryCompleted
      = mAda->moveArmToTSR(aboveForqueTSR, mCollisionFreeConstraint);

  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

//==============================================================================
void FeedingDemo::moveIntoForque()
{
  bool trajectoryCompleted = mAda->moveArmToEndEffectorOffset(
      Eigen::Vector3d(0, 1, 0), 0.032, mCollisionFreeConstraint);
  // trajectoryCompleted might be false because the forque hit the food
  // along the way and the trajectory was aborted
}

//==============================================================================
void FeedingDemo::moveOutOfForque()
{
  bool trajectoryCompleted = mAda->moveArmToEndEffectorOffset(
      Eigen::Vector3d(0, -1, 0), 0.04, mCollisionFreeConstraint);
  // trajectoryCompleted might be false because the forque hit the food
  // along the way and the trajectory was aborted
}

//==============================================================================
bool FeedingDemo::moveAbovePlate(
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  double heightAbovePlate
      = getRosParam<double>("/feedingDemo/heightAbovePlate", mNodeHandle);
  double horizontalToleranceAbovePlate = getRosParam<double>(
      "/planning/tsr/horizontalToleranceAbovePlate", mNodeHandle);
  double verticalToleranceAbovePlate = getRosParam<double>(
      "/planning/tsr/verticalToleranceAbovePlate", mNodeHandle);

  auto abovePlateTSR = pr_tsr::getDefaultPlateTSR();
  abovePlateTSR.mT0_w
      = mWorkspace->getPlate()->getRootBodyNode()->getWorldTransform();
  abovePlateTSR.mTw_e.translation() = Eigen::Vector3d{0, 0, heightAbovePlate};

  abovePlateTSR.mBw = createBwMatrixForTSR(
      horizontalToleranceAbovePlate, verticalToleranceAbovePlate, 0, 0);
  Eigen::Isometry3d eeTransform
      = *mAda->getHand()->getEndEffectorTransform("plate");
  eeTransform.linear()
      = eeTransform.linear()
        * Eigen::Matrix3d(
              Eigen::AngleAxisd(M_PI * 0.25, Eigen::Vector3d::UnitZ()));
  abovePlateTSR.mTw_e.matrix() *= eeTransform.matrix();

  if (viewer)
  {
    // tsrMarkers.push_back(viewer->addTSRMarker(abovePlateTSR, 100,
    // "someTSRName"));
  }

  std::vector<double> velocityLimits{0.2, 0.2, 0.2, 0.2, 0.2, 0.4};
  Eigen::VectorXd nominalConfiguration(6);
  nominalConfiguration << -2.00483, 3.26622, 1.8684, -2.38345, 4.11224, 5.03713;
  // 0.7823, 3.0054, 4.4148, 2.3930, 2.1522, 0.03480;

  auto trajectory = mAda->planArmToTSR(
      abovePlateTSR, mCollisionFreeConstraint, nominalConfiguration);

  visualizeTrajectory(trajectory);
  return mAda->moveArmOnTrajectory(
      trajectory,
      mCollisionFreeConstraint,
      ada::TrajectoryPostprocessType::KUNZ,
      velocityLimits);
}

void FeedingDemo::visualizeTrajectory(
    aikido::trajectory::TrajectoryPtr trajectory)
{
  dart::dynamics::BodyNodePtr endEffector
      = mAda->getMetaSkeleton()->getBodyNode("j2n6s200_forque_end_effector");

  if (!mViewer)
  {
    ROS_WARN("Visualize trajectory: viewer is nullptr");
  }
  else if (!endEffector)
  {
    ROS_WARN("Visualize trajectory: endEffector is nullptr");
  }
  else if (!trajectory)
  {
    ROS_WARN("Visualize trajectory: trajectory is nullptr");
  }
  else
  {
    mViewer->removeTrajectoryMarker(trajectoryMarkerPtr);
    trajectoryMarkerPtr = nullptr;
    trajectoryMarkerPtr = mViewer->addTrajectoryMarker(
        trajectory,
        mAda->getArm()->getMetaSkeleton(),
        *endEffector,
        Eigen::Vector4d{1, 1, 1, 1},
        0.01,
        16u);
  }
}

//==============================================================================
void FeedingDemo::moveAbovePlateAnywhere(
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  double heightAbovePlate
      = getRosParam<double>("/feedingDemo/heightAbovePlate", mNodeHandle);
  double horizontalToleranceAbovePlate = getRosParam<double>(
      "/planning/tsr/horizontalToleranceAbovePlate", mNodeHandle);
  double verticalToleranceAbovePlate = getRosParam<double>(
      "/planning/tsr/verticalToleranceAbovePlate", mNodeHandle);

  static std::default_random_engine generator(time(0));
  static std::uniform_real_distribution<double> distribution(-0.08, 0.08);
  static std::uniform_real_distribution<double> distribution2(-M_PI, M_PI);
  double randX = distribution(generator);
  double randY = distribution(generator);
  double angle = distribution2(generator);
  // double randX = 0, randY = 0;

  auto abovePlateTSR = pr_tsr::getDefaultPlateTSR();
  abovePlateTSR.mT0_w
      = mWorkspace->getPlate()->getRootBodyNode()->getWorldTransform();
  abovePlateTSR.mTw_e.translation() = Eigen::Vector3d{randX, randY, 0.07};

  // abovePlateTSR.mBw = createBwMatrixForTSR(0.1, verticalToleranceAbovePlate,
  // -M_PI, M_PI);
  abovePlateTSR.mBw = createBwMatrixForTSR(
      0.005, verticalToleranceAbovePlate, -M_PI * 0.1, M_PI * 0.1);
  Eigen::Isometry3d eeTransform
      = *mAda->getHand()->getEndEffectorTransform("plate");
  eeTransform.linear()
      = eeTransform.linear()
        * Eigen::Matrix3d(Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitZ()));
  abovePlateTSR.mTw_e.matrix() *= eeTransform.matrix();

  if (viewer)
  {
    // tsrMarkers.push_back(viewer->addTSRMarker(abovePlateTSR, 100,
    // "someTSRName"));
  }

  bool trajectoryCompleted
      = mAda->moveArmToTSR(abovePlateTSR, mCollisionFreeConstraint);
  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

//==============================================================================
void FeedingDemo::moveAboveFood(
    const Eigen::Isometry3d& foodTransform,
    int pickupAngleMode,
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer,
    bool useAngledTranslation)
{
  double heightAboveFood
      = getRosParam<double>("/feedingDemo/heightAboveFood", mNodeHandle);
  // If the robot is not simulated, we want to plan the trajectory to move a
  // little further downwards,
  // so that the MoveUntilTouchController can take care of stopping the
  // trajectory.
  double horizontalToleranceNearFood = getRosParam<double>(
      "/planning/tsr/horizontalToleranceNearFood", mNodeHandle);
  double verticalToleranceNearFood = getRosParam<double>(
      "/planning/tsr/verticalToleranceNearFood", mNodeHandle);

  aikido::constraint::dart::TSR aboveFoodTSR;
  Eigen::Isometry3d eeTransform
      = *mAda->getHand()->getEndEffectorTransform("food");
  if (pickupAngleMode == 0)
  {
    aboveFoodTSR.mT0_w = foodTransform;
    // eeTransform.linear() = eeTransform.linear() *
    // Eigen::Matrix3d(Eigen::AngleAxisd(0.5 * , Eigen::Vector3d::UnitX()));
  }
  else
  {
    Eigen::Isometry3d defaultFoodTransform = Eigen::Isometry3d::Identity();
    defaultFoodTransform.translation() = foodTransform.translation();
    aboveFoodTSR.mT0_w = defaultFoodTransform;
    // celery-style
    // eeTransform.linear() = eeTransform.linear() *
    // Eigen::Matrix3d(Eigen::AngleAxisd( M_PI * 0.5, Eigen::Vector3d::UnitZ())
    // * Eigen::AngleAxisd( M_PI - angle + 0.5, Eigen::Vector3d::UnitX()));

    if (pickupAngleMode == 1)
    {
      // strawberry-style
      eeTransform.linear()
          = eeTransform.linear()
            * Eigen::Matrix3d(
                  Eigen::AngleAxisd(-M_PI * 0.5, Eigen::Vector3d::UnitZ())
                  * Eigen::AngleAxisd(M_PI + 0.5, Eigen::Vector3d::UnitX()));
    }
    else
    {
      // banana-style
      eeTransform.linear()
          = eeTransform.linear()
            * Eigen::Matrix3d(
                  Eigen::AngleAxisd(M_PI * 0.5, Eigen::Vector3d::UnitZ())
                  * Eigen::AngleAxisd(M_PI + 0.5, Eigen::Vector3d::UnitX()));
    }
  }
  aboveFoodTSR.mBw = createBwMatrixForTSR(
      horizontalToleranceNearFood, verticalToleranceNearFood, 0, 0);
  aboveFoodTSR.mTw_e.matrix() *= eeTransform.matrix();

  float distance = heightAboveFood * 0.85;

  if (pickupAngleMode == 0)
  {
    // vertical
    aboveFoodTSR.mTw_e.translation() = Eigen::Vector3d{-0.01, 0, -distance};
  }
  else if (pickupAngleMode == 1)
  {
    // strawberry style angled
    aboveFoodTSR.mTw_e.translation() = Eigen::Vector3d{0, 0, distance};
  }
  else
  {
    // banana style angled
    aboveFoodTSR.mTw_e.translation() = Eigen::Vector3d{
        -sin(M_PI * 0.25) * distance, 0, cos(M_PI * 0.25) * distance};
  }

  // tsrMarkers.push_back(viewer->addTSRMarker(aboveFoodTSR, 100,
  // "someTSRName"));
  // std::this_thread::sleep_for(std::chrono::milliseconds(20000));

  auto trajectory = mAda->planArmToTSR(aboveFoodTSR, mCollisionFreeConstraint);
  visualizeTrajectory(trajectory);
  bool trajectoryCompleted = mAda->moveArmOnTrajectory(
      trajectory,
      mCollisionFreeConstraint,
      ada::TrajectoryPostprocessType::KUNZ);

  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

//==============================================================================
bool FeedingDemo::moveIntoFood()
{
  auto length
      = getRosParam<double>("/feedingDemo/heightAboveFood", mNodeHandle)
        + getRosParam<double>("/feedingDemo/heightIntoFood", mNodeHandle);

  bool trajectoryCompleted = mAda->moveArmToEndEffectorOffset(
      Eigen::Vector3d(0, 0, -1), length, mCollisionFreeConstraint);

  // trajectoryCompleted might be false because the forque hit the food
  // along the way and the trajectory was aborted
  return true;
}

//==============================================================================
bool FeedingDemo::moveIntoFood(
    Perception* perception,
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  ROS_INFO("Servoing into food");
  std::shared_ptr<aikido::control::TrajectoryExecutor> executor
      = mAda->getTrajectoryExecutor();

  std::shared_ptr<aikido::control::ros::RosTrajectoryExecutor> rosExecutor
      = std::dynamic_pointer_cast<aikido::control::ros::RosTrajectoryExecutor>(
          executor);

  if (rosExecutor == nullptr)
  {
    throw std::runtime_error("no ros executor");
  }

  int numDofs = mAda->getArm()->getMetaSkeleton()->getNumDofs();
  Eigen::VectorXd velocityLimits = Eigen::VectorXd::Ones(numDofs) * 0.2;

  PerceptionServoClient servoClient(
      mNodeHandle,
      boost::bind(&Perception::perceiveFood, perception, _1),
      mArmSpace,
      mAda,
      mAda->getArm()->getMetaSkeleton(),
      mAda->getHand()->getEndEffectorBodyNode(),
      rosExecutor,
      mCollisionFreeConstraint,
      0.1,
      velocityLimits,
      0.1,
      0.002);
  servoClient.start();

  return servoClient.wait(10000.0);
}

//==============================================================================
void FeedingDemo::moveOutOfFood()
{
  auto length
      = getRosParam<double>("/feedingDemo/heightAboveFood", mNodeHandle) * 0.75;

  bool trajectoryCompleted = mAda->moveArmToEndEffectorOffset(
      Eigen::Vector3d(0, 0, 1), length, nullptr);
  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

//==============================================================================
void FeedingDemo::moveOutOfFood(float dist)
{
  bool trajectoryCompleted = mAda->moveArmToEndEffectorOffset(
      Eigen::Vector3d(0, 0, 1), dist, nullptr);
  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

//==============================================================================
bool FeedingDemo::moveInFrontOfPerson()
{
  double distanceToPerson
      = getRosParam<double>("/feedingDemo/distanceToPerson", mNodeHandle);
  double horizontalToleranceNearPerson = getRosParam<double>(
      "/planning/tsr/horizontalToleranceNearPerson", mNodeHandle);
  double verticalToleranceNearPerson = getRosParam<double>(
      "/planning/tsr/verticalToleranceNearPerson", mNodeHandle);

  aikido::constraint::dart::TSR personTSR;
  Eigen::Isometry3d personPose = Eigen::Isometry3d::Identity();
  personPose.translation() = mWorkspace->getPersonPose()
                                 .translation(); // + Eigen::Vector3d(0,0.2,0);
  personPose.linear()
      = Eigen::Matrix3d(Eigen::AngleAxisd(M_PI, Eigen::Vector3d::UnitZ()));
  personTSR.mT0_w = personPose;
  personTSR.mTw_e.translation() = Eigen::Vector3d{0, distanceToPerson, 0};

  personTSR.mBw = createBwMatrixForTSR(
      horizontalToleranceNearPerson, verticalToleranceNearPerson, 0, 0);
  personTSR.mTw_e.matrix()
      *= mAda->getHand()->getEndEffectorTransform("person")->matrix();

  std::vector<double> velocityLimits{0.2, 0.2, 0.2, 0.2, 0.2, 0.4};
  auto trajectory = mAda->planArmToTSR(personTSR, mCollisionFreeConstraint);
  visualizeTrajectory(trajectory);
  return mAda->moveArmOnTrajectory(
      trajectory,
      mCollisionFreeConstraint,
      ada::TrajectoryPostprocessType::KUNZ,
      velocityLimits);
}

//==============================================================================
bool FeedingDemo::tiltUpInFrontOfPerson(
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  printRobotConfiguration();

  // Eigen::Vector3d workingPersonTranslation(0.283465, 0.199386, 0.652674);
  Eigen::Vector3d workingPersonTranslation(0.263, 0.269386, 0.652674);
  std::vector<double> tiltOffsetVector
      = getRosParam<std::vector<double>>("/study/personPose", mNodeHandle);
  Eigen::Vector3d tiltOffset{
      tiltOffsetVector[0], tiltOffsetVector[1], tiltOffsetVector[2]};
  Eigen::Vector3d personTranslation
      = mAda->getHand()->getEndEffectorBodyNode()->getTransform().translation()
        + tiltOffset;
  Eigen::Vector3d correctionTranslation
      = workingPersonTranslation - personTranslation;

  for (double i = 0; i <= 1.0; i += 0.5)
  {
    aikido::constraint::dart::TSR personTSR;
    Eigen::Isometry3d personPose = Eigen::Isometry3d::Identity();
    personPose.translation() = personTranslation + correctionTranslation * i;
    ROS_INFO_STREAM(
        "personTranslation: " << personPose.translation().matrix().transpose());
    personPose.linear()
        = Eigen::Matrix3d(Eigen::AngleAxisd(M_PI, Eigen::Vector3d::UnitZ()));
    personTSR.mT0_w = personPose;
    personTSR.mTw_e.translation() = Eigen::Vector3d{0, 0, 0};

    personTSR.mBw = createBwMatrixForTSR(0.02, 0.02, -M_PI / 4, M_PI / 4);
    Eigen::Isometry3d eeTransform
        = *mAda->getHand()->getEndEffectorTransform("person");
    eeTransform.linear()
        = eeTransform.linear()
          * Eigen::Matrix3d(
                Eigen::AngleAxisd(M_PI * -0.25, Eigen::Vector3d::UnitY())
                * Eigen::AngleAxisd(M_PI * 0.25, Eigen::Vector3d::UnitX()));
    personTSR.mTw_e.matrix() *= eeTransform.matrix();

    // auto markers = viewer->addTSRMarker(personTSR, 100, "personTSRSamples");

    bool trajectoryCompleted = false;
    try
    {
      auto trajectory = mAda->planArmToTSR(personTSR, mCollisionFreeConstraint);
      visualizeTrajectory(trajectory);
      bool trajectoryCompleted = mAda->moveArmOnTrajectory(
          trajectory,
          mCollisionFreeConstraint,
          ada::TrajectoryPostprocessType::KUNZ);
    }
    catch (std::runtime_error e)
    {
      ROS_WARN("tilt up trajectory failed!");
      continue;
    }
    return trajectoryCompleted;
  }
  return false;
}

//==============================================================================
void FeedingDemo::tiltDownInFrontOfPerson(
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  printRobotConfiguration();
  Eigen::Vector3d workingPersonTranslation(0.269274, 0.191136, 0.71243);
  Eigen::Vector3d personTranslation;
  personTranslation
      = mAda->getHand()->getEndEffectorBodyNode()->getTransform().translation()
        + Eigen::Vector3d{0.01, 0, 0.06} + Eigen::Vector3d{0, 0, 0.0};
  Eigen::Vector3d correctionTranslation
      = workingPersonTranslation - personTranslation;

  for (double i = 0; i <= 1.0; i += 0.2)
  {
    aikido::constraint::dart::TSR personTSR;
    Eigen::Isometry3d personPose = Eigen::Isometry3d::Identity();
    personPose.translation() = personTranslation + correctionTranslation * i;
    ROS_INFO_STREAM(
        "personTranslation: " << personPose.translation().matrix().transpose());
    personPose.linear()
        = Eigen::Matrix3d(Eigen::AngleAxisd(M_PI, Eigen::Vector3d::UnitZ()));
    personTSR.mT0_w = personPose;
    personTSR.mTw_e.translation() = Eigen::Vector3d{0, 0, 0};

    personTSR.mBw = createBwMatrixForTSR(0.02, 0.02, -M_PI / 4, M_PI / 4);
    Eigen::Isometry3d eeTransform
        = *mAda->getHand()->getEndEffectorTransform("person");
    eeTransform.linear()
        = eeTransform.linear()
          * Eigen::Matrix3d(
                Eigen::AngleAxisd(-M_PI * 0.25, Eigen::Vector3d::UnitX()));
    personTSR.mTw_e.matrix() *= eeTransform.matrix();

    bool trajectoryCompleted = false;
    try
    {
      auto trajectory = mAda->planArmToTSR(personTSR, mCollisionFreeConstraint);
      visualizeTrajectory(trajectory);
      bool trajectoryCompleted = mAda->moveArmOnTrajectory(
          trajectory,
          mCollisionFreeConstraint,
          ada::TrajectoryPostprocessType::KUNZ);
    }
    catch (std::runtime_error e)
    {
      ROS_WARN("tilt down trajectory failed!");
      continue;
    }
    if (trajectoryCompleted)
    {
      return;
    }
    else
    {
      ROS_WARN("aborting tilt down!");
      return;
    }
  }
}

void FeedingDemo::moveDirectlyToPerson(
    bool tilted, aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  double distanceToPerson = 0.02;
  double horizontalToleranceNearPerson = getRosParam<double>(
      "/planning/tsr/horizontalToleranceNearPerson", mNodeHandle);
  double verticalToleranceNearPerson = getRosParam<double>(
      "/planning/tsr/verticalToleranceNearPerson", mNodeHandle);

  Eigen::Isometry3d personPose = createIsometry(
      getRosParam<std::vector<double>>("/study/personPose", mNodeHandle));
  if (tilted)
  {
    std::vector<double> tiltOffsetVector
        = getRosParam<std::vector<double>>("/study/tiltOffset", mNodeHandle);
    Eigen::Vector3d tiltOffset{
        tiltOffsetVector[0], tiltOffsetVector[1], tiltOffsetVector[2]};
    personPose.translation() += tiltOffset;
  }

  aikido::constraint::dart::TSR personTSR;
  personTSR.mT0_w = personPose;
  personTSR.mTw_e.translation() = Eigen::Vector3d{0, distanceToPerson, 0};

  if (tilted)
  {
    personTSR.mBw = createBwMatrixForTSR(
        horizontalToleranceNearPerson,
        verticalToleranceNearPerson,
        -M_PI / 4,
        M_PI / 4);
    Eigen::Isometry3d eeTransform
        = *mAda->getHand()->getEndEffectorTransform("person");
    eeTransform.linear()
        = eeTransform.linear()
          * Eigen::Matrix3d(
                Eigen::AngleAxisd(M_PI * -0.25, Eigen::Vector3d::UnitY())
                * Eigen::AngleAxisd(M_PI * 0.25, Eigen::Vector3d::UnitX()));
    personTSR.mTw_e.matrix() *= eeTransform.matrix();
  }
  else
  {
    personTSR.mBw = createBwMatrixForTSR(
        horizontalToleranceNearPerson, verticalToleranceNearPerson, 0, 0);
    personTSR.mTw_e.matrix()
        *= mAda->getHand()->getEndEffectorTransform("person")->matrix();
  }

  // tsrMarkers.push_back(viewer->addTSRMarker(personTSR, 100, "someTSRName"));

  std::vector<double> velocityLimits{0.2, 0.2, 0.2, 0.2, 0.2, 0.4};
  auto trajectory = mAda->planArmToTSR(personTSR, mCollisionFreeConstraint);
  visualizeTrajectory(trajectory);
  bool trajectoryCompleted = mAda->moveArmOnTrajectory(
      trajectory,
      mCollisionFreeConstraint,
      ada::TrajectoryPostprocessType::KUNZ,
      velocityLimits);
  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

//==============================================================================
bool FeedingDemo::moveTowardsPerson()
{
  return mAda->moveArmToEndEffectorOffset(
      Eigen::Vector3d(0, 1, -0.6),
      getRosParam<double>("/feedingDemo/distanceToPerson", mNodeHandle) * 0.9,
      mCollisionFreeConstraint);
}

//==============================================================================
bool FeedingDemo::moveTowardsPerson(
    Perception* perception,
    aikido::rviz::WorldInteractiveMarkerViewerPtr viewer)
{
  if (!mAdaReal)
  {
    moveTowardsPerson();
    return true;
  }

  std::shared_ptr<aikido::control::TrajectoryExecutor> executor
      = mAda->getTrajectoryExecutor();
  std::shared_ptr<aikido::control::ros::RosTrajectoryExecutor> rosExecutor
      = std::dynamic_pointer_cast<aikido::control::ros::RosTrajectoryExecutor>(
          executor);

  if (rosExecutor == nullptr)
  {
    throw std::runtime_error("no ros executor");
  }

  int numDofs = mAda->getArm()->getMetaSkeleton()->getNumDofs();
  Eigen::VectorXd velocityLimits = Eigen::VectorXd::Zero(numDofs);
  for (int i = 0; i < numDofs; i++)
    velocityLimits[i] = 0.2;

  feeding::PerceptionServoClient servoClient(
      mNodeHandle,
      boost::bind(&Perception::perceiveFace, perception, _1),
      mArmSpace,
      mAda,
      mAda->getArm()->getMetaSkeleton(),
      mAda->getHand()->getEndEffectorBodyNode(),
      rosExecutor,
      mCollisionFreeConstraint,
      0.2,
      velocityLimits,
      0,
      0.06);
  servoClient.start();
  return servoClient.wait(30);
}

//==============================================================================
void FeedingDemo::moveAwayFromPerson()
{
  Eigen::Vector3d targetPosition
      = mWorkspace->getPersonPose().translation() + Eigen::Vector3d(0, -0.2, 0);
  Eigen::Vector3d forqueTipPosition
      = mAda->getHand()->getEndEffectorBodyNode()->getTransform().translation();
  Eigen::Vector3d direction = targetPosition - forqueTipPosition;

  bool trajectoryCompleted = mAda->moveArmToEndEffectorOffset(
      direction.normalized(), direction.norm(), mCollisionFreeConstraint);

  if (!trajectoryCompleted)
  {
    throw std::runtime_error("Trajectory execution failed");
  }
}

aikido::rviz::WorldInteractiveMarkerViewerPtr FeedingDemo::getViewer()
{
  return mViewer;
}
};
