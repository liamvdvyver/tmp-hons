;; the classic sussman anomaly:
;;
;; initial state:
;;  a c
;;  b d
;; -----
;;
;; goal:
;;  b d
;;  a c
;; -----

(define (problem simple)
  (:domain blocksworld)
  (:objects
    a - block
    b - block
    c - block
    d - block
    r1 - arm
    r2 - arm
  )
  (:init
    (arm r1)
    (arm r2)
    (block a)
    (block b)
    (block c)
    (block d)
    (arm-clear r1)
    (arm-clear r2)
    (on-table b) (on-table d)
    (clear a) (clear c)
    (on a b) (on c d)
  )
  (:goal (and (arm-clear r1) (arm-clear r2) (on b a) (on-table a) (on d c) (on-table c)))
)
