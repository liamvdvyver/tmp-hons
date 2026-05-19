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
  )
  (:init
    (arm r1)
    (block a)
    (block b)
    (block c)
    (block d)
    (arm-clear r1)
    (on-table b) (on-table d)
    (clear a) (clear c)
    (on a b) (on c d)
  )
  (:goal (and (arm-clear r1) (on b a) (on-table a) (on d c) (on-table c)))
)

;;(define (problem simple)
;;  (:domain blocksworld)
;;  (:objects
;;    a - block
;;    r1 - arm
;;  ;;  b - block
;;  )
;;  (:init
;;    (arm r1)
;;    (block a)
;;    (arm-clear r1)
;;    (on-table a)
;;    (clear a)
;;  )
;;  (:goal (and (holding r1 a)))
;;)
