;;; Warning!!! This file is derived from those in actions/menus/... do not edit here
;;CheckTiesInStaff
(define CheckTiesInStaff::return #t)
(d-PushPosition)
(d-MoveToBeginning)
(let ((ok #t))	
  (let loop ()
    (if (and (d-IsTied) (Singlenote?))
      (let ((note (d-GetNote)))
				(if (and (d-NextChord) (Singlenote?))
					(let ((nextnote (d-GetNote)))
						(if (equal? note nextnote)
							(if (d-NextChord)
								(loop))
							(begin
								(d-InfoDialog (_ "Tied notes not the same"))
								(set! ok #f))))
					(if (Singlenote?)
						(begin
							(d-InfoDialog "No note to tie to")
							(set! ok #f))))))
		(if (and ok (d-NextChord))
			(loop)))

 (if ok
   (d-PopPosition)
   (let ((position (GetPosition)))
     (set! CheckTiesInStaff::return #f)
     (d-PopPosition)
     (apply d-GoToPosition position))))
     
     
