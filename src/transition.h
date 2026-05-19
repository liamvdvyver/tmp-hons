#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <z3++.h>

#include "strips.h"

using ActionMap = std::unordered_map<std::string, strips::GroundAction>;

struct GroundAtomTemplate {
  strips::PredicateId predicate_id;
  std::vector<strips::ObjectId> object_ids;
};

struct GroundActionTemplate {
  strips::GroundAction action;
  std::vector<strips::GroundLiteral> preconditions;
  std::vector<strips::GroundLiteral> effects;
};

class TransitionCache {
public:
  TransitionCache(const strips::Domain &domain,
                  const std::vector<std::string> &objects,
                  bool partially_ordered);

  ActionMap add_transition(z3::context &ctx, z3::solver &solver, int t) const;

private:
  const strips::Domain &domain_;
  const std::vector<std::string> &objects_;
  bool partially_ordered_;
  std::vector<GroundAtomTemplate> atoms_;
  std::vector<GroundActionTemplate> actions_;
  std::vector<std::vector<std::size_t>> atom_explainers_;
  std::vector<std::vector<std::size_t>> action_conflicts_;
};
