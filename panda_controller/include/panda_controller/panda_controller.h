#include <mutex>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <random>

#include <ros/ros.h>
#include <ros/console.h>
#include <ros/package.h>

#include "mujoco_ros_msgs/JointSet.h"

#include "panda_controller/mujoco_interface.h"

#include <rbdl/rbdl.h>
#include <rbdl/addons/urdfreader/urdfreader.h>

#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/planning_scene_interface/planning_scene_interface.h>
#include <moveit_msgs/DisplayRobotState.h>
#include <moveit_msgs/DisplayTrajectory.h>
#include <moveit_msgs/CollisionObject.h>
#include <moveit_visual_tools/moveit_visual_tools.h>

class PandaController{
    public:
        PandaController(ros::NodeHandle &nh, DataContainer &dc);
        ~PandaController();
        void compute();
        void updateKinematicsDynamics();
        void computeControlInput();
        void initMoveit();
        void setMoveitObstables();
        void generateRandTraj();
        Eigen::Vector3d quintic_spline(double time, double time_0, double time_f, double x_0, double x_dot_0, double x_ddot_0, double x_f, double x_dot_f, double x_ddot_f);

    private:
        std::mutex m_dc_;
        std::mutex m_ci_;
        DataContainer &dc_;

        bool is_init_ = false;

        double sim_time_ = 0.0;

        // Robot State
        Eigen::VectorXd q_;
        Eigen::VectorXd q_dot_;
        Eigen::VectorXd effort_;

        // Control
        Eigen::VectorXd qddot_desired_;
        Eigen::VectorXd qdot_desired_;
        Eigen::VectorXd q_desired_;

        double kv, kp;

        Eigen::VectorXd control_input_;

        // Kinematics & Dynamics
        RigidBodyDynamics::Model robot_;
        Eigen::VectorXd non_linear_;
        Eigen::MatrixXd A_;

        // Moveit
        inline static const std::string PLANNING_GROUP="panda_arm";
        moveit::planning_interface::MoveGroupInterface move_group_;
        moveit::planning_interface::PlanningSceneInterface planning_scene_interface_;
        moveit::planning_interface::MoveGroupInterface::Plan random_plan_;

        Eigen::VectorXd q_limit_u_;
        Eigen::VectorXd q_limit_l_;

        double traj_init_time_;
};