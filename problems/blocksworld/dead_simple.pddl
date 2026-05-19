(define (problem dead-simple)
  (:domain blocksworld)
  (:objects
    a - block
    r1 - arm
  )
  (:init
    (arm r1)
    (block a)
    (arm-clear r1)
    (on-table a)
    (clear a)
  )
  (:goal (and (holding r1 a)))
)
