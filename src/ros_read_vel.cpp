#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include <inttypes.h>
#include "ros/ros.h"
#include <iostream>
#include <string>
#include <sstream>
#include "std_msgs/Float64.h"

using namespace std;

// Global variables
std_msgs::Float64 position_msg;

int main(int argc, char **argv)
{
  float rpos = 0.0;
  string inputS;

  // Initialize ROS node
  ros::init(argc, argv, "ros_read_vel");
  ros::NodeHandle n;
  // Publish for desired position message
  ros::Publisher read_position_pub = n.advertise<std_msgs::Float64>("setpoint", 1000);
  ros::Rate loop_rate(1);
  position_msg.data = 0.0;
  ROS_INFO("Reading Desired Velocity.");
  while (ros::ok())
  {
	// Read a line from standard input and parse the desired position
	getline (cin,inputS);
	if (inputS == "q")
	{
		rpos = 0.0;
    	position_msg.data = rpos;
		read_position_pub.publish(position_msg);		
        break;
    }
    else if (inputS == "kill")
    {
		position_msg.data = -1;
		read_position_pub.publish(position_msg);
    }
	else if (inputS == "enable")
	{
		position_msg.data = -2;
		read_position_pub.publish(position_msg);
	}
    else if (inputS == "ccw")
	{
        position_msg.data = -3;
		read_position_pub.publish(position_msg);
	}
	else if (inputS == "cw")
	{
		position_msg.data = -4;
		read_position_pub.publish(position_msg);
	}
	else
	{	
		stringstream ss;
		ss<<inputS;
		ss>>rpos; //convert string into int and store it in "asInt"
		ss.str(""); //clear the stringstream
		ss.clear(); 
		
		if ((rpos != 0.0) || (rpos == 0.0 && inputS == "0"))
		{
			//cout << "Read : " << rpos << endl;
			// Publish the received desired position
			position_msg.data = rpos;
			read_position_pub.publish(position_msg);
		}
	}
	ros::spinOnce();
    loop_rate.sleep();
  }
  return 0;
}