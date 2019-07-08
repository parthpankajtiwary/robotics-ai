#include <pointcloudhandler.h>
#include <fstream>
PointCloudHandler::PointCloudHandler(ros::NodeHandle &n) :
	as(n, "pointcloudfunction", boost::bind(&PointCloudHandler::execute, this, _1), false)
{
	d_nh = n;
	as.start(); // start the action server

	reset();

	plane_pub = d_nh.advertise<PCLPointCloud>("alice_pointcloud/planes", 1);
	v_plane_pub = d_nh.advertise<alice_msgs::PcPlane>("alice_pointcloud/vPlanes", 1);
	object_pub = d_nh.advertise<PCLPointCloud>("alice_pointcloud/objectsCloud", 1);
}

void PointCloudHandler::reset()
{
	d_detectMultiPlane = false;
	d_detectPlane = false;
	d_detectEmptyShelf = false;
	d_detectObjects = false;
	failCounter = 0;
	preCount = 0;
}


void PointCloudHandler::execute(const alice_msgs::pointcloudfunctionGoalConstPtr &goal)
{
	string function = goal->function;

	reset();

	if (function == "emptyShelf")
		d_detectEmptyShelf = true;

	if (function == "detectPlane")
		d_detectPlane = true;

	if (function == "detectMultiPlane")
		d_detectMultiPlane = true;


	if (function == "findEmptySpace")
		{

		}

	if (function == "getClusters")
		d_detectObjects = true;

	d_pc_sub = d_nh.subscribe<sensor_msgs::PointCloud2>("front_xtion/depth_registered/points", 1, &PointCloudHandler::pcCallback, this);
//	d_pc_sub = d_nh.subscribe<const sensor_msgs::PointCloud>("camera/depth_registered/points", 1, &PointCloudHandler::pcCallback, this);

}

void PointCloudHandler::shutdownSubcriber()
{
	d_pc_sub.shutdown(); // unsubscribe
}

bool PointCloudHandler::getPlane(PCLPointCloudPtr &pointcloud, PCLPointCloudPtr &planeCloud)
{
	ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
	PointIndices::Ptr inliers (new pcl::PointIndices);

	// Optional
	seg.setOptimizeCoefficients (true);
	// Mandatory
	seg.setModelType(pcl::SACMODEL_PLANE);
	seg.setMethodType(pcl::SAC_RANSAC);
	seg.setDistanceThreshold (0.015);  // 0.015
	seg.setMaxIterations(500);
    //ROS_INFO_STREAM(pointcloud->points.size());
	seg.setInputCloud(pointcloud);
	seg.segment(*inliers, *coefficients);

	size_t minPointsInPlane = 5000;

	if (inliers->indices.size() < minPointsInPlane)
		return false;

	extract.setInputCloud(pointcloud);
	extract.setIndices(inliers);
	extract.setNegative(false); // false means keep planes
	extract.filter(*planeCloud);

	extract.setNegative(true); // true means remove planes
	extract.filter(*pointcloud);
	return true;
}

void PointCloudHandler::transformPC(PCLPointCloudPtr &cloud, string transformTo)
{
	sensor_msgs::PointCloud2 cloudMsg;
	toROSMsg(*cloud, cloudMsg);
	//toROSMsg

	PCLPointCloudPtr pointcloud(new PCLPointCloud);
	sensor_msgs::PointCloud2 cloudMsg_transformed;
	pcl_ros::transformPointCloud(transformTo, cloudMsg, cloudMsg_transformed, tfTransformer);

	fromROSMsg(cloudMsg_transformed, *cloud);
}

void PointCloudHandler::pcCallback(const sensor_msgs::PointCloud2ConstPtr &cloudMsg)
{
	if (preCount < 60)
	{
		++preCount;
		return;
	}
	// transform the point cloud to mico base link...

	string transformTo = "/mico_base_link";

	if (!d_detectObjects)
		transformTo = "/base_link";

	ros::Time now = ros::Time::now();
	tfTransformer.waitForTransform(transformTo, "/front_xtion_link", now, ros::Duration(0.1));

	if (!tfTransformer.canTransform(transformTo, cloudMsg->header.frame_id, cloudMsg->header.stamp))
	{
		ROS_INFO_STREAM("No transform found");
		return;
	}
	/*
	tf::StampedTransform xtion_tf;
	tfTransformer.lookupTransform(transformTo, "/front_xtion_link", now, xtion_tf);
	std::fstream logger;
	logger.open("/home/borg/log.txt", std::fstream::in | std::fstream::out | std::fstream::app);

	tf::Matrix3x3 euler(xtion_tf.getRotation());
	double roll, pitch, yaw;
	euler.getRPY(roll, pitch, yaw);
	logger << "Roll: "<< roll << " Pitch: " << pitch   << " Yaw: " << yaw << '\n';
	logger.close();
	*/

	PCLPointCloudPtr pointcloud(new PCLPointCloud);
	sensor_msgs::PointCloud2 cloudMsg_transformed;
	pcl_ros::transformPointCloud(transformTo, *cloudMsg, cloudMsg_transformed, tfTransformer);

	fromROSMsg(cloudMsg_transformed, *pointcloud);

	PassThrough<Point> passthrough_filter;
	passthrough_filter.setInputCloud(pointcloud);
	passthrough_filter.setFilterFieldName("z");

	if (d_detectObjects)
		passthrough_filter.setFilterLimits(-0.6, 2.0); // Might need to change this!
	else
		passthrough_filter.setFilterLimits(0.0, 4.0); // Might need to change this!

	passthrough_filter.filter(*pointcloud);


	PCLPointCloudPtr voxelCloud(new PCLPointCloud);
	VoxelGrid<Point> voxel;
	voxel.setInputCloud(pointcloud);
	voxel.setLeafSize (0.01f, 0.01f, 0.01f);
	voxel.filter (*voxelCloud);

	vector<alice_msgs::Location> vLocations;

	if (d_detectMultiPlane || d_detectObjects)
	{
		vector<sensor_msgs::PointCloud2> vPlanes;

		PCLPointCloudPtr objectCloud(new PCLPointCloud);
		PCLPointCloudPtr planeCloud(new PCLPointCloud);

		*objectCloud = *pointcloud;

		bool done = false;

		while (!done)
		{
			if (!getPlane(objectCloud, planeCloud))
			{
				done = true;
				break;
			}

		//	transformPC(planeCloud, transformTo);

			Point minPt, maxPt;
			getMinMax3D(*planeCloud, minPt, maxPt);

			if (fabs(maxPt.z - minPt.z) < 0.15f) // planes can be vertical
			{
				PCLPointCloudPtr tempCloud(new PCLPointCloud);
				for (size_t point = 0; point < planeCloud->points.size(); ++point)
					tempCloud->points.push_back(planeCloud->points.at(point));

				sensor_msgs::PointCloud2 sensorCloud;
				toROSMsg(*tempCloud, sensorCloud);
				vPlanes.push_back(sensorCloud);
			}
		}

		PCLPointCloudPtr planesCloud(new PCLPointCloud);
		planesCloud->header.frame_id = "base_link";

		ROS_INFO_STREAM("Nr of planes: " << vPlanes.size());

		for (size_t idx = 0; idx < vPlanes.size(); ++idx)
		{
			PCLPointCloudPtr tCloud(new PCLPointCloud);
			fromROSMsg(vPlanes.at(idx), *tCloud);

			for (size_t idp = 0; idp < tCloud->points.size(); ++idp)
				planesCloud->points.push_back(tCloud->points.at(idp));

		}


		if (failCounter < 10)
			if (vPlanes.size() == 0)
			{
				++failCounter;
				return;
			}

		plane_pub.publish(planesCloud);

		alice_msgs::PcPlane pcPlane;
		pcPlane.vector = vPlanes;
		v_plane_pub.publish(pcPlane);

		if (d_detectObjects)
			object_pub.publish(objectCloud);

		shutdownSubcriber();
		return;
	}

	if (d_detectEmptyShelf)
	{
		PCLPointCloudPtr objectCloud(new PCLPointCloud);
		PCLPointCloudPtr planeCloud(new PCLPointCloud);

		*objectCloud = *voxelCloud;
		bool correctPlane = false;

		while (!correctPlane)
		{
			getPlane(objectCloud, planeCloud);
			Point minPt, maxPt;
			getMinMax3D(*planeCloud, minPt, maxPt);

			bool empty = true;

			if (fabs(maxPt.z - minPt.z) < 0.08f) // planes can be vertical
			{
				float line = maxPt.z + 0.04f; // a virtual plane 4cm above the found plane

				for (size_t idx = 0; idx < objectCloud->points.size(); ++idx) // check if a point lies on this line
				{
					Point point = objectCloud->points.at(idx);

					if (point.x > minPt.x && point.x < maxPt.x &&
						point.y > minPt.y && point.y < maxPt.y) // the object is in the plane dimensions
					{
						if (fabs(point.z - line) <= 0.1) // point is most likely on the plane as well
						{
							empty = false;  			// therefore the plane is not empty
							break;
						}
					}
				}
			}
			else
				empty = false;

			if (empty)
			{
				correctPlane = true;
				alice_msgs::Location location;
				location.minX = minPt.x;
				location.minY = minPt.y;
				location.minZ = minPt.z;
				location.maxX = maxPt.x;
				location.maxY = maxPt.y;
				location.maxZ = maxPt.z;

				vLocations.push_back(location);
			}
		}

		d_result.object = vLocations;
		as.setSucceeded(d_result);
		shutdownSubcriber();
		return;
	}

}

