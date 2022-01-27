class Publishers {
    public:
        ros::Publisher state_pub;
        ros::Publisher states_pub;
        ros::Publisher vel_pub;
        ros::Publisher yaw_pub;
        ros::Publisher pcl_pub;
        ros::Publisher full_pcl_pub;
        ros::Publisher planes_pub;
        ros::Publisher gt_pub;

        double last_transform_time = -1;

        Publishers() {
            this->only_couts = true;
        }

        Publishers(ros::NodeHandle& nh) {
            this->state_pub = nh.advertise<nav_msgs::Odometry>("/limovelo/state", 1000); 
            this->states_pub = nh.advertise<geometry_msgs::PoseArray>("/limovelo/states", 1000); 

            this->vel_pub = nh.advertise<nav_msgs::Odometry>("/limovelo/vel", 1000); 
            this->yaw_pub = nh.advertise<std_msgs::Float32>("/limovelo/yaw", 1000); 
            
            this->pcl_pub = nh.advertise<sensor_msgs::PointCloud2>("/limovelo/pcl", 1000); 
            this->full_pcl_pub = nh.advertise<sensor_msgs::PointCloud2>("/limovelo/full_pcl", 1000); 

            this->gt_pub = nh.advertise<nav_msgs::Odometry>("/limovelo/gt", 1000);

            this->planes_pub = nh.advertise<geometry_msgs::PoseArray>("/limovelo/planes", 1000);
            this->only_couts = false;
        }

        void state(const State& state, bool couts) {
            if (not only_couts) publish_pos(state);
            if (not only_couts) publish_vels(state);
            if (couts) cout_state(state);
        }

        void states(const States& states) {
            publish_states(states);
        }

        void planes(const State& X, const Planes& planes) {
            publish_planes(X, planes);
        }

        void pointcloud(PointCloud& pcl) {
            pcl.header.frame_id = "map";
            publish_pcl(pcl);
        }

        void pointcloud(Points& points) {
            if (points.empty()) return;
            
            PointCloud pcl;
            pcl.header.frame_id = "map";
            pcl.header.stamp = Conversions::sec2Microsec(points.back().time);
            for (Point p : points) pcl.push_back(p.toPCL());

            publish_pcl(pcl);
        }

        void full_pointcloud(PointCloud& pcl) {
            pcl.header.frame_id = "map";
            publish_full_pcl(pcl);
        }

        void full_pointcloud(Points& points) {
            if (points.empty()) return;
            
            PointCloud pcl;
            pcl.header.frame_id = "map";
            pcl.header.stamp = Conversions::sec2Microsec(points.back().time);
            for (Point p : points) pcl.push_back(p.toPCL());

            publish_full_pcl(pcl);
        }

        void rottransl(const RotTransl& RT) {
            cout_rottransl(RT);
        }

        void t1_t2(const Points& points, const IMUs& imus, const States& states, double t1, double t2) {
            cout_t1_t2(points, imus, states, t1, t2);
        }

        void receive_tf(const tf2_msgs::TFMessage::ConstPtr& msg) {
            if (this->is_first_tf) this->set_first_tf(msg, "base_link");
            this->publish_tf(msg, "base_link");   
        }

        void tf(const State& state) {
            if (state.time <= last_transform_time) return;
            this->send_transform(state);
            last_transform_time = state.time;
        }

        void extrinsics(const State& state) {
            this->cout_extrinsics(state);
        }

    private:
        bool only_couts;
        bool is_first_tf = true;
        Eigen::Vector3d tfirst;
        Eigen::Quaterniond qfirst;

        void set_first_tf(const tf2_msgs::TFMessage::ConstPtr& msg, const std::string& body_frame) {
            for (auto tf : msg->transforms)
                if (tf.child_frame_id == body_frame) {
                    this->tfirst = Eigen::Vector3d (tf.transform.translation.x, tf.transform.translation.y, tf.transform.translation.z);
                    this->qfirst = Eigen::Quaterniond (tf.transform.rotation.w, tf.transform.rotation.x, tf.transform.rotation.y, tf.transform.rotation.z);
                }

            this->is_first_tf = false;
        }

        void publish_tf(const tf2_msgs::TFMessage::ConstPtr& msg, const std::string& body_frame) {
            for (auto tf : msg->transforms)
                if (tf.child_frame_id == body_frame)
                    this->publish_tf(
                        tf.transform, ros::Time::now(),
                        "map", "body"
                    );
        }

        void publish_tf(const geometry_msgs::Transform& transform, const ros::Time& stamp, std::string world, std::string body) {
            nav_msgs::Odometry msg;
            msg.header.stamp = stamp;
            msg.header.frame_id = world;

            auto t = transform.translation;
            Eigen::Vector3d tnow(t.x, t.y, t.z);
            msg.pose.pose.position.x = (qfirst.conjugate() * (tnow - tfirst)).x();
            msg.pose.pose.position.y = (qfirst.conjugate() * (tnow - tfirst)).y();
            msg.pose.pose.position.z = (qfirst.conjugate() * (tnow - tfirst)).z();

            auto q = transform.rotation;
            Eigen::Quaterniond qnow(q.w, q.x, q.y, q.z);
            msg.pose.pose.orientation.x = (qnow * qfirst.conjugate()).x();
            msg.pose.pose.orientation.y = (qnow * qfirst.conjugate()).y();
            msg.pose.pose.orientation.z = (qnow * qfirst.conjugate()).z();
            msg.pose.pose.orientation.w = (qnow * qfirst.conjugate()).w();
            this->gt_pub.publish(msg);
        }

        void cout_extrinsics(const State& state) {
            std::cout << "t:" << std::endl;
            std::cout << state.I_Rt_L().t.transpose() << std::endl;
            std::cout << "R:" << std::endl;
            std::cout << state.I_Rt_L().R << std::endl;
            std::cout << "-----------" << std::endl;
        }

        void publish_planes(const State& X, const Planes& planes) {
            geometry_msgs::PoseArray normalPoseArray;
            normalPoseArray.header.frame_id = "map";
            normalPoseArray.header.stamp = ros::Time().fromSec(X.time);
                
            for (Plane plane : planes)
            {
                Point point_world = plane.centroid;
                Normal n = plane.n;

                geometry_msgs::Pose normalPose;
                normalPose.position.x = point_world.x;
                normalPose.position.y = point_world.y;
                normalPose.position.z = point_world.z;

                double NORM = std::sqrt(n.A*n.A + n.B*n.B + n.C*n.C);
                
                normalPose.orientation.x = 0;
                normalPose.orientation.y = -n.C;
                normalPose.orientation.z = n.B;
                normalPose.orientation.w = NORM + n.A;

                normalPoseArray.poses.push_back(normalPose);
            }

            this->planes_pub.publish(normalPoseArray);
        }

        void send_transform(const State& X) {
            static tf::TransformBroadcaster br;
            tf::Transform transform;
            tf::Quaternion q;

            transform.setOrigin(tf::Vector3(X.pos(0), \
                                            X.pos(1), \
                                            X.pos(2)));
            
            Eigen::Quaternionf q_from_R(X.R);
            q.setW(q_from_R.w());
            q.setX(q_from_R.x());
            q.setY(q_from_R.y());
            q.setZ(q_from_R.z());
            transform.setRotation(q);
            
            br.sendTransform(tf::StampedTransform(transform, ros::Time(X.time), "map", "body"));
        }

        void cout_rottransl(const RotTransl& RT) {
            std::cout << RT.R << std::endl;
            std::cout << RT.t.transpose() << std::endl;
        }

        void publish_pcl(const PointCloud& pcl) {
            sensor_msgs::PointCloud2 msg;
            msg.header.stamp = ros::Time(Conversions::microsec2Sec(pcl.header.stamp));
            msg.header.frame_id = "map";
            pcl::toROSMsg(pcl, msg);
            this->pcl_pub.publish(msg);
        }

        void publish_full_pcl(const PointCloud& pcl) {
            sensor_msgs::PointCloud2 msg;
            msg.header.stamp = ros::Time(Conversions::microsec2Sec(pcl.header.stamp));
            msg.header.frame_id = "map";
            pcl::toROSMsg(pcl, msg);
            this->full_pcl_pub.publish(msg);
        }

        void publish_yaw(const State& state) {
            std_msgs::Float32 yaw;

            Eigen::Quaternionf q(state.R);
            double siny_cosp = 2 * (q.w() * q.z() + q.x() * q.y());
            double cosy_cosp = 1 - 2 * (q.y() * q.y() + q.z() * q.z());
            yaw.data = std::atan2(siny_cosp, cosy_cosp);

            this->yaw_pub.publish(yaw);
        }

        void publish_states(const States& states) {
            geometry_msgs::PoseArray msg;
            msg.header.frame_id = "map";
            msg.header.stamp = ros::Time(states.back().time);

            for (const State& state : states) {
                geometry_msgs::Pose pose;

                pose.position.x = state.pos(0);
                pose.position.y = state.pos(1);
                pose.position.z = state.pos(2);

                Eigen::Quaternionf q(state.R * state.I_Rt_L().R);
                pose.orientation.x = q.x();
                pose.orientation.y = q.y();
                pose.orientation.z = q.z();
                pose.orientation.w = q.w();

                msg.poses.push_back(pose);
            }

            this->states_pub.publish(msg);
        }

        void publish_pos(const State& state) {
            nav_msgs::Odometry msg;
            msg.header.stamp = ros::Time(state.time);
            msg.header.frame_id = "map";

            msg.pose.pose.position.x = state.pos(0);
            msg.pose.pose.position.y = state.pos(1);
            msg.pose.pose.position.z = state.pos(2);

            Eigen::Quaternionf q(state.R * state.I_Rt_L().R);
            msg.pose.pose.orientation.x = q.x();
            msg.pose.pose.orientation.y = q.y();
            msg.pose.pose.orientation.z = q.z();
            msg.pose.pose.orientation.w = q.w();

            this->state_pub.publish(msg);
        }

        void publish_vels(const State& state) {
            nav_msgs::Odometry msg;
            msg.header.stamp = ros::Time(state.time);
            msg.header.frame_id = "map";

            msg.pose.pose.orientation.x = state.vel(0);
            msg.pose.pose.orientation.y = state.vel(1);
            msg.pose.pose.orientation.z = state.vel(2);
            msg.pose.pose.orientation.w = 0.;

            this->vel_pub.publish(msg);
        }

        void cout_state(const State& state) {
            std::cout << "---------" << std::endl;
            std::cout << "New pos: " << state.pos.transpose() << std::endl;
            std::cout << "New vel: " << state.vel.transpose() << std::endl;
            std::cout << "New R: " << std::endl << state.R << std::endl;
            std::cout << "---------" << std::endl;
        }

        void cout_t1_t2(const Points& points, const IMUs& imus, const States& states, double t1, double t2) {
            if (points.size() > 0 and imus.size() > 0 and states.size() > 0) {
                std::cout << "-----------" << std::endl;
                std::cout << std::setprecision(16) << "   " << t1 << " to " << t2 << std::endl;
                std::cout << std::setprecision(16) << "L: " << points.front().time << " -- " << points.back().time << " = " << points.size() << std::endl;
                std::cout << std::setprecision(16) << "I: " << imus.front().time << " -- " << imus.back().time << " = " << imus.size() << std::endl;
                std::cout << std::setprecision(16) << "X: " << states.front().time << " -- " << states.back().time << " = " << states.size() << std::endl;
            }
        }

};