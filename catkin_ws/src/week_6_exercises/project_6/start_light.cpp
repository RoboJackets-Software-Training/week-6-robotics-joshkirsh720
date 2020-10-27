#include <ros/ros.h>
#include <ros/publisher.h>

#include <opencv2/opencv.hpp>

#include <cv_bridge/cv_bridge.h>

#include <std_msgs/Bool.h>
#include <sensor_msgs/Image.h>

ros::Publisher raceStart_pub, debugImage_pub;
bool race_started=false;
double time_red_off=0;


cv::Mat kernel(int x, int y) {
	return cv::getStructuringElement(cv::MORPH_RECT, cv::Size(x,y));
}


bool hasLight(std::vector<std::vector<cv::Point>> contours) {
	//check if red light is on
	bool has_light = false;
	for(auto contour : contours) {
		double area = cv::contourArea(contour, false);
		double perimeter = cv::arcLength(contour, true);
		double circularity = 4 * M_PI * (area / (perimeter * perimeter));

		if(circularity > .7 && area > 200) {
			has_light = true;
			break;
		}
	}

	return has_light;
}

void img_callback(const sensor_msgs::ImageConstPtr &msg) {
	cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
	cv::Mat frame = cv_ptr->image;

	cv::Mat hsv_frame, green_threshold_frame, red_threshold_frame;
	cv::cvtColor(frame, hsv_frame, cv::COLOR_BGR2HSV);
	cv::inRange(hsv_frame, cv::Scalar(80, 120, 120), cv::Scalar(140, 255, 255), green_threshold_frame);
	cv::inRange(hsv_frame, cv::Scalar(0, 120, 120), cv::Scalar(40, 255, 255), red_threshold_frame);


	//some cleaning up for each
	cv::GaussianBlur(red_threshold_frame, red_threshold_frame, cv::Size(3,3), 0, 0);
	cv::morphologyEx(red_threshold_frame, red_threshold_frame, cv::MORPH_OPEN, kernel(3,3));
	cv::morphologyEx(red_threshold_frame, red_threshold_frame, cv::MORPH_CLOSE, kernel(3,3));

	cv::GaussianBlur(green_threshold_frame, green_threshold_frame, cv::Size(3,3), 0, 0);
	cv::morphologyEx(green_threshold_frame, green_threshold_frame, cv::MORPH_OPEN, kernel(3,3));
	cv::morphologyEx(green_threshold_frame, green_threshold_frame, cv::MORPH_CLOSE, kernel(3,3));
	

	//checking for a color
	std::vector<std::vector<cv::Point> > red_contours, green_contours;
	findContours(red_threshold_frame, red_contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
	findContours(green_threshold_frame, green_contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	bool green_light = hasLight(green_contours);
	bool red_light = hasLight(red_contours);

	if(red_light) {
		race_started = false;
	} else if(!red_light && !green_light) {
		if(time_red_off == 0) {
			time_red_off = ros::Time::now().toSec();
		}
	} else if(green_light && ros::Time::now().toSec() - time_red_off <= 1) {
		race_started = true;
	} else if(time_red_off != 0 && ros::Time::now().toSec() - time_red_off > 1) {
		race_started = false;
		time_red_off = 0;
	}

	std_msgs::Bool start_msg;
	start_msg.data = race_started;
	raceStart_pub.publish(start_msg);


	sensor_msgs::Image out;
    cv_ptr->image = frame;
    cv_ptr->encoding = "bgr8";
    cv_ptr->toImageMsg(out);
    debugImage_pub.publish(out);
}



int main(int argc, char** argv){
	ros::init(argc, argv, "start_light");
	ros::NodeHandle p_nh = ros::NodeHandle("~");

	ros::Subscriber subscriber = p_nh.subscribe("/camera/image", 1, &img_callback);
	raceStart_pub = p_nh.advertise<std_msgs::Bool>("/event/race_started", 1);
	debugImage_pub = p_nh.advertise<sensor_msgs::Image>("debug_image", 1);


	ros::Rate rate(10);
	while(ros::ok()) {
		ros::spinOnce();
		rate.sleep();
	}
}