(let ((tag "TerminateHairpinAtBarline"))
(d-MoveCursorRight)
(d-Directive-standalone tag)
(d-DirectivePut-standalone-postfix tag "<>\\!  ")
(d-DirectivePut-standalone-display tag (_ "End Hairpin"))
(d-DirectivePut-standalone-minpixels tag 30)
(d-MoveCursorRight)
(d-ChooseBarline)
(d-SetSaved #f)
(d-RefreshDisplay))
