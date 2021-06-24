//
// Created by Hyungtae Lim on 6/23/21.
//

#include <iostream>
// For disable PCL complile lib, to use PointXYZIR
#define PCL_NO_PRECOMPILE
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <patchwork/node.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/common/centroid.h>
#include "patchwork/patchwork.hpp"
#include <visualization_msgs/Marker.h>
#include "tools/kitti_loader.hpp"

using namespace std;

ros::Publisher CloudPublisher;
ros::Publisher TPPublisher;
ros::Publisher FPPublisher;
ros::Publisher FNPublisher;
ros::Publisher PrecisionPublisher;
ros::Publisher RecallPublisher;

boost::shared_ptr<PatchWork> PatchworkGroundSeg;

std::string output_filename;
std::string acc_filename, pcd_savepath;
string      algorithm;
string      mode;
string      seq;
bool        save_flag;

template<typename T>
pcl::PointCloud<T> cloudmsg2cloud(sensor_msgs::PointCloud2 cloudmsg)
{
    pcl::PointCloud<T> cloudresult;
    pcl::fromROSMsg(cloudmsg,cloudresult);
    return cloudresult;
}

template<typename T>
sensor_msgs::PointCloud2 cloud2msg(pcl::PointCloud<T> cloud, std::string frame_id = "map")
{
    sensor_msgs::PointCloud2 cloud_ROS;
    pcl::toROSMsg(cloud, cloud_ROS);
    cloud_ROS.header.frame_id = frame_id;
    return cloud_ROS;
}



int main(int argc, char **argv) {
    ros::init(argc, argv, "Benchmark");
    ros::NodeHandle nh;
    nh.param<string>("/algorithm", algorithm, "patchwork");
    nh.param<string>("/seq", seq, "00");

    PatchworkGroundSeg.reset(new PatchWork(&nh));

    CloudPublisher  = nh.advertise<sensor_msgs::PointCloud2>("/benchmark/cloud", 100);
    TPPublisher     = nh.advertise<sensor_msgs::PointCloud2>("/benchmark/TP", 100);
    FPPublisher     = nh.advertise<sensor_msgs::PointCloud2>("/benchmark/FP", 100);
    FNPublisher     = nh.advertise<sensor_msgs::PointCloud2>("/benchmark/FN", 100);

    PrecisionPublisher = nh.advertise<visualization_msgs::Marker>("/precision", 1);
    RecallPublisher    = nh.advertise<visualization_msgs::Marker>("/recall", 1);

    KittiLoader loader("/home/shapelim/dataset/dataset/sequences/00");

    int N = loader.size();
    for (int n = 0; n < N; ++n){
        cout << n << "th node come" << endl;
        pcl::PointCloud<PointType> pc_curr;
        loader.get_cloud(n, pc_curr);
        pcl::PointCloud<PointType> pc_ground;
        pcl::PointCloud<PointType> pc_non_ground;

        static double time_taken;
        cout << "Operating patchwork..." << endl;
        PatchworkGroundSeg->estimate_ground(pc_curr, pc_ground, pc_non_ground, time_taken);

        // Estimation
        double precision, recall, precision_naive, recall_naive;
        calculate_precision_recall(pc_curr, pc_ground, precision, recall);
        calculate_precision_recall(pc_curr, pc_ground, precision_naive, recall_naive, false);

        cout << "\033[1;32m" << n << "th, " << " takes : " << time_taken << " | " << pc_curr.size() << " -> " << pc_ground.size()
             << "\033[0m" << endl;

        cout << "\033[1;32m P: " << precision << " | R: " << recall << "\033[0m" << endl;

        ofstream sc_output(output_filename, ios::app);
        sc_output << n << "," << time_taken << "," << precision << "," << recall << "," << precision_naive << "," << recall_naive;

        sc_output << std::endl;
        sc_output.close();

        // Publish msg
        pcl::PointCloud<PointType> TP;
        pcl::PointCloud<PointType> FP;
        pcl::PointCloud<PointType> FN;
        pcl::PointCloud<PointType> TN;
        discern_ground(pc_ground, TP, FP);
        discern_ground(pc_non_ground, FN, TN);

        if (save_flag) {
            std::map<int, int> pc_curr_gt_counts, g_est_gt_counts;
            double             accuracy;
            save_all_accuracy(pc_curr, pc_ground, acc_filename, accuracy, pc_curr_gt_counts, g_est_gt_counts);

            std::string count_str        = std::to_string(n);
            std::string count_str_padded = std::string(NUM_ZEROS - count_str.length(), '0') + count_str;
            std::string pcd_filename     = pcd_savepath + "/" + count_str_padded + ".pcd";

        //    pc2pcdfile(TP, FP, FN, TN, pcd_filename);
        }
        // Write data
        CloudPublisher.publish(cloud2msg(pc_curr));
        TPPublisher.publish(cloud2msg(TP));
        FPPublisher.publish(cloud2msg(FP));
        FNPublisher.publish(cloud2msg(FN));
        pub_score("p", precision);
        pub_score("r", recall);

    }

    ros::spin();

    return 0;
}
