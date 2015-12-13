(let ((params PlaybackView::params)(tag "Temp")(tag2 "Temp2")(saved (d-GetSaved)))
    (define (no-tempo-at-start)
        (define no-tempo #t)
        (d-PushPosition)
        (d-MoveToBeginning)
      
        (let loop ()
          (if (Music?)
            (set! no-tempo #t)
            (if (d-Directive-standalone? "MetronomeMark")
                (set! no-tempo #f)
                (if (d-NextObjectInMeasure)
                    (loop)
                    (set! no-tempo #t)))))
        (d-PopPosition)
        (disp "Found no-tempo " no-tempo "\n\n")
        no-tempo)



    (d-DirectivePut-score-override tag DENEMO_OVERRIDE_AFFIX)
    (d-DirectivePut-score-prefix tag "\n\\include \"live-score.ily\"\n")
    (if (no-tempo-at-start)
      (d-DirectivePut-voice-postfix tag (string-append " \\set Score.tempoHideNote = ##t \\tempo 4=" (number->string (d-MovementTempo)) " ")))
    (d-DirectivePut-score-prefix tag2 "\n\\header { tagline = #f }\n")
    (d-DirectivePut-score-postfix tag2 "  \\applyContext #(override-color-for-all-grobs (x11-color 'black)) ")
    (d-DirectivePut-movementcontrol-postfix tag "\n\\layout{}\\midi {}\n")
    (d-DirectivePut-movementcontrol-override tag DENEMO_OVERRIDE_AFFIX)
    (d-DirectivePut-paper-postfix tag "
    ragged-bottom = ##t
    #(set! paper-alist (cons '(\"custom-size\" . (cons (* 20 cm) (* 100 cm))) paper-alist))
    #(set-paper-size \"custom-size\")")
    (d-SetSaved saved)
    (d-DisplayTypesetSvg (/ (string->number  (d-ScoreProperties "query=fontsize"))18.0) params)
    (d-DirectiveDelete-movementcontrol tag)
    (d-DirectiveDelete-paper tag)
    (d-DirectiveDelete-score tag)
    (d-DirectiveDelete-voice tag)
    (d-DirectiveDelete-score tag2)
    (d-DirectivePut-score-prefix tag2 "\n%\\header { tagline = #f }\n") ;;to keep the same line numbers we don't delete this line but comment it out
    (d-SetSaved saved)
    )
