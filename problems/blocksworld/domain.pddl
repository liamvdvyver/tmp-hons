(define
  (domain blocksworld)
  (:requirements :strips :typing)
  (:types block)
  (:predicates
    (on ?a - block ?b - block)
    (on-table ?b - block)
    (holding ?b - block)
    (clear ?b - block)
    (arm-clear)
  )
  (:action pick
    :parameters (?b - block)
    :precondition (and (on-table ?b) (clear ?b) (arm-clear))
    :effect (and
      (not (arm-clear))
      (not (on-table ?b))
      (not (clear ?b))
      (holding ?b)
    )
  )
  (:action place
    :parameters (?b - block)
    :precondition (and (holding ?b))
    :effect (and
      (arm-clear)
      (on-table ?b)
      (clear ?b)
      (not (holding ?b))
    )
  )
  (:action pick-from
    :parameters (?a - block ?b - block)
    :precondition (and (on ?a ?b) (clear ?a) (arm-clear))
    :effect (and
      (not (arm-clear))
      (not (on ?a ?b))
      (holding ?a)
      (not (clear ?a))
      (clear ?b)
    )
  )
  (:action place-on
    :parameters (?a - block ?b - block)
    :precondition (and (clear ?b) (holding ?a))
    :effect (and
      (arm-clear)
      (on ?a ?b)
      (not (holding ?a))
      (clear ?a)
      (not (clear ?b))
    )
  )
)
