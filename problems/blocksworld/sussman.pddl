;; The classic Sussman anomaly:
;;
;; initial state:
;;  C
;;  A B
;; -----
;;
;; goal:
;;  A
;;  B
;;  C
;; ---

(define (problem sussman)
  (:domain blocksworld)
  (:objects
    A - block
    B - block
    C - block
  )
  (:init
    (arm-clear)
    (on-table A) (on-table B)
    (on C A)
    (clear C) (clear B)
  )
  (:goal (and (arm-clear) (on A B) (on B C) (on-table C)))
)
