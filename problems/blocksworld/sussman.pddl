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
  )
  (:init
    (arm-clear)
    (on-table a) (on-table b)
    (on c a)
    (clear c) (clear b)
  )
  (:goal (and (arm-clear) (on a b) (on b c) (on-table c)))
)
