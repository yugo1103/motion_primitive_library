/**
 * @file graph_search.h
 * @brief backend of graph search, implemetation of A* and Lifelong Planning A*
 */

#ifndef MPL_GRAPH_SEARCH_H
#define MPL_GRAPH_SEARCH_H

#include <mpl_planner/common/state_space.h>
#include <mpl_basis/trajectory.h>

namespace MPL {

/**
 * @brief GraphSearch class
 *
 * Implement A* and Lifelong Planning A*
 */
template <int Dim, typename Coord> class GraphSearch {
public:
  /**
   * @brief Simple empty constructor
   *
   * @param verbose enable print out debug infos, default is set to False
   */
  GraphSearch(bool verbose = false) : verbose_(verbose){};

  /**
   * @brief Astar graph search
   *
   * @param start_coord start state
   * @param start_key key of the start state
   * @param ENV object of `env_base' class
   * @param ss_ptr workspace input
   * @param traj output trajectory
   * @param max_expand max number of expanded states, default value is -1 which
   * means there is no limitation
   * @param max_t max time horizon of expanded states, default value is -1 which
   * means there is no limitation
   */
  bool Astar(const Coord &start_coord, Key start_key,
                  const std::shared_ptr<env_base<Dim>> &ENV,
                  std::shared_ptr<StateSpace<Dim, Coord>> &ss_ptr,
                  Trajectory<Dim> &traj, int max_expand = -1,
                  decimal_t max_t = 0) {
    // Check if done
    if (ENV->is_goal(start_coord))
      return 0;

    // Initialize start node
    StatePtr<Coord> currNode_ptr = ss_ptr->hm_[start_key];
    if (ss_ptr->pq_.empty()) {
      if (verbose_)
        printf(ANSI_COLOR_GREEN "Start from new node!\n" ANSI_COLOR_RESET);
      currNode_ptr = std::make_shared<State<Coord>>(start_key, start_coord);
      currNode_ptr->g = 0;
      currNode_ptr->h = ss_ptr->eps_ == 0 ? 0 : ENV->get_heur(start_coord);
      decimal_t fval = currNode_ptr->g + ss_ptr->eps_ * currNode_ptr->h;
      currNode_ptr->heapkey =
        ss_ptr->pq_.push(std::make_pair(fval, currNode_ptr));
      currNode_ptr->iterationopened = true;
      currNode_ptr->iterationclosed = false;
      ss_ptr->hm_[start_key] = currNode_ptr;
    }

    int expand_iteration = 0;
    while (true) {
      expand_iteration++;
      // get element with smallest cost
      currNode_ptr = ss_ptr->pq_.top().second;
      ss_ptr->pq_.pop();
      currNode_ptr->iterationclosed = true; // Add to closed list

      // Get successors
      vec_E<Coord> succ_coord;
      std::vector<MPL::Key> succ_key;
      std::vector<decimal_t> succ_cost;
      std::vector<int> succ_act_id;

      ENV->get_succ(currNode_ptr->coord, succ_coord, succ_key, succ_cost,
                    succ_act_id);

      // Process successors (satisfy dynamic constraints but might hit obstacles)
      for (unsigned s = 0; s < succ_coord.size(); ++s) {
        // If the primitive is occupied, skip
        if (std::isinf(succ_cost[s]))
          continue;

        // Get child
        StatePtr<Coord> &succNode_ptr = ss_ptr->hm_[succ_key[s]];
        if (!succNode_ptr) {
          succNode_ptr = std::make_shared<State<Coord>>(succ_key[s], succ_coord[s]);
          succNode_ptr->h = ss_ptr->eps_ == 0 ? 0 : ENV->get_heur(succNode_ptr->coord);
          /*
           * Comment this block if build multiple connected graph
           succNode_ptr->pred_hashkey.push_back(currNode_ptr->hashkey);
           succNode_ptr->pred_action_id.push_back(succ_act_id[s]);
           succNode_ptr->pred_action_cost.push_back(succ_cost[s]);
           */
        }


        /**
         * Comment following if build single connected graph
         * */
        succNode_ptr->pred_hashkey.push_back(currNode_ptr->hashkey);
        succNode_ptr->pred_action_cost.push_back(succ_cost[s]);
        succNode_ptr->pred_action_id.push_back(succ_act_id[s]);
        //*/

        // see if we can improve the value of successor
        // taking into account the cost of action
        decimal_t tentative_gval = currNode_ptr->g + succ_cost[s];

        if (tentative_gval < succNode_ptr->g) {
          /**
           * Comment this block if build multiple connected graph
           succNode_ptr->pred_hashkey.front() = currNode_ptr->hashkey;  // Assign
           new parent
           succNode_ptr->pred_action_id.front() = succ_act_id[s];
           succNode_ptr->pred_action_cost.front() = succ_cost[s];
           */
          succNode_ptr->g = tentative_gval; // Update gval

          decimal_t fval = succNode_ptr->g + (ss_ptr->eps_) * succNode_ptr->h;

          // if currently in OPEN, update
          if (succNode_ptr->iterationopened && !succNode_ptr->iterationclosed) {
            if (verbose_) {
              if ((*succNode_ptr->heapkey).first < fval) {
                std::cout << "UPDATE fval(old) = "
                  << (*succNode_ptr->heapkey).first << std::endl;
                std::cout << "UPDATE fval = " << fval << std::endl;
              }
            }

            (*succNode_ptr->heapkey).first = fval; // update heap element
            // ss_ptr->pq.update(succNode_ptr->heapkey);
            ss_ptr->pq_.increase(succNode_ptr->heapkey); // update heap
            // printf(ANSI_COLOR_RED "ASTAR ERROR!\n" ANSI_COLOR_RESET);
          } else // new node, add to heap
          {
            // std::cout << "ADD fval = " << fval << std::endl;
            succNode_ptr->heapkey =
              ss_ptr->pq_.push(std::make_pair(fval, succNode_ptr));
            succNode_ptr->iterationopened = true;
          }
        }
      }

      // If goal reached, abort!
      if (ENV->is_goal(currNode_ptr->coord))
        break;

      // If maximum time reached, abort!
      if (max_t > 0 && currNode_ptr->coord.t >= max_t && !std::isinf(currNode_ptr->g)) {
        if (verbose_)
          printf(ANSI_COLOR_GREEN
                 "MaxExpandTime [%f] Reached!!!!!!\n\n" ANSI_COLOR_RESET,
                 max_t);
        break;
      }

      // If maximum expansion reached, abort!
      if (max_expand > 0 && expand_iteration >= max_expand) {
        printf(ANSI_COLOR_RED
               "MaxExpandStep [%d] Reached!!!!!!\n\n" ANSI_COLOR_RESET,
               max_expand);
        //return std::numeric_limits<decimal_t>::infinity();
        return false;
      }

      // If pq is empty, abort!
      if (ss_ptr->pq_.empty()) {
        printf(ANSI_COLOR_RED
               "Priority queue is empty!!!!!!\n\n" ANSI_COLOR_RESET);
        //return std::numeric_limits<decimal_t>::infinity();
        return false;
      }
    }

    if (verbose_) {
      decimal_t fval = ss_ptr->calculateKey(currNode_ptr);
      printf(ANSI_COLOR_GREEN "goalNode fval: %f, g: %f!\n" ANSI_COLOR_RESET,
             fval, currNode_ptr->g);
      printf(ANSI_COLOR_GREEN "Expand [%d] nodes!\n" ANSI_COLOR_RESET,
             expand_iteration);
    }

    if (ENV->is_goal(currNode_ptr->coord)) {
      if (verbose_)
        printf(ANSI_COLOR_GREEN "Reached Goal !!!!!!\n\n" ANSI_COLOR_RESET);
    }

    ss_ptr->expand_iteration_ = expand_iteration;
    traj = recoverTraj(currNode_ptr, ss_ptr, ENV, start_key);
    //return currNode_ptr->g;
    return true;
  }


  /**
   * @brief Lifelong Planning Astar graph search
   *
   * @param start_coord start state
   * @param start_key key of the start state
   * @param ENV object of `env_base' class
   * @param ss_ptr workspace input
   * @param traj output trajectory
   * @param max_expand max number of expanded states, default value is -1 which
   * means there is no limitation
   * @param max_t max time horizon of expanded states, default value is -1 which
   * means there is no limitation
   */
  decimal_t LPAstar(const Coord &start_coord, Key start_key,
                    const std::shared_ptr<env_base<Dim>> &ENV,
                    std::shared_ptr<StateSpace<Dim, Coord>> &ss_ptr,
                    Trajectory<Dim> &traj, int max_expand = -1,
                    decimal_t max_t = 0) {
    // Check if done
    if (ENV->is_goal(start_coord)) {
      if (verbose_)
        printf(ANSI_COLOR_GREEN
               "Start is inside goal region!\n" ANSI_COLOR_RESET);
      return 0;
    }

    // set Tmax in ss_ptr
    ss_ptr->max_t_ =
      max_t > 0 ? max_t : std::numeric_limits<decimal_t>::infinity();

    // Initialize start node
    StatePtr<Coord> currNode_ptr = ss_ptr->hm_[start_key];
    if (!currNode_ptr) {
      if (verbose_)
        printf(ANSI_COLOR_GREEN "Start from new node!\n" ANSI_COLOR_RESET);
      currNode_ptr = std::make_shared<State<Coord>>(start_key, start_coord);
      currNode_ptr->g = std::numeric_limits<decimal_t>::infinity();
      currNode_ptr->rhs = 0;
      currNode_ptr->h = ss_ptr->eps_ == 0 ? 0 : ENV->get_heur(start_coord);
      currNode_ptr->heapkey = ss_ptr->pq_.push(
        std::make_pair(ss_ptr->calculateKey(currNode_ptr), currNode_ptr));
      currNode_ptr->iterationopened = true;
      currNode_ptr->iterationclosed = false;
      ss_ptr->hm_[start_key] = currNode_ptr;
    }

    // Initialize goal node
    StatePtr<Coord> goalNode_ptr = std::make_shared<State<Coord>>(Key(), Coord());
    if (!ss_ptr->best_child_.empty() &&
         ENV->is_goal(ss_ptr->best_child_.back()->coord))
      goalNode_ptr = ss_ptr->best_child_.back();

    int expand_iteration = 0;
    while (ss_ptr->pq_.top().first < ss_ptr->calculateKey(goalNode_ptr) ||
           goalNode_ptr->rhs != goalNode_ptr->g) {
      expand_iteration++;
      // Get element with smallest cost
      currNode_ptr = ss_ptr->pq_.top().second;
      ss_ptr->pq_.pop();
      currNode_ptr->iterationclosed = true; // Add to closed list

      if (currNode_ptr->g > currNode_ptr->rhs)
        currNode_ptr->g = currNode_ptr->rhs;
      else {
        currNode_ptr->g = std::numeric_limits<decimal_t>::infinity();
        ss_ptr->updateNode(currNode_ptr);
      }

      // Get successors
      vec_E<Coord> succ_coord = currNode_ptr->succ_coord;
      std::vector<MPL::Key> succ_key = currNode_ptr->succ_hashkey;
      std::vector<decimal_t> succ_cost = currNode_ptr->succ_action_cost;
      std::vector<int> succ_act_id = currNode_ptr->succ_action_id;

      bool explored = false;
      if (currNode_ptr->succ_hashkey.empty()) {
        explored = true;
        ENV->get_succ(currNode_ptr->coord, succ_coord, succ_key, succ_cost,
                      succ_act_id);
        currNode_ptr->succ_coord.resize(succ_coord.size());
        currNode_ptr->succ_hashkey.resize(succ_coord.size());
        currNode_ptr->succ_action_id.resize(succ_coord.size());
        currNode_ptr->succ_action_cost.resize(succ_coord.size());
      }

      // Process successors
      for (unsigned s = 0; s < succ_key.size(); ++s) {
        // Get child
        StatePtr<Coord> &succNode_ptr = ss_ptr->hm_[succ_key[s]];
        if (!(succNode_ptr)) {
          succNode_ptr = std::make_shared<State<Coord>>(succ_key[s], succ_coord[s]);
          succNode_ptr->h = ss_ptr->eps_ == 0 ? 0 : ENV->get_heur(succNode_ptr->coord);
        }

        // store the hashkey
        if (explored) {
          currNode_ptr->succ_coord[s] = succ_coord[s];
          currNode_ptr->succ_hashkey[s] = succ_key[s];
          currNode_ptr->succ_action_id[s] = succ_act_id[s];
          currNode_ptr->succ_action_cost[s] = succ_cost[s];
        }

        int id = -1;
        for (unsigned int i = 0; i < succNode_ptr->pred_hashkey.size(); i++) {
          if (succNode_ptr->pred_hashkey[i] == currNode_ptr->hashkey) {
            id = i;
            break;
          }
        }
        if (id == -1) {
          succNode_ptr->pred_hashkey.push_back(currNode_ptr->hashkey);
          succNode_ptr->pred_action_cost.push_back(succ_cost[s]);
          succNode_ptr->pred_action_id.push_back(succ_act_id[s]);
        }

        ss_ptr->updateNode(succNode_ptr);
      }

      // If goal reached or maximum time reached, terminate!
      if (ENV->is_goal(currNode_ptr->coord) || max_t > 0)
        goalNode_ptr = currNode_ptr;

      // If maximum expansion reached, abort!
      if (max_expand > 0 && expand_iteration >= max_expand) {
        if (verbose_)
          printf(ANSI_COLOR_RED
                 "MaxExpandStep [%d] Reached!!!!!!\n\n" ANSI_COLOR_RESET,
                 max_expand);
        return std::numeric_limits<decimal_t>::infinity();
      }

      // If pq is empty, abort!
      if (ss_ptr->pq_.empty()) {
        if (verbose_)
          printf(ANSI_COLOR_RED
                 "Priority queue is empty!!!!!!\n\n" ANSI_COLOR_RESET);
        return std::numeric_limits<decimal_t>::infinity();
      }
    }

    //***** Report value of goal
    if (verbose_) {
      printf(ANSI_COLOR_GREEN
             "goalNode fval: %f, g: %f, rhs: %f!\n" ANSI_COLOR_RESET,
             ss_ptr->calculateKey(goalNode_ptr), goalNode_ptr->g,
             goalNode_ptr->rhs);
      // printf(ANSI_COLOR_GREEN "currNode fval: %f, g: %f, rhs: %f!\n"
      // ANSI_COLOR_RESET,
      //     ss_ptr->calculateKey(currNode_ptr), currNode_ptr->g,
      //     currNode_ptr->rhs);
      printf(ANSI_COLOR_GREEN "Expand [%d] nodes!\n" ANSI_COLOR_RESET,
             expand_iteration);
    }

    //****** Check if the goal is reached, if reached, set the flag to be True
    if (verbose_) {
      if (ENV->is_goal(goalNode_ptr->coord))
        printf(ANSI_COLOR_GREEN "Reached Goal !!!!!!\n\n" ANSI_COLOR_RESET);
      else
        printf(ANSI_COLOR_GREEN
               "MaxExpandTime [%f] Reached!!!!!!\n\n" ANSI_COLOR_RESET,
               goalNode_ptr->coord.t);
    }

    // auto start = std::chrono::high_resolution_clock::now();
    //****** Recover trajectory
    traj = recoverTraj(goalNode_ptr, ss_ptr, ENV, start_key);

    // std::chrono::duration<decimal_t> elapsed_seconds =
    // std::chrono::high_resolution_clock::now() - start;
    // printf("time for recovering: %f, expand: %d\n", elapsed_seconds.count(),
    // expand_iteration);

    ss_ptr->expand_iteration_ = expand_iteration;
    return goalNode_ptr->g;
  }
private:
  /// Recover trajectory
  Trajectory<Dim> recoverTraj(
    StatePtr<Coord> currNode_ptr,
    std::shared_ptr<StateSpace<Dim, Coord>> ss_ptr,
    const std::shared_ptr<env_base<Dim>> &ENV, const Key &start_key) {
  // Recover trajectory
  ss_ptr->best_child_.clear();

  vec_E<Primitive<Dim>> prs;
  while (!currNode_ptr->pred_hashkey.empty()) {
    if (verbose_) {
      std::cout << "t: " << currNode_ptr->coord.t << " --> "
                << currNode_ptr->coord.t - ss_ptr->dt_ << std::endl;
      printf("g: %f, rhs: %f, h: %f\n", currNode_ptr->g, currNode_ptr->rhs,
             currNode_ptr->h);
    }
    ss_ptr->best_child_.push_back(currNode_ptr);
    int min_id = -1;
    decimal_t min_rhs = std::numeric_limits<decimal_t>::infinity();
    decimal_t min_g = std::numeric_limits<decimal_t>::infinity();
    for (unsigned int i = 0; i < currNode_ptr->pred_hashkey.size(); i++) {
      Key key = currNode_ptr->pred_hashkey[i];
      // std::cout << "action id: " << currNode_ptr->pred_action_id[i] << "
      // parent g: " << ss_ptr->hm_[key]->g << " action cost: " <<
      // currNode_ptr->pred_action_cost[i] << " parent key: " <<key <<
      // std::endl;
      if (min_rhs > ss_ptr->hm_[key]->g + currNode_ptr->pred_action_cost[i]) {
        min_rhs = ss_ptr->hm_[key]->g + currNode_ptr->pred_action_cost[i];
        min_g = ss_ptr->hm_[key]->g;
        min_id = i;
      } else if (!std::isinf(currNode_ptr->pred_action_cost[i]) &&
                 min_rhs ==
                     ss_ptr->hm_[key]->g + currNode_ptr->pred_action_cost[i]) {
        if (min_g < ss_ptr->hm_[key]->g) {
          min_g = ss_ptr->hm_[key]->g;
          min_id = i;
        }
      }
    }

    if (min_id >= 0) {
      Key key = currNode_ptr->pred_hashkey[min_id];
      int action_idx = currNode_ptr->pred_action_id[min_id];
      currNode_ptr = ss_ptr->hm_[key];
      Primitive<Dim> pr;
      ENV->forward_action(currNode_ptr->coord, action_idx, pr);
      prs.push_back(pr);
      if (verbose_) {
        // std::cout << "parent t: " << currNode_ptr->t << " key: " << key <<
        // std::endl;
        // printf("Take action id: %d,  action cost: J: [%f, %f, %f]\n",
        // action_idx, pr.J(0), pr.J(1), pr.J(2));
        // print_coeffs(pr);
      }
    } else {
      if (verbose_) {
        printf(ANSI_COLOR_RED
               "Trace back failure, the number of predecessors is %zu: \n",
               currNode_ptr->pred_hashkey.size());
        for (unsigned int i = 0; i < currNode_ptr->pred_hashkey.size(); i++) {
          Key key = currNode_ptr->pred_hashkey[i];
          printf("i: %d, gvalue: %f, cost: %f\n" ANSI_COLOR_RESET, i,
                 ss_ptr->hm_[key]->g, currNode_ptr->pred_action_cost[i]);
        }
      }

      break;
    }

    if (currNode_ptr->hashkey == start_key) {
      ss_ptr->best_child_.push_back(currNode_ptr);
      break;
    }
  }

  std::reverse(prs.begin(), prs.end());
  std::reverse(ss_ptr->best_child_.begin(), ss_ptr->best_child_.end());
  return Trajectory<Dim>(prs);
}
  /// Verbose flag
  bool verbose_ = false;
};
}
#endif
