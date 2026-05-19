(define (problem dead-simple)
  (:domain blocksworld)
  (:objects
    a - block
  )
  (:init
    (arm-clear)
    (on-table a)
    (clear a)
  )
  (:goal (and (arm-clear)))
)
