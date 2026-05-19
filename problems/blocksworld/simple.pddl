;; the classic sussman anomaly:
;;
;; initial state:
;;  a b
;; -----
;;
;; goal:
;;  a
;;  b
;; ---

(define (problem simple)
  (:domain blocksworld)
  (:objects
    a - block
    b - block
  )
  (:init
    (arm-clear)
    (on-table a) (on-table b)
    (clear a) (clear b)
  )
  (:goal (and (arm-clear) (on a b) (on-table b)))
)

;;(define (problem simple)
;;  (:domain blocksworld)
;;  (:objects
;;    a - block
;;  ;;  b - block
;;  )
;;  (:init
;;    (arm-clear)
;;    (on-table a)
;;    (clear a)
;;  )
;;  (:goal (and (holding a)))
;;)
