;;MultiMeasureRests
  (let ((WMRtag DenemoWholeMeasureRestTag)(MMRtag "MultiMeasureRests"))
    (define count 1)
    (define (count-grouped-rests)
        (let outer-loop ()
            (if (d-MoveToMeasureRight)
            (let loop ()
                (if (d-Directive-standalone? MMRtag)
                    (begin
                        (set! count (1+ count))
                        (outer-loop)
                        )
                    (if (d-NextObjectInMeasure)
                        (loop)))))))
  (define (re-calculate)        
            (d-PushPosition)
             (count-grouped-rests)
            (d-PopPosition)
            (if (zero? count)
                (begin (disp "Need to delete and do wholemeasurerest"))
                (begin
                    (d-SetSaved #f)
                     (d-DirectivePut-standalone-display MMRtag (string-append "Rest " (number->string count)))
                     (d-DirectivePut-standalone-postfix MMRtag (string-append "\\set Score.skipBars = ##t R1*" (d-InsertTimeSig "query=timesigname") "*" (number->string count))))))  
 (define (ungroup)
    (if (d-Directive-standalone? MMRtag)
        (begin
            (d-DeleteObject)
            (d-WholeMeasureRest)))
    (let outer-loop ()
            (if (d-MoveToMeasureRight)
            (let loop ()
                (if (d-Directive-standalone? MMRtag)
                    (begin
                        (d-DeleteObject)
                    (d-WholeMeasureRest)
                        (outer-loop))
                    (if (d-NextObjectInMeasure)
                        (loop)))))))
  (define (next-chord-good)
    (let loop ()
        (if  (and (or (d-NextObjectInMeasure) (and (d-MeasureRight) (not (EmptyMeasure?)))) (not (d-Directive-standalone?)) (not (Timesignature?)) (not (Music?)))
            (loop)
            (Music?))))
  (define (groupable-wholemeasure-rest?)
    (and (d-Directive-chord? WMRtag) (not (d-GetNonprinting)) (not (d-DirectiveGetNthTag-chord 1))))
  ;;; procedure starts here  
    (cond ((groupable-wholemeasure-rest?)
            (set! count 1)
            (d-DeleteObject)
            (d-DirectivePut-standalone-minpixels MMRtag 100)
            (d-DirectivePut-standalone-graphic MMRtag  "MultiMeasureRests")
            (d-DirectivePut-standalone-gx  MMRtag  50)
            (d-DirectivePut-standalone-gy MMRtag  -2)
            (d-PushPosition)
            (let loop ()
            (if (next-chord-good)
                (begin      
                        (if (groupable-wholemeasure-rest?)
                        (begin
                                (set! count (1+ count))
                            (d-DeleteObject)
                                (d-Directive-standalone MMRtag)
                                (d-SetDurationInTicks (* 1536 (GetPrevailingTimeSig #t)))
                                (d-DirectivePut-standalone-graphic MMRtag "BracketedWholeMeasureRest")
                                (d-DirectivePut-standalone-gx MMRtag 50)
                                (d-DirectivePut-standalone-gy MMRtag -2)
                                (d-DirectivePut-standalone-postfix MMRtag "%{ grouped rest %}")
                            (d-DirectivePut-standalone-override MMRtag DENEMO_OVERRIDE_LILYPOND)
                                (d-DirectivePut-standalone-display MMRtag (_ "Grouped in mm rest"))
                            (d-DirectivePut-standalone-tx MMRtag 25)
                                (d-DirectivePut-standalone-ty MMRtag -30)
                                (d-DirectivePut-standalone-minpixels MMRtag 100)
                                (loop))))))
            (d-PopPosition)
            (if (d-Directive-standalone? MMRtag)
            (begin
                    (d-SetDurationInTicks (* 1536 (GetPrevailingTimeSig #t)))
                 (d-DirectivePut-standalone-display MMRtag (string-append "Rest " (number->string count)))
                    (d-DirectivePut-standalone-ty MMRtag -28)  
                    (d-DirectivePut-standalone-postfix MMRtag (string-append "\\set Score.skipBars = ##t R1*" (d-InsertTimeSig "query=timesigname") "*" (number->string count)))))
           (d-SetSaved #f))
           
           
     ((d-Directive-standalone? MMRtag)
        (if (equal? (d-DirectiveGet-standalone-postfix MMRtag) "%{ grouped rest %}")
            (begin
                (d-InfoDialog (_ "This whole measure rest is grouped with adjacent ones to form a multi-measure rest.\nThe first of the group should be the multi-measure rest itself. Place the cursor on that object to do editing.") #f))
            (begin
                (if (not (equal? MultiMeasureRests::params "edit"))
                    (re-calculate)
                    (let ((choice (d-PopupMenu (list (cons (_ "Help") 'help) (cons (_ "Recalculate") 'recalculate) (cons (_ "Un-group") 'ungroup)))))
                    
                        (cond ((equal? choice 'help)
                                (d-InfoDialog (_ "This represents a number of whole measure rests.\nThe following measure rests are grouped with this one when typeset on their own. In full score they are typeset separately to match the other parts.\nIf you add further grouped whole measure rests, this Directive will need re-calculating.\nThe check score routine that is run before printing will do this for you.") #f))
                            ((equal? choice 'recalculate)
                                (re-calculate))
                            ((equal? choice 'ungroup)
                                (ungroup))))))))
        (else (d-InfoDialog (_ "Invoke this command with the cursor on the first whole measure rest to be grouped") #f))))
 

