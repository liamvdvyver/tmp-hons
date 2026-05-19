# Towards heuristic algorithms for task and motion planning: extending the SMT-planning paradigm

Liam van der Vyver, u7309661, COMP4450

## Cluster

Intelligent systems: planning, including task and motion planning, is a subfield of classical AI.

## Keywords

Task and motion planning, heuristic search

## National Interest Test Statement

This project aims to address shortcomings in the performance of intelligent robotic
systems which must plan their actions in advance, particularly the ability of these systems
to investigate the best options first to save computation time.
Addressing existing shortcomings of intelligent robotic systems increases
the economic contributions which can be made to the Australian economy through automation, and
positioning Australia as a leader in the global robotics industry by locally developing expertise
maximises the role Australian businesses, institutions, and jobs can have
in the adoption of these systems, as demand for automation can be filled domestically.

## Introduction

Task and motion planning (TAMP) systems jointly perform high level symbolic reasoning (task planning),
as well as low level motion planning. As advances in computer vision and language
modelling allow for increasing complex symbolic problems, and robots become increasingly
high degree of freedom, performance constraints remain pertinent to the usability of these systems.
In (task) planning, the state-of-the-art relies on heuristic search to find good plans
quickly, at the cost of optimality. The geometric nature of motion planning problems
lends itself well to heuristics, however this information cannot be effectively
utilised for the task planning.

Discovery of constraints to prune the search space of the task planning problem
has shown promise in the TAMP literature, however integrating additional 
constraints is identified as a research gap.

This project aims to explore the extent to which heuristic methods can address these gaps, specifically:

Can general-purpose heuristics discovered during motion planning,
or problem specific heuristics specified by standalone components,
help TAMP systems to plan best-first, or on a pruned space?

The proposal is structured in a review of modern TAMP systems and classical
heuristic planning methods, the extension of an existing TAMP framework
to incorporate heuristics, and analysis of a reference implementation's performance.

## Research problem

Since the development of the HSP and FF planning systems, symbolic task planning has been dominated
by heuristic search methods, which construct relaxed and computationally simplified
abstractions of a problem to immediately search the most promising solutions, even at the cost of optimality.

Iteratively Deepened Task and Motion Planning (IDTMP) is a TMP algorithm which utilises
Satisfiability Modulo Theory (SMT) solvers and a black-box motion planner to
solve TAMP problems. Generalisation of certain motion planning failures
to prune the planning problem yields performance gains, however further constraints
are identified as a gap to be addressed to improve scalability. IDTMP utilises
incremental SMT solvers, in which constraints can be added and removed efficiently.

Unlike heuristic state-space search methods in classical planning,
SMT-based planners do not perform the search themselves, but rather
formally encode the planning problem and use an SMT solver to plan.
If heuristics are used for ordering the search space, rather than pruning,
it is also necessary to devise constraints which may be
added/removed in order to do so.

While previous work does use constraints to prune the search space,
this pruning is not done heuristically, which may provide further improvement gains.
Further, best-first SMT-planning is a novel contribution to the TAMP literature.

## Research design

The project will begin with an implementation of the IDTMP algorithm, and an
analysis of it's performance in a number of common toy domains to identify
those with the highest scalability concerns. This will be performed in a software
robotics simulator such as PyBullet.

Several extensions of algorithm will be extended, with three initially proposed directions:

* Incorporating existing heuristic/constraint methods from classical planning,
* Extending the collision-generalisation constraints to prune more aggressively,
* Utilising a (black box) heuristic cost evaluator to search within
  heuristic cost bounds rather than plan length bounds.

These extensions, along with any more which are developed, will be used alongside
the base IDTMP algorithm, to solve TMP problems in the identified toy domains,
and performance on problems of various sizes will be measured.

## Evaluation criteria

The project will be evaluated based on the extent to which heuristic methods
improve performance on the tested toy problems, and the extend to which
the data collected are informative about the applicability of similar methods
used in classical planning to the task and motion planning problem.

While it is not reasonable to expect that all heuristics/algorithms will improve
performance on all problems tested, it is a reasonable minimum that toy problems
which admit good heuristics should benefit.

Once the performance of the new algorithms has been measured,
performance will be statistically analysed through regression analysis
to determine if heuristic methods scale better than the base algorithm.

The quality (cost) of the plans found by each method will also be compared.
If extremely sub-optimal plans are found quickly, this is a less
useful contribution for most applications.

## Ethics

No human data will be used for the project.

## Conclusion

By extending the real-world performance gains of heuristic methods
to the TAMP problem, this project will further develop the
constraint-based methods available in SMT-based TAMP.
The resulting methods will be useful for AI-practitioners,
allowing problems in larger domains, or higher-dimensional
robot configuration spaces to be solved in reasonable amounts of time.

## Reflection

I did not use Generative AI for any writing in this proposal.
I have used it to check my work for errors which I have manually corrected,
and in familiarising myself with the subject matter it has been useful
in testing my intuition.

I was unsure which area of Task and Motion Planning I wanted to research,
but this topic is one in which my general interest in heuristic search
(which developed when studying game-playing) is applicable.
Heuristic methods have dominated in domains such as chess,
so it has been interesting to see how they can apply to other sub-fields of AI.
In the future, I will always look for intersections between my different interests,
since these are often ripe with potential for new work.

## Bibliography
