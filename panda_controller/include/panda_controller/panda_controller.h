#include <mutex>
#include <random>
#include <math.h>

#include <ros/ros.h>
#include <ros/console.h>
#include <ros/package.h>

#include "panda_controller/util.h"

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

#include <torch/script.h> 
#include <fstream>

#include <std_msgs/Float32MultiArray.h>

# define MODE_INIT 105
# define MODE_HOME 104
# define MODE_RANDOM 114
# define MODE_FORCE 102
# define MODE_STOP 115

class PandaController{
    public:
        PandaController(ros::NodeHandle &nh, DataContainer &dc, int control_mode);
        ~PandaController();
        void compute();
        void updateKinematicsDynamics();
        void computeControlInput();
        void logData();
        void printData();
        void initMoveit();
        void setMoveitObstables();
        void generateRandTrajThread();
        void generateRandTraj();
        void loadNetwork();
        void writeBuffer();
        void computeBackwardDynamicsModel();
        void computeForwardDynamicsModel();
        void computeTrainedModel();
        void computeExtForce();
        Eigen::Matrix7d getC(Eigen::Vector7d q, Eigen::Vector7d q_dot);
        void computeSOSML();
        void computeESO();
        void computeHOFTO();
        void publishResidual();

    private:
        double hz_ = 2000;
        double cur_time_;
        double pre_time_;
        double init_time_;

        int mode_ = 0;
        double mode_init_time_ =  0.0;
        Eigen::VectorXd q_mode_init_;
        Eigen::VectorXd q_dot_mode_init_;
        Eigen::Isometry3d x_mode_init_;

        std::mutex m_dc_;
        std::mutex m_ci_;
        std::mutex m_ext_;
        std::mutex m_buffer_;
        std::mutex m_rbdl_;

        DataContainer &dc_;

        bool is_init_ = false;

        double sim_time_ = 0.0;

        bool is_write_ = false;
        std::ofstream writeFile;

        // Robot State
        Eigen::Vector7d q_;
        Eigen::Vector7d q_dot_;
        Eigen::Vector7d effort_;

        Eigen::Isometry3d x_;
        Eigen::VectorXd x_dot_;
        Eigen::MatrixXd j_temp_;
        Eigen::MatrixXd j_;

        // Control
        Eigen::VectorXd q_ddot_desired_;
        Eigen::VectorXd q_dot_desired_;
        Eigen::VectorXd q_desired_;

        Eigen::Isometry3d x_target_;
        Eigen::Isometry3d x_desired_;
        Eigen::VectorXd x_dot_desired_;
        Eigen::Isometry3d x_ddot_desired_;

        Eigen::MatrixXd kv, kp;
        Eigen::MatrixXd kv_task_, kp_task_;

        Eigen::VectorXd control_input_;
        Eigen::Vector7d control_input_filtered_;

        // Kinematics & Dynamics
        RigidBodyDynamics::Model robot_;
        Eigen::VectorXd non_linear_;
        Eigen::MatrixXd A_;
        Eigen::MatrixXd C_;
        
        Eigen::MatrixXd Lambda_;

        // Moveit
        inline static const std::string PLANNING_GROUP="panda_arm";
        moveit::planning_interface::MoveGroupInterface move_group_;
        moveit::planning_interface::PlanningSceneInterface planning_scene_interface_;
        moveit::planning_interface::MoveGroupInterface::Plan random_plan_;
        moveit::planning_interface::MoveGroupInterface::Plan random_plan_next_;

        std::vector<double> q_target_plan_;
        std::vector<double> q_init_plan_; 
        std::vector<double> q_dot_plan_;

        Eigen::VectorXd q_limit_u_;
        Eigen::VectorXd q_limit_l_;

        double traj_init_time_ = 0.0;
        double traj_duration_ = 0.0;
        int total_waypoints_;
        int cur_waypoint_ = 0;

        bool init_traj_prepared_ = false;
        bool next_traj_prepared_ = false;

        // Torch
        static const int num_seq = 5;
        static const int num_features = 2;
        static const int num_joint = 7;

        float ring_buffer_[num_seq*num_features*num_joint];
        float ring_buffer_control_input_[num_seq*num_joint];
        int ring_buffer_idx_ = 0;

        float max_theta_ = 3.14;
        float min_theta_ = -3.14;
        float max_theta_dot_ = 0.3;
        float min_theta_dot_ = -0.3;
        Eigen::Matrix<float, num_joint, 1> output_scaling;

        static const int num_hidden_neurons_ = 200;

        Eigen::Matrix<float, num_hidden_neurons_, num_seq*num_features*num_joint> backward_W0;
        Eigen::Matrix<float, num_hidden_neurons_, 1> backward_b0;
        Eigen::Matrix<float, num_hidden_neurons_, num_hidden_neurons_> backward_W2;
        Eigen::Matrix<float, num_hidden_neurons_, 1> backward_b2;
        Eigen::Matrix<float, num_joint, num_hidden_neurons_> backward_W4;
        Eigen::Matrix<float, num_joint, 1> backward_b4;

        Eigen::Matrix<float, num_hidden_neurons_, (num_seq-1)*num_features*num_joint + num_joint> forward_W0;
        Eigen::Matrix<float, num_hidden_neurons_, 1> forward_b0;
        Eigen::Matrix<float, num_hidden_neurons_, num_hidden_neurons_> forward_W2;
        Eigen::Matrix<float, num_hidden_neurons_, 1> forward_b2;
        Eigen::Matrix<float, num_features*num_joint, num_hidden_neurons_> forward_W4;
        Eigen::Matrix<float, num_features*num_joint, 1> forward_b4;

        Eigen::Matrix<float, (num_seq-1)*num_features*num_joint, 1> condition_;
        Eigen::Matrix<float, num_features*num_joint, 1> state_;
        Eigen::Matrix<float, num_joint, 1> input_;

        Eigen::Matrix<float, num_hidden_neurons_, 1> backward_layer1_;
        Eigen::Matrix<float, num_hidden_neurons_, 1> backward_layer2_;
        Eigen::Matrix<float, num_joint, 1> backward_network_output_;

        Eigen::Matrix<float, num_hidden_neurons_, 1> forward_layer1_;
        Eigen::Matrix<float, num_hidden_neurons_, 1> forward_layer2_;
        Eigen::Matrix<float, num_features*num_joint, 1> forward_network_output_;
        
        Eigen::Vector7d network_output_share_;


        // Residual publish
        ros::Publisher resi_publisher_;
        std_msgs::Float32MultiArray resi_msg_;
        float resi_buffer_[num_seq*num_joint];
        int resi_buffer_idx_ = 0;


        // Force Control
        double f_I_ = 0.0;
        double f_d_x_ = 0.0;

        Eigen::Matrix<double, 6, 7> j_dyn_cons_inv_T_;

        Eigen::VectorXd estimated_ext_torque_LSTM_;
        Eigen::Vector7d estimated_ext_torque_SOSML_;
        Eigen::Vector7d estimated_ext_torque_ESO_;
        Eigen::Vector7d estimated_ext_torque_HOFTO_;
        Eigen::VectorXd measured_ext_torque_;
        
        Eigen::VectorXd estimated_ext_force_;
        Eigen::VectorXd estimated_ext_force_SOSML_;
        Eigen::VectorXd estimated_ext_force_ESO_;
        Eigen::VectorXd estimated_ext_force_HOFTO_;
        Eigen::VectorXd estimated_ext_force_pre_;
        Eigen::VectorXd estimated_ext_force_init_;
        Eigen::VectorXd measured_ext_force_;

        // Second Order Sliding Mode Momentum Observer(SOSML)
        Eigen::Vector7d p_;
        Eigen::Vector7d p_hat_;
        Eigen::Vector7d p_tilde_;
        Eigen::Vector7d p_tilde_sign_;
        
        Eigen::VectorXd g_;
        
        Eigen::Vector7d sigma_;

        Eigen::Matrix7d T1_, T2_;
        Eigen::Matrix7d S1_, S2_;

        // Extended State Observer(ESO)
        Eigen::Vector7d x1_;
        Eigen::Vector7d x1_hat_;
        Eigen::Vector7d x1_tilde_;
        Eigen::Vector7d x2_hat_;
        Eigen::Vector7d x3_hat_;
        
        double eta1_, eta2_;
        double epsilon_;

        Eigen::MatrixXd A_ESO_;
        Eigen::Matrix7d C_ESO_;
        Eigen::VectorXd g_ESO_;

        // High-Order Finite Time Observer(HOFTO)
        Eigen::Vector7d z1_, z2_, z3_, z4_;
        Eigen::Vector7d x1_z1_diff_;
        Eigen::DiagonalMatrix<double, 7> L1_, L2_, L3_, L4_;
        Eigen::Vector7d x1_HOFTO_, x2_HOFTO_;
        double m2_, m3_, m4_, m5_;

        Eigen::MatrixXd A_HOFTO_;
        Eigen::VectorXd non_linear_HOFTO_;
};