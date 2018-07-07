#include "ros/ros.h"
#include "ros/console.h"
#include "std_msgs/String.h"
#include "sensor_msgs/Image.h"
#include "sensor_msgs/CameraInfo.h"

#include "opencv/cv.h"
#include "opencv2/calib3d.hpp"
#include <cv_bridge/cv_bridge.h>
#include <vector>


ros::Publisher pubImg;
bool isCamInfoAvailable = false;
sensor_msgs::CameraInfo camInfo;
cv::Mat camMtx(3,3,CV_32F);
cv::Mat camDst(1,5,CV_32F);
cv::Size patternSize = {7,5};
std::vector<cv::Point3f> objPnts;

void camInfoCallback(const sensor_msgs::CameraInfo::ConstPtr& info)
{
  if(isCamInfoAvailable) return;
  isCamInfoAvailable = true;
  float tempK[9];
  for (int i = 0; i < 9; i++) {
     tempK[i] = info->K[i];
  }
  std::memcpy(camMtx.data, info->K.data(),3*3*sizeof(float));
  std::memcpy(camDst.data, info->D.data(),1*5*sizeof(float));
  //ROS_INFO_STREAM( camDst.at<float>(0) << " " << camMtx.at<float>(0) );
}

void imgCallback(const sensor_msgs::Image::ConstPtr& img)
{
  if(!isCamInfoAvailable) return;
  // From ROS image to OpenCV Image
  cv_bridge::CvImagePtr cv_ptr;
  cv_ptr = cv_bridge::toCvCopy(img, sensor_msgs::image_encodings::BGR8);

  // find checkerboard corners
  cv_bridge::CvImagePtr img_gry_ptr(new cv_bridge::CvImage);
  cv::cvtColor(cv_ptr->image,img_gry_ptr->image,cv::COLOR_BGR2GRAY);
  std::vector<cv::Point2f> corners;
  bool found = cv::findChessboardCorners(img_gry_ptr->image,
                                       patternSize,
                                       corners);
  if (!found) return;

  cv::drawChessboardCorners(cv_ptr->image,patternSize,corners,found);

  // find the rotation of the camera relative to the checkerboard
  cv::Mat rot(1,3,CV_32F);
  cv::Mat tra(1,3,CV_32F);

  try
  {
    if(corners.size() == 7*5)
    {
      cv::solvePnP(objPnts,corners,camMtx,camDst,rot,tra);
      ROS_INFO_STREAM( rot );
    }
  }
  catch (cv::Exception e)
  {
    ROS_WARN_STREAM(e.msg);//objPnts << std::endl<<
  }


  //ROS_DEBUG_STREAM("hei: " << rot << tra);

  // add axis to the checkerboard

  // Publish the image
  pubImg.publish(cv_ptr->toImageMsg());
}

int main(int argc, char **argv)
{
  for(float height = 0; height<patternSize.height; height++)
  {
    for (float width = 0; width <patternSize.width; width++ )
    {
      cv::Point3f pnt(height,width,0);
      objPnts.push_back(pnt);
    }

  }


  ros::init(argc, argv, "camPoseEstimator");
  ros::NodeHandle nh;

  ros::Subscriber subImg = nh.subscribe("/sensors/camera/image_color", 1, imgCallback);
  ros::Subscriber subCamInfo = nh.subscribe("/sensors/camera/camera_info", 1, camInfoCallback);
  pubImg = nh.advertise<sensor_msgs::Image>("imageWithAxis", 100);

  ros::spin();

  return 0;
}
