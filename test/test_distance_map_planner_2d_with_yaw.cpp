#include "read_map.hpp"
#include "timer.hpp"
#include <mpl_planner/planner/map_planner.h>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

int main(int argc, char **argv) {
  if (argc != 2) {
    printf(ANSI_COLOR_RED "Input yaml required!\n" ANSI_COLOR_RESET);
    return -1;
  }

  // Load the map
  MapReader<Vec2i, Vec2f> reader(argv[1]);
  if (!reader.exist()) {
    printf(ANSI_COLOR_RED "Cannot find input file [%s]!\n" ANSI_COLOR_RESET,
           argv[1]);
    return -1;
  }

  // Pass the data into a VoxelMapUtil class for collision checking
  std::shared_ptr<MPL::OccMapUtil> map_util = std::make_shared<MPL::OccMapUtil>();
  map_util->setMap(reader.origin(), reader.dim(), reader.data(),
                   reader.resolution());
  map_util->freeUnknown();

  // Initialize start and goal, using acc control
  Waypoint2D start, goal;
  start.pos = Vec2f(reader.start(0), reader.start(1));
  start.vel = Vec2f::Zero();
  start.acc = Vec2f::Zero();
  start.jrk = Vec2f::Zero();
  start.yaw = 0;
  start.use_pos = true;
  start.use_vel = true;
  start.use_acc = false;
  start.use_jrk = false;
  start.use_yaw = false;

  goal.pos = Vec2f(reader.goal(0), reader.goal(1));
  goal.vel = Vec2f::Zero();
  goal.acc = Vec2f::Zero();
  goal.jrk = Vec2f::Zero();
  goal.yaw = 0;
  goal.control = start.control;

  // Initialize control input
  decimal_t u = 0.5;
  decimal_t du = u;
  vec_E<VecDf> U;
  for (decimal_t dx = -u; dx <= u; dx += du)
    for (decimal_t dy = -u; dy <= u; dy += du)
      U.push_back(Vec2f(dx, dy));

  // Initialize planner
  std::unique_ptr<MPL::OccMapPlanner> planner(
      new MPL::OccMapPlanner(true));    // Declare a mp planner using voxel map
  planner->setMapUtil(map_util); // Set collision checking function
  planner->setVmax(1.0);         // Set max velocity
  planner->setAmax(1.0);         // Set max acceleration
  planner->setDt(1.0);           // Set dt for each primitive
  planner->setU(U);              // Set control input

  // Planning
  Timer time(true);
  bool valid = planner->plan(start, goal); // Plan from start to goal
  double dt = time.Elapsed().count();
  printf("MPL Planner takes: %f ms\n", dt);
	printf("MPL Planner expanded states: %zu\n", planner->getCloseSet().size());
	const Trajectory2D traj = planner->getTraj();


  // Create a path from planned traj
	const auto ws = traj.getWaypoints();
	vec_Vec2f path;
	for(const auto& w: ws)
		path.push_back(w.pos);

  // Add yaw control input
  decimal_t u_yaw = 0.5;
  vec_E<VecDf> U_yaw;
  for (decimal_t dx = -u; dx <= u; dx += du)
    for (decimal_t dy = -u; dy <= u; dy += du)
      for (decimal_t dyaw = -u_yaw; dyaw <= u_yaw; dyaw += u_yaw)
        U_yaw.push_back(Vec3f(dx, dy, dyaw));


  // Initiaize planner as a distance map planner
	planner.reset(new MPL::OccMapPlanner(true));
  planner->setMapUtil(map_util); // Set collision checking function
  planner->setVmax(1.0);         // Set max velocity
  planner->setAmax(1.0);         // Set max acceleration
  planner->setDt(1.0);           // Set dt for each primitive
  planner->setU(U_yaw);              // Set control input
  planner->setEpsilon(1.0);           // Set heursitic to zero

	planner->setValidRegion(path, Vec2f(0.5, 0.5)); // Set search region around path
	planner->setPotentialRadius(Vec2f(1.0, 1.0)); // Set potential distance
  planner->setPotentialWeight(1); // Set potential weight
  planner->setGradientWeight(0); // Set gradient weight
  planner->updatePotentialMap(start.pos); // Update potential map

  decimal_t yaw_max = 0.5; // Set yaw max
  start.use_yaw = true; // enable yaw
  planner->setYawmax(yaw_max);       // Set yaw threshold

  // Planning
  Timer time_dist(true);
  bool valid_dist = planner->plan(start, goal); // Plan from start to goal
  double dt_dist = time_dist.Elapsed().count();
  printf("MPL Distance Planner takes: %f ms\n", dt_dist);
	printf("MPL Distance Planner expanded states: %zu\n", planner->getCloseSet().size());
	const Trajectory2D traj_dist = planner->getTraj();

  // Plot the result in svg image
  typedef boost::geometry::model::d2::point_xy<double> point_2d;
  // Declare a stream and an SVG mapper
  std::ofstream svg("output.svg");
  boost::geometry::svg_mapper<point_2d> mapper(svg, 1000, 1000);

  // Draw the canvas
  const Vec2i dim = reader.dim();
  boost::geometry::model::polygon<point_2d> bound;
  const double origin_x = reader.origin()(0);
  const double origin_y = reader.origin()(1);
  const double range_x = reader.dim()(0) * reader.resolution();
  const double range_y = reader.dim()(1) * reader.resolution();
  std::vector<point_2d> points;
  points.push_back(point_2d(origin_x, origin_y));
  points.push_back(point_2d(origin_x, origin_y + range_y));
  points.push_back(point_2d(origin_x + range_x, origin_y + range_y));
  points.push_back(point_2d(origin_x + range_x, origin_y));
  points.push_back(point_2d(origin_x, origin_y));
  boost::geometry::assign_points(bound, points);
  boost::geometry::correct(bound);

  mapper.add(bound);
  mapper.map(bound, "fill-opacity:1.0;fill:rgb(255,255,255);stroke:rgb(0,0,0);"
                    "stroke-width:2"); // White

  // Draw start and goal
  point_2d start_pt, goal_pt;
  boost::geometry::assign_values(start_pt, start.pos(0), start.pos(1));
  mapper.add(start_pt);
  mapper.map(start_pt, "fill-opacity:1.0;fill:rgb(255,0,0);", 10); // Red
  boost::geometry::assign_values(goal_pt, goal.pos(0), goal.pos(1));
  mapper.add(goal_pt);
  mapper.map(goal_pt, "fill-opacity:1.0;fill:rgb(255,0,0);", 10); // Red

  // Draw the obstacles
  const auto data = map_util->getMap();
  for(int x = 0; x < dim(0); x ++) {
    for(int y = 0; y < dim(1); y ++) {
        Vec2f pt = map_util->intToFloat(Vec2i(x, y));
        decimal_t occ = (decimal_t) data[map_util->getIndex(Vec2i(x,y))]/100;
        if(occ <= 0)
          continue;
        point_2d a;
        boost::geometry::assign_values(a, pt(0), pt(1));
        mapper.add(a);
        if(occ < 1)
          mapper.map(a, "fill-opacity:"+std::to_string(occ/2.0) +";fill:rgb(118,215,234);", 1);
        else
          mapper.map(a, "fill-opacity:1.0;fill:rgb(0,0,0);", 1);
    }
  }

  // Draw searched region
  for (const auto &pt : planner->getSearchRegion()) {
 // for (const auto &pt : planner->getCloseSet()) {
    point_2d a;
    boost::geometry::assign_values(a, pt(0), pt(1));
    mapper.add(a);
    mapper.map(a, "fill-opacity:0.2;fill:rgb(100,200,100);", 1); // Green
  }

  if (valid) {
    // Draw the trajectory
    double total_t = traj.getTotalTime();
    printf("Total time T: %f\n", total_t);
    printf("Total J:  J(VEL) = %f, J(ACC) = %f, J(JRK) = %f, J(SNP) = %f\n",
           traj.J(Control::VEL), traj.J(Control::ACC), traj.J(Control::JRK), traj.J(Control::SNP));
    int num = 200; // number of points on trajectory to draw
    const auto ws = traj.sample(num);
    boost::geometry::model::linestring<point_2d> line;
    for (const auto& w: ws)
        line.push_back(point_2d(w.pos(0), w.pos(1)));
    mapper.add(line);
    mapper.map(line,
        "opacity:0.4;fill:none;stroke:rgb(212,0,0);stroke-width:5"); // Red
	}

	if (valid_dist) {
		// Draw the trajectory
		double total_t = traj_dist.getTotalTime();
		printf("Total dist time T: %f\n", total_t);
		printf("Total dist J:  J(VEL) = %f, J(ACC) = %f, J(JRK) = %f, J(SNP) = %f, J(YAW) = %f\n",
					 traj_dist.J(Control::VEL), traj_dist.J(Control::ACC), traj_dist.J(Control::JRK), traj_dist.J(Control::SNP), traj_dist.Jyaw());
		int num = 200; // number of points on trajectory to draw
		const auto ws = traj_dist.sample(num);
		boost::geometry::model::linestring<point_2d> line;
		for (const auto& w: ws)
			line.push_back(point_2d(w.pos(0), w.pos(1)));
		mapper.add(line);
		mapper.map(line,
							 "opacity:0.8;fill:none;stroke:rgb(10,10,250);stroke-width:5"); // Blue

    // Draw yaw
    const auto ws_yaw = traj_dist.sample(20);
    Vec2f d(0.7, 0);
    for (const auto& w: ws_yaw) {
      decimal_t yaw = w.yaw;
      decimal_t yaw1 = yaw + yaw_max;
      decimal_t yaw2 = yaw - yaw_max;
      Mat2f Ryaw1, Ryaw2;
      Ryaw1 << cos(yaw1), -sin(yaw1), sin(yaw1), cos(yaw1);
      Ryaw2 << cos(yaw2), -sin(yaw2), sin(yaw2), cos(yaw2);
      Vec2f p1 = w.pos;
      Vec2f p2 = w.pos + Ryaw1*d;
      Vec2f p3 = w.pos + Ryaw2*d;
      Vec2f p4 = (p2+p3)/2;
      boost::geometry::model::linestring<point_2d> tria;
      tria.push_back(point_2d(p1(0), p1(1)));
      tria.push_back(point_2d(p2(0), p2(1)));
      tria.push_back(point_2d(p3(0), p3(1)));
      tria.push_back(point_2d(p1(0), p1(1)));
      tria.push_back(point_2d(p4(0), p4(1)));
      mapper.add(tria);
      mapper.map(tria,
                 "opacity:0.8;fill:none;stroke:rgb(212,0,0);stroke-width:2"); // Red
    }


	}

	// Write title at the lower right corner on canvas
	mapper.text(point_2d(origin_x + range_x - 13.5, origin_y+2.4), "test_distance_map_planner_2d_with_yaw",
							"fill-opacity:1.0;fill:rgb(10,10,250);");

  mapper.text(point_2d(origin_x + range_x - 13, origin_y+1.8), "Green: ",
              "fill-opacity:1.0;fill:rgb(100,200,100);");
  mapper.text(point_2d(origin_x + range_x - 10.5, origin_y+1.8), "search region",
              "fill-opacity:1.0;fill:rgb(0,0,0);");

  mapper.text(point_2d(origin_x + range_x - 13, origin_y+1.2), "Red: ",
              "fill-opacity:1.0;fill:rgb(237,10,63);");
  mapper.text(point_2d(origin_x + range_x - 10.5, origin_y+1.2), "original trajectory",
              "fill-opacity:1.0;fill:rgb(0,0,0);");

  mapper.text(point_2d(origin_x + range_x - 13, origin_y+0.6), "Blue:",
              "fill-opacity:1.0;fill:rgb(10,10,250);");
  mapper.text(point_2d(origin_x + range_x - 10.5, origin_y+0.6), "perturbed trajectory",
              "fill-opacity:1.0;fill:rgb(0,0,0);");


  return 0;
}
