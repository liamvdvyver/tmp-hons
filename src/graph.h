#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "strips.h"

namespace graph {

struct Literal {
  bool positive = true;
  strips::GroundAtom atom;

  bool operator==(const Literal &other) const {
    return positive == other.positive && atom.predicate_id == other.atom.predicate_id &&
           atom.object_ids == other.atom.object_ids;
  }
};

struct FactLayer {
  std::vector<Literal> literals;
  std::unordered_set<std::string> literal_set;
  std::unordered_set<std::string> mutexes;
};

struct ActionInstance {
  strips::ActionId action_id = 0;
  std::vector<strips::ObjectId> object_ids;
  std::vector<Literal> preconditions;
  std::vector<Literal> effects;
  bool is_noop = false;

  std::string pretty(const strips::Domain &domain,
                     const std::vector<std::string> &objects) const;
};

struct ActionLayer {
  std::vector<ActionInstance> actions;
  std::unordered_set<std::string> mutexes;
};

using ParallelPlan = std::vector<std::vector<ActionInstance>>;

class PlanningGraph {
public:
  PlanningGraph(const strips::Domain &domain, const strips::Problem &problem);
  virtual ~PlanningGraph() = default;

  virtual void extend();
  void build_to_length(int n);
  void build_until_fixpoint();
  void build_until_goal_or_fixpoint();

  int horizon() const;
  bool leveled_off() const;

  bool goal_reachable(int t) const;
  bool literals_mutex(const Literal &a, const Literal &b, int t) const;
  bool actions_mutex(int t, int i, int j) const;

  std::optional<ParallelPlan> extract_plan(int t) const;

  const std::vector<FactLayer> &fact_layers() const;
  const std::vector<ActionLayer> &action_layers() const;
  const std::vector<std::string> &objects() const;
  const strips::Domain &domain() const;
  const strips::Problem &problem() const;

protected:
  const strips::Domain &domain_;
  const strips::Problem &problem_;
  std::vector<std::string> objects_;
  std::unordered_map<std::string, strips::ObjectId> object_ids_;
  std::vector<FactLayer> fact_layers_;
  std::vector<ActionLayer> action_layers_;
  bool leveled_off_ = false;
};

class IncrementalPlanningGraph : public PlanningGraph {
public:
  IncrementalPlanningGraph(const PlanningGraph &base_graph,
                           std::unordered_set<std::string> additional_action_mutexes);

  void extend() override;

private:
  const PlanningGraph &base_graph_;
  std::unordered_set<std::string> additional_action_mutexes_;
};

std::string literal_key(const Literal &literal);
std::string mutex_key(const std::string &a, const std::string &b);
std::string action_key(strips::ActionId action_id,
                       const std::vector<strips::ObjectId> &object_ids);
std::string action_key(const ActionInstance &action);

} // namespace graph
