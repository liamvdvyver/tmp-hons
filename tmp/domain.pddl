(define
  (domain blocksworld)
  (:requirements :strips :typing)
  (:types block arm)
  (:predicates
    (on ?a - block ?b - block)
    (on-table ?b - block)
    (holding ?a - arm ?b - block)
    (held ?b - block)
    (arm-clear ?a - arm)
    (block ?b - block)
    (arm ?a - arm)
  )
  (:action pick
    :parameters (?a - arm ?b - block)
    :precondition (and (arm ?a) (block ?b) (on-table ?b) (arm-clear ?a) (not (held ?b)))
    :effect (and
      (not (arm-clear ?a))
      (not (on-table ?b))
      (holding ?a ?b)
      (held ?b)
    )
  )
  (:action place
    :parameters (?a - arm ?b - block)
    :precondition (and (holding ?a ?b) (held ?b))
    :effect (and
      (arm-clear ?a)
      (on-table ?b)
      (not (holding ?a ?b))
      (not (held ?b))
    )
  )
  (:action pick-from
    :parameters (?a - arm ?b1 - block ?b2 - block)
    :precondition (and (on ?b1 ?b2) (arm-clear ?a) (not (held ?b1)))
    :effect (and
      (not (arm-clear ?a))
      (not (on ?b1 ?b2))
      (holding ?a ?b1)
      (held ?b1)
    )
  )
  (:action place-on
    :parameters (?a - arm ?b1 - block ?b2 - block)
    :precondition (and (holding ?a ?b1) (held ?b1))
    :effect (and
      (arm-clear ?a)
      (on ?b1 ?b2)
      (not (holding ?a ?b1))
      (not (held ?b1))
    )
  )
)
