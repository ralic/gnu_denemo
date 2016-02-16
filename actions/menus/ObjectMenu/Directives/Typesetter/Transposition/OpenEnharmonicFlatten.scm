;;;;OpenEnharmonicFlatten
(let ((tag  "OpenEnharmonicFlatten")(params "OpenEnharmonicFlatten::params"))
(if (equal? params "edit")
	(d-InfoDialog (_ "This Denemo Directive starts block of music that will be enhamonically flattened. The block of music should be closed with a curly brace }.t"))
	(begin
 (d-Directive-standalone tag)
 (d-DirectivePut-standalone-postfix tag "\\transpose d eeses{")
(d-DirectivePut-standalone-display tag (_ "Enh-"))
		(d-DirectivePut-standalone-graphic tag "
{b
denemo
40")
(d-DirectivePut-standalone-minpixels tag 30)
(d-RefreshDisplay)
		(d-DirectivePut-standalone-gy tag 10)
		(d-RefreshDisplay)
		(d-SetSaved #f))))