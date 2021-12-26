#ifndef SLAM_LIBRARIES_H
#define SLAM_LIBRARIES_H
// SLAM libraries
#include "use-ikfom.hpp"
#include "ikd_Tree.h"
#endif

class Mapper {
    public:
        double last_map_time;

    private:
        KD_TREE<PointType>::Ptr map;

    public:
        Mapper();
        bool exists();
        int size();

        void add(PointCloud&, bool downsample=false);        
        void add(const State&, PointCloud&, bool downsample=false);        
        Planes match(const State&, const PointCloud&);
        bool hasToMap(double t);

    private:
        void init_tree();
        void build_tree(PointCloud&);
        bool exists_tree();

        void add_points(PointCloud&, bool downsample=false);
        Plane match_plane(const State&, const PointType&);
};