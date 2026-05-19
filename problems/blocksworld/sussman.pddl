;; the classic sussman anomaly:
;;
;; initial state:
;;  c
;;  a b
;; -----
;;
;; goal:
;;  a
;;  b
;;  c
;; ---

(define (problem sussman)
  (:domain blocksworld)
  (:objects
    a - block
    b - block
    c - block
    r1 - arm
  )
  (:init
    (arm r1)
    (block a)
    (block b)
    (block c)
    (arm-clear r1)
    (on-table a) (on-table b)
    (on c a)
    (clear c) (clear b)
  )
  (:goal (and (arm-clear r1) (on a b) (on b c) (on-table c)))
)
