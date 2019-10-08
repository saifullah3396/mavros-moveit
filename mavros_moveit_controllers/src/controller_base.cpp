#include <tf/tf.h>
#include <mavros_moveit_controllers/controller_base.h>
#include <mavros_moveit_controllers/position_controller.h>
#include <mavros_moveit_controllers/velocity_controller.h>
#include <mavros_moveit_utils/utils.h>

namespace mavros_moveit_controllers {

void ControllerBase::init() 
{
    ros::NodeHandle p_nh("~");
    // setpoint publishing rate MUST be faster than 2Hz. From mavros documentation
    double rate;
    p_nh.param("rate", rate, 100.0);
    rate_ = ros::Rate(rate);
    std::string frame_id;
    p_nh.param<std::string>("frame_id", frame_id, "map");
    target_.header.frame_id = frame_id;
    int coordinate_frame;
    p_nh.param("coordinate_frame", coordinate_frame, (int)mavros_msgs::PositionTarget::FRAME_LOCAL_NED);
    target_.coordinate_frame = coordinate_frame;

    // setup publishers/subscribers/services
    state_sub_ = nh_.subscribe<mavros_msgs::State>("mavros/state", 5, &ControllerBase::stateCb, this);
    local_pose_sub_ = nh_.subscribe<geometry_msgs::PoseStamped>("mavros/local_position/pose", 5, &ControllerBase::poseCb, this);
    local_cmd_pose_sub_ = nh_.subscribe<geometry_msgs::PoseStamped>("mavros/local_position/cmd_pose", 5, &ControllerBase::poseCb, this);
    local_raw_pub_ = nh_.advertise<mavros_msgs::PositionTarget>("mavros/setpoint_raw/local", 5);
}

ControllerBase* ControllerBase::makeController(const std::string& type) {
    ControllerBase* controller;
    if (type == "position")
        controller = new PositionController();
    else if (type == "velocity")
        controller = new VelocityController();
    else
        ROS_FATAL(
            "Cannot initialize controller of type '%s'. \
            Possible types are: 'position' and 'velocity'", type.c_str());
    return controller;
}

void ControllerBase::update() {
    while(ros::ok()){
        ros::spinOnce();
        rate_.sleep();
    }
}

bool ControllerBase::statusCheck()
{
    // is mavros connected to px4?
    if (!mavros_state_.connected) {
        ROS_WARN("Controller cannot generate command since mavros is not connected to px4.");
        return false;
    }

    // robot state is received yet?
    if (!state_received_) {
        ROS_WARN("Controller cannot generate command due to unknown mavros state.");
        return false;
    }

    // robot current pose is received yet?
    if (!pose_received_) {
        ROS_WARN("Controller cannot generate command due to unknown robot pose.");
        return false;
    }

    // is robot armed?
    if (!mavros_state_.armed) {
        ROS_WARN("Controller cannot generate command due to unarmed robot.");
        return false;
    }

    // is robot armed?
    if (!mavros_state_.armed) {
        ROS_WARN("Controller cannot generate command due to unarmed robot.");
        return false;
    }

    if (mavros_state_.mode != "OFFBOARD") {
        ROS_WARN("Controller cannot generate command unless robot mode is set to OFFBOARD.");
        return false;
    }
    return true;
}

bool ControllerBase::targetReached(const tf::Pose& target) {
    tf::Pose tf_curr;
    tf::poseMsgToTF(current_pose_.pose, tf_curr);
    auto diff_t = tf_curr.inverseTimes(target);
    auto yaw = mavros_moveit_utils::getYaw(diff_t.getRotation());
    if (fabsf(diff_t.getOrigin().x()) <= target_pos_tol && 
        fabsf(diff_t.getOrigin().y()) <= target_pos_tol &&
        fabsf(diff_t.getOrigin().z()) <= target_pos_tol &&
        fabsf(yaw) <= target_orientation_tol)
    {
        return true;
    }
    return false;
}

void ControllerBase::stateCb(const mavros_msgs::State::ConstPtr& mavros_state) {
    mavros_state_ = *mavros_state;
    state_received_ = true;
}

void ControllerBase::poseCb(const geometry_msgs::PoseStamped::ConstPtr& current_pose) {
    current_pose_ = *current_pose;
    pose_received_ = true;
}

}