#include <ros/ros.h>
#include <costmap_2d/costmap_2d_ros.h>
#include <costmap_2d/costmap_2d.h>
#include <nav_core/base_global_planner.h>
#include <geometry_msgs/PoseStamped.h>
#include <angles/angles.h>
#include <base_local_planner/world_model.h>
#include <base_local_planner/costmap_model.h>

#include <string>
#include <vector>
#include <queue>
#include <limits>
#include <tf/tf.h>
#include <iterator>
#include <algorithm>

#include "alice_msgs/MakePlan.h"


namespace test_planner {
class GlobalPlannerPython : public nav_core::BaseGlobalPlanner {

public:
  GlobalPlannerPython();
  GlobalPlannerPython(std::string name, costmap_2d::Costmap2DROS *costmap_ros);

  void initialize(std::string name, costmap_2d::Costmap2DROS *costmap_ros);
  bool makePlan(const geometry_msgs::PoseStamped &start,
      const geometry_msgs::PoseStamped &goal,
      std::vector<geometry_msgs::PoseStamped> &plan
      );

  size_t ToIndex(float x, float y);
  void FromIndex(size_t index, int &x, int &y);
  void FromPositionToIndex(float &x, float &y);
  void FromIndexToPosition(float &x, float &y);

private:
  costmap_2d::Costmap2DROS *costmap_ros_;
  costmap_2d::Costmap2D *costmap_;
  float origin_x_;
  float origin_y_;
  float resolution_;
  int width_;
  int height_;
  int map_size_;
  bool initialized_;

  ros::ServiceClient makeplan_service_;

};
}
