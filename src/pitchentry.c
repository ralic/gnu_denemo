/* pitchentry.c
 *  responses to pitchrecognition from audio in
 *
 * for Denemo, a gtk+ frontend to GNU Lilypond
 * (c)2007 Richard Shann
 */
#include <math.h>
#include "audio.h"
#include "pitchentry.h"
#include "view.h"


#define  DEFAULT_HIGH (4500.0)
#define  DEFAULT_LOW (60.0)
#define DEFAULT_TIMER_RATE (50)


static GtkWidget *PR_window = NULL;/* a top level window for controlling pitch-recognition entry.
			      We do not create one of these for each view (ie each DenemoGUI object, ie each score) because there is only one audio input source being used, so we would have to cope with resource contention issues, there is just no point. */
static DenemoGUI *PR_gui; /* the gui for which the pitch recognition has been set up */
static gboolean PR_insert; /* whether the notes should be inserted directly in the score or put in a list for controlled insertion */
static gboolean PR_continuous; /* whether to go on to the next measure once overlay for current measure is full */
gint PR_click;/*volume of a audible warning of next measure or extra tones in measure */
static gboolean PR_tuning; /* whether the notes should be analyzed to refine the frequency determination
			    for tuning instruments etc */
static GtkWidget *PR_notelabel = NULL;
static GtkWidget *PR_deviation = NULL;
static GtkWidget *PR_indicator = NULL;
static gdouble PR_target_pitch = 440.0;
static gdouble PR_accurate_pitch = 0.0;

static guint PR_timer;// timer id
static guint PR_enter;// signal id
static guint PR_leave;// signal id
//keymap *PR_oldkeymap;// the keymap of PR_gui when PR was started
//keymap *PR_rhythmkeymap;// special keymap for rhythm work.

static guint PR_time = DEFAULT_TIMER_RATE;//the period of timer for checking for a new pitch in ms 10 gives quicker response,100 more reliability... 
gboolean PR_enable = TRUE;
guint greatest_interval;
static gdouble lowest_pitch = DEFAULT_LOW;
static gdouble highest_pitch = DEFAULT_HIGH;
static gboolean repeated_notes_allowed;
static gdouble transposition_required = 1.0;
typedef struct notespec {
  guint step;  /* 0=C 1=D ... 6=B */
  gint alteration; /* -2 = bb, -1=b, 0=natural, 1=#, 2=x */
} notespec;

typedef struct notepitch {

  double pitch; /* pitch of note of given notespec in range C=260 to B=493 Hz
		 actual pitch is *=2^octave   */
  notespec spec;
} notepitch;

typedef struct temperament {
  gchar *name;
  gint sharp;/* which notepitch is the sharpest in circle of 5ths - initially G# */
  gint flat;/* which notepitch is the flattest in circle of 5ths - initially Eb */
  notepitch notepitches[12]; /* pitches of C-natural, C#, ... A#, B-natural */
} temperament;

#if 0
Pythagorean
261.6	279.4	294.3 	310.1	331.1	348.8 	372.5	392.4	419.1 	441.5	465.1	496.7
Van Zwolle
261.6	279.4	294.3 	314.3	331.1	353.6 	372.5	392.4	419.1 	441.5	471.5	496.7
Meantone
261.6	272.8	292.3 	313.2	326.7	350.0 	365.0	391.1	407.9 	437.0	468.3	488.3
Silbermann I
261.6	275.0	293.0 	312.2	328.1	349.6 	367.5	391.6	411.6 	438.5	467.2	491.1
Silbermann II
261.6	276.2	293.0 	312.2	329.6	349.6 	369.2	391.6	413.4 	438.5	467.2	493.3
Rameau
261.6	276.9	293.3 	309.7	330.4	348.8 	369.2	391.1	413.9 	438.5	463.0	493.9
Werckmeister III
261.6	277.5	293.3 	311.1	328.9	350.0 	370.0	391.1	414.8 	438.5	466.7	493.3
Werckmeister IV
261.6	275.6	293.0 	311.5	329.6	348.8 	369.2	390.7	413.4 	437.5	467.2	492.2
Werckmeister V
261.6	275.6	293.3 	311.1	328.9	348.8 	368.7	392.4	413.4 	438.5	465.1	493.3
Werckmeister VI
261.6	278.3	293.8 	311.3	331.1	349.5 	372.5	391.7	414.2 	441.5	466.9	496.7
Kirnberger II
261.6	277.5	294.3 	312.2	328.9	348.8 	370.0	392.4	416.2 	438.5	465.1	493.3
Kirnberger III
261.6	277.5	293.3 	311.1	328.9	348.8 	370.0	391.1	414.8 	438.5	465.1	493.3
Italian 18th Century
261.6	277.2	293.0 	311.1	328.9	349.2 	370.0	391.1	414.4 	438.0	465.1	492.8
Equal Temperament
261.6	277.2	293.7 	311.1	329.6	349.2 	370.0	392.0	415.3 	440.0	466.2	493.9
HARPSICHORD TUNING - A COURSE OUTLINE, by G.C. Klop, distributed by The Sunbury Press, P.O. Box 1778, Raleigh, NC 27602.
#endif
static temperament Equal = {
  "Equal", 8,3,
    { 
      {261.6, {0, 0}},
      {277.2, {0, 1}},
      {293.7, {1, 0}},
      {311.1, {2, -1}},
      {329.6, {2, 0}},
      {349.2, {3, 0}},
      {370.0, {3, 1}},
      {392.0, {4, 0}},
      {415.3, {4, 1}},
      {440.0, {5, 0}},
      {466.2, {6, -1}},
      {493.9, {6, 0}}
  }
};

static temperament Lehman = {
  "Lehman", 8,3,
    { 
      {262.37*1     , {0, 0}},
      {262.37*1.0590, {0, 1}},
      {262.37*1.1203, {1, 0}},
      {262.37*1.1889, {2, -1}},
      {262.37*1.2551, {2, 0}},
      {262.37*1.3360, {3, 0}},
      {262.37*1.4120, {3, 1}},
      {262.37*1.4968, {4, 0}},
      {262.37*1.5869, {4, 1}},
      {262.37*1.6770, {5, 0}},
      {262.37*1.7816, {6, -1}},
      {262.37*1.8827, {6, 0}}
  }
};

		
 	 	
	 	
 	 	
	 	
	 	
 	 	
	 	
 	 	
	 	
 	 	
	 	

static temperament Meantone = {
  "Quarter comma meantone", 8,3,
    { 
      {261.6, {0, 0}},
      {272.8, {0, 1}},
      {292.3, {1, 0}},
      {313.2, {2, -1}},
      {326.7, {2, 0}},
      {350.0, {3, 0}},
      {365.0, {3, 1}},
      {391.1, {4, 0}},
      {407.9, {4, 1}},
      {437.0, {5, 0}},
      {468.3, {6, -1}},
      {488.3, {6, 0}}
  }
};


static temperament WerckmeisterIV = {
  "Werckmeister IV", 8,3,
    { 
      {263.11, {0, 0}}, /* c */
      {275.93, {0, 1}},
      {294.66, {1, 0}}, /* d */
      {311.83, {2, -1}},/* Eb */
      {330.00, {2, 0}}, /* e */
      {350.81, {3, 0}}, /* f */
      {369.58, {3, 1}},
      {392.88, {4, 0}}, /* g */
      {413.90, {4, 1}},/* g# */
      {440.00, {5, 0}}, /* a */
      {469.86, {6, -1}}, /* Bb */
      {492.77, {6, 0}}/* b */
  }
};

static temperament *PR_temperament; /* the currently used temperament */

static temperament *temperaments[] = {&Equal, &Meantone, &WerckmeisterIV, &Lehman};

static void switch_back_to_main_window(void) {

    gtk_window_present(GTK_WINDOW(Denemo.window));
}


static void pr_display_note(DenemoGUI*gui, gchar *notename) {
  gchar *labelstr = g_strdup_printf("<span foreground=\"black\" font_desc=\"48\">%s</span>",notename);
  //printf("string is %s\n", labelstr);
  gtk_label_set_markup(GTK_LABEL(PR_notelabel), labelstr);
  g_free(labelstr);
}

static void pr_display_pitch_deviation(DenemoGUI*gui, double deviation) {
  gchar *labelstr = g_strdup_printf("<span foreground=\"%s\" font_desc=\"48\">%2.1f</span>",deviation>0.0?"blue":deviation<0.0?"red":"black", deviation);
  gtk_label_set_markup(GTK_LABEL(PR_deviation),labelstr );
  g_free(labelstr);
}



/* return c,d,e,f,g,a,b depending on the step. German translation will
   be difficult I'm afraid. */
static gchar step_name(guint step) {
  if(step>4)
    step -= 7;
  return 'C'+step;


}

/* return "##", "#" ..."  "..... "bb" for double sharp to double flat accidentals
 a const string is returned*/
static gchar *alteration_name(gint alteration) {
  switch(alteration) {
  case -2:
    return "bb";
  case -1:
    return "b";
  case 0:
    return "";
  case 1:
    return "#";

  case 2:
    return "##";
  default:
    return "ER";
  }
}




/* returns the note names currently set for the given temperament
 caller must free the string */
static gchar * notenames(gpointer p) {
  temperament *t=(temperament*)p;
  gchar *str, *oldstr;
  gint i;
  oldstr = g_strdup("");
  for(i=0;i<12;i++) {
    if(i==t->flat)
      str = g_strdup_printf("%s<span size=\"large\" foreground=\"red\">%c%s</span> ", oldstr, step_name(t->notepitches[i].spec.step), alteration_name(t->notepitches[i].spec.alteration));
    else
    if(i==t->sharp)
       str = g_strdup_printf("%s<span size=\"large\" foreground=\"blue\">%c%s</span> ", oldstr, step_name(t->notepitches[i].spec.step), alteration_name(t->notepitches[i].spec.alteration));
    else
      str = g_strdup_printf("%s%c%s ", oldstr, step_name(t->notepitches[i].spec.step), alteration_name(t->notepitches[i].spec.alteration));
    g_free(oldstr);
    oldstr = str;
  }
  return str;
}


/* returns an opaque id for the user's default temperament
 FIXME user prefs */
static gpointer default_temperament() {
  gint i;
  for(i=0;i<G_N_ELEMENTS(temperaments);i++) {
    if(!strcmp(Denemo.prefs.temperament->str, temperaments[i]->name))
        return (gpointer) temperaments[i];
  }
  return (gpointer) &Equal;
}

static void set_temperament(GtkMenuItem *item, temperament *t) {
  PR_temperament = t;
  g_string_assign(Denemo.prefs.temperament, t->name);
  //g_print("Temperament set to %p (cf %p %p)\n", PR_temperament, &Equal, &Meantone);
}

static void sharpen(GtkButton *button, GtkWidget *label) {
#define f  (PR_temperament->notepitches[PR_temperament->flat].spec)
  gint next = (PR_temperament->flat+11)%12;
#define g (PR_temperament->notepitches[next].spec)
 if(g.alteration+1>2) 
  return;
 else  {
     f.step = g.step;

      f.alteration =   g.alteration+1;

  }
#undef f
#undef g
  PR_temperament->sharp = PR_temperament->flat;
  PR_temperament->flat = (PR_temperament->flat+7)%12;
  gchar *names = notenames(PR_temperament);
  gtk_label_set_markup((GtkLabel*)label, names);
  g_free(names);
  switch_back_to_main_window();
  return;
}




static void flatten(GtkButton *button, GtkWidget *label) {

#define s (PR_temperament->notepitches[PR_temperament->sharp].spec)
  gint next = (PR_temperament->sharp+1)%12;
#define t (PR_temperament->notepitches[next].spec)
if(t.alteration-1<-2) 
  return;
 else {
   s.step = t.step;
   s.alteration =  t.alteration-1;  
 }
#undef s
#undef t
  PR_temperament->flat = PR_temperament->sharp;
  PR_temperament->sharp = (PR_temperament->sharp+5)%12;
  gchar *names = notenames(PR_temperament);
  gtk_label_set_markup((GtkLabel*)label, names);
  g_free(names);
  switch_back_to_main_window();
  return;
}


static void sound_click(void) {
  if(PR_click)
    gdk_beep();
}

static GList *get_tones(GList *tone_store, gint measurenum) {
  GList *g = g_list_nth(tone_store, measurenum);
  if(g)
    return g->data;
  return NULL;
}

/* apply the tones in the currentmeasure to the notes of the currentmeasure
* return TRUE if measure has enough tones for all its notes
*/

static gboolean  apply_tones(DenemoScore *si) { 
  gboolean ret=FALSE;
  DenemoStaff* curstaff = ((DenemoStaff*)si->currentstaff->data);
  GList *store;
  gint measurenum;
  store  = (curstaff->tone_store);
  measurenode *curmeasure = curstaff->measures;
  GList *store_el = NULL;
  // move cursor to start of current measure
  si->currentobject = (objnode *) si->currentmeasure->data;
  si->cursor_x = 0;
  si->cursor_appending = !(gboolean)si->currentobject;
  //calcmarkboundaries (si);
  measurenum =  si->currentmeasurenum - 1;
  curmeasure = si->currentmeasure;
  if(curmeasure) {
    store_el = get_tones(store, measurenum);
    objnode *curobj = curmeasure->data;
    while (curobj)
      {
	tone* thetone;
	//skip over invalid tones
	while(store_el && (thetone = (tone*)store_el->data) &&
	      !thetone->valid)
	  store_el = store_el->next;
	gboolean tone_stored = FALSE;
	DenemoObject *theobj = (DenemoObject *) curobj->data;
	if(theobj->type == CHORD && ((chord*)theobj->object)->notes /* not a rest */) {
	  if(thetone==NULL || store_el==NULL)
	    ((chord*)theobj->object)->tone_node = NULL;
	  else {
	    gint dclef  = find_prevailing_clef(si);
	    gint mid_c_offset = thetone->step;
	    ((chord*)theobj->object)->tone_node = store_el;
	    modify_note((chord*)theobj->object, mid_c_offset, thetone->enshift, dclef);
	    tone_stored=TRUE;
	    if(!si->cursor_appending)
	      cursorright(PR_gui);
	    store_el = store_el->next;

	  }// tone available
	}// note available
	/* skip over non notes */
	do {
	  curobj = curobj->next;
	  if(curobj)
	    theobj = (DenemoObject *) curobj->data;
	} while(curobj && 
		(theobj->type != CHORD || ((chord*)theobj->object)->notes==NULL));
	if(tone_stored && curobj==NULL && curmeasure->next)
	 ret = TRUE;
      }// while objects in measure
    if(store_el && !PR_continuous) 
      sound_click();//extra tones in measure
    showwhichaccidentals ((objnode *) si->currentmeasure->data,
			  si->curmeasurekey, si->curmeasureaccs);
  }
  return ret;
}


/*
 * enter_note_in_score
 * enters the note FOUND in the score gui->si at octave OCTAVE steps above/below mid-c
 */
static void enter_note_in_score (DenemoGUI *gui, notepitch * found, gint octave) {
  //printf("Cursor_y %d and staffletter = %d\n", gui->si->cursor_y, gui->si->staffletter_y);
  gui->si->cursor_y = gui->si->staffletter_y = found->spec.step;
  gui->si->cursor_y += 7*octave;
  shiftcursor(gui, found->spec.step);
  setenshift(gui->si, found->spec.alteration);
  displayhelper (gui);
}





static GList * put_tone(GList *store, gint measurenum, tone *thetone) {
  // extend store if too small
  gint i = measurenum + 1 - g_list_length(store);
  for(;i>0;i--) {
    store = g_list_append(store,NULL);
  }
  GList *g = g_list_nth(store, measurenum);
  if(g)
    g->data = g_list_append(g->data, thetone);
  return store;
}


/*
 * enter_tone_in_store
 * enters the note FOUND as a tone in the tone store
 */
static void enter_tone_in_store (DenemoGUI *gui, notepitch * found, gint octave) {
  gboolean nextmeasure;
  tone *thetone = (tone*)g_malloc0(sizeof(tone));
  //g_print("tone %p\n", thetone);
  thetone->enshift = found->spec.alteration;
  thetone->step = found->spec.step + 7*octave;
  thetone->octave = octave;
  thetone->valid = TRUE;
#define store  (((DenemoStaff*)gui->si->currentstaff->data)->tone_store)
  store = put_tone(store, gui->si->currentmeasurenum - 1, thetone);
  nextmeasure = apply_tones(gui->si);
  displayhelper (gui);
  if(PR_continuous && nextmeasure) {
    sound_click();
    measureright(gui);
  }  
#undef store
}

/*
 * clear the references to tones (ie any overlay) in the currentstaff
 */
static void clear_tone_nodes(DenemoGUI *gui ) {
  DenemoScore *si = gui->si;
  DenemoStaff* curstaff = ((DenemoStaff*)si->currentstaff->data);
  measurenode *curmeasure;
  for (curmeasure = curstaff->measures;curmeasure; curmeasure = curmeasure->next) {
    objnode *curobj = curmeasure->data;
    for (; curobj; curobj = curobj->next)
      {
	DenemoObject *theobj = (DenemoObject *) curobj->data;
	if(theobj->type == CHORD) {
	   ((chord*)theobj->object)->tone_node = NULL;
	}
      }
  }
}

static void free_tones(GList *tones) {
  if(tones){
    g_list_foreach(tones,freeit,NULL);
    g_list_free(tones);
  }
}

  // clear gui->si->currentstaff->data->tone_store and the references to it
static void clear_tone_store(GtkButton *button, DenemoGUI *gui) {
#define store  (((DenemoStaff*)gui->si->currentstaff->data)->tone_store)
  g_list_foreach (store, (GFunc)free_tones, NULL);
  clear_tone_nodes(gui);
  g_list_free(store);
  store = NULL;
#undef store
  displayhelper(gui);
  if(PR_gui)
    switch_back_to_main_window();
}
void clear_overlay(GtkAction *action, gpointer param) {
  DenemoGUI *gui = Denemo.gui;
  clear_tone_store(NULL, gui);
}

/*
 * clear the references to tones (ie any overlay) in the currentmeasure
 */
static void clear_tone_nodes_currentmeasure(DenemoScore *si ) {
  measurenode *curmeasure = si->currentmeasure;   
  objnode *curobj = curmeasure->data;
  for (; curobj; curobj = curobj->next)
    {
      DenemoObject *theobj = (DenemoObject *) curobj->data;
      if(theobj->type == CHORD) {
	((chord*)theobj->object)->tone_node = NULL;
      }
    } 
}



gboolean delete_tone(DenemoScore *si, chord *thechord) {
  GList *tone_node = thechord->tone_node;
  if(tone_node) {
    ((tone*)tone_node->data)->valid = FALSE;
  objnode *keep = si->currentobject;
  gint keepx = si->cursor_x;
  gboolean keepa = si->cursor_appending;
  apply_tones(si);
  /*restore the position of the cursor before the delete of the tone */
  si->currentobject = keep;
  si->cursor_x = keepx;
  si->cursor_appending = keepa;
  calcmarkboundaries (si);

  displayhelper (PR_gui);
  return TRUE;
  } else {
    return FALSE;
  }

}

/* return note for the passed pitch, or NULL if not a good note;
 * @param pitch, the pitch being enquired about
 * @temperament, the temperament to use to determine which pitch.
 * @param which_octave returns the number of the octave above/below mid_c
FIXME there is a bug when the enharmonic changes take b# into the wrong octave
*/
static notepitch * determine_note(gdouble pitch, temperament *t, gint* which_octave, double *deviation) {
  gint i;
  gint octave = 0;
  while(pitch > t->notepitches[11].pitch*(1.0231)/* quartertone */) {
    //printf("pitch going down %f pitch %f\n", pitch, t->notepitches[11].pitch*(1.0231) );
    pitch /= 2;
    octave++;
  }
  while(pitch < t->notepitches[0].pitch*(0.977)/* quartertone */) {
    //printf("pitch going up %f pitch\n", pitch);
    pitch *=2;
    octave--;
  }
  for(i=0;i<12;i++) {
    //printf("considering %d %f\n", pitch, t->notepitches[i].pitch);
    if((pitch > t->notepitches[i].pitch*(0.977)) &&
       (pitch <= t->notepitches[i].pitch*(1.0231))) {
      *which_octave = octave;
      //printf("found %d octave \n", octave);
      *deviation = (pitch - t->notepitches[i].pitch)/( t->notepitches[i].pitch*(pow(2,1.0/12.0)-1.0)/100.0) ;
      return &t->notepitches[i];
    }
  }

  return NULL;
}

static void display_pitch(double note, DenemoGUI *gui) {
  gint octave;
  double deviation;
  temperament *t = (temperament*)PR_temperament;
    notepitch *found = determine_note(note, t , &octave, &deviation);
    if(found) {
      int i;
      gchar *octstr=g_strdup("");
      for(i=0;i<octave+1;i++) {
	gchar *str = g_strdup_printf("%s%c",octstr,'\'');
	g_free(octstr);
	octstr=str;
      }
      for(i=0;i>octave+1;i--) {
	gchar *str = g_strdup_printf("%s%c",octstr,',');
	g_free(octstr);
	octstr=str;
      }
      pr_display_note(gui, g_strdup_printf("%c%s%s",step_name(found->spec.step), alteration_name(found->spec.alteration), octstr));
      g_free(octstr);
      pr_display_pitch_deviation(gui, deviation);
      // FIXME if tuning display a graphic for how far note is from target_note
      if(PR_tuning) {
	PR_accurate_pitch = note;
	gtk_widget_draw(PR_indicator, NULL);


      }
    }
/*     fprintf(stderr, "Pitch is %d %d\t", (int)(Freq2Pitch(note)+0.5), (int)note); */
  }

static gint measure_pitch_accurately (DenemoGUI *gui) {
  double note = determine_frequency();
  //g_print("returned %f\n", note);
  if(note>0.0) display_pitch(note, gui);

  return TRUE;
}
/* look for a new note played into audio input, if
   present insert it into the score/store */
gint pitchentry(DenemoGUI *gui) {
  static gint last_step=-1, last_alteration, last_octave;
  if(PR_window==NULL)
    return FALSE;/* stops the timer */
  double deviation;
  temperament *t = (temperament*)PR_temperament;
  gint octave;
  gdouble note;
  note = get_pitch();
  note *= transposition_required;
  if((note<highest_pitch) && (note>lowest_pitch))
   {
     //printf("Got a note %2.1f\n", note);
    notepitch *found = determine_note(note, t , &octave, &deviation);
    if(found) {
      /* if tuning */
      if(PR_tuning) {
	setTuningTarget(note); 
	PR_target_pitch = found->pitch * (pow(2,(octave)));
	}
      if(!repeated_notes_allowed)
	if(found->spec.step==last_step &&
	   found->spec.alteration==last_alteration &&
	   (octave==last_octave || octave==last_octave-1)/* sometimes it jumps down an octave */) {
	  last_step=-1;
	  printf("Ignoring repeated note\n");
	  return TRUE;
	}
      
      last_step = found->spec.step;
      last_alteration = found->spec.alteration;
      last_octave = octave;

      if(!PR_enable) {
	//printf("Enter the score area to insert notes!");
	return TRUE;
      }
      
      // Enter the note in the score
      if(!PR_tuning){
	display_pitch(note, gui);
	if(PR_insert)
	  enter_note_in_score(gui, found, octave);
	else
	  enter_tone_in_store(gui, found, octave);
      }

    }//note found
   } // acceptable range
  return TRUE;
}




  // toggle PR_continuous advance to next measure or not
static void toggle_continuous(GtkButton *button, DenemoGUI *gui ) {
  PR_continuous = !PR_continuous;
  switch_back_to_main_window();
}

static void change_silence(GtkSpinButton *widget, gpointer data){
  double silence = gtk_spin_button_get_value(widget);
  set_silence(silence);
  switch_back_to_main_window();
}
static void change_threshold(GtkSpinButton *widget, gpointer data){
  double t = gtk_spin_button_get_value(widget);
  set_threshold(t);
  switch_back_to_main_window();
}
static void change_smoothing(GtkSpinButton *widget, gpointer data){
  double m = gtk_spin_button_get_value(widget);
  set_smoothing(m);
  switch_back_to_main_window();
}

static void change_onset_detection(GtkSpinButton *widget, gpointer data){
  guint m = gtk_spin_button_get_value(widget);
  set_onset_type(m);
  switch_back_to_main_window();
}


static void change_lowest_pitch(GtkSpinButton *widget, gpointer data){
  lowest_pitch  = gtk_spin_button_get_value(widget);
  switch_back_to_main_window();
}
static void change_highest_pitch(GtkSpinButton *widget, gpointer data){
  highest_pitch  = gtk_spin_button_get_value(widget);
  switch_back_to_main_window();
}

static void change_greatest_interval(GtkSpinButton *widget, gpointer data){
  greatest_interval  = gtk_spin_button_get_value_as_int(widget);
  switch_back_to_main_window();
}

static void change_transposition(GtkSpinButton *widget, gpointer data){
  gdouble power  = gtk_spin_button_get_value_as_int(widget);
  transposition_required = pow(2.0, power);
  switch_back_to_main_window();
//printf("transposing = %f 2^1/12=%f\n", transposition_required, pow(2,1.0/12.0));
}


static void frequency_smoothing(GtkSpinButton *widget, gpointer data){
  double m = gtk_spin_button_get_value(widget);
  set_frequency_smoothing(m);
}

/* destroy PR_window, and hence terminate the pitch recog subsystem */
static void stop_PR_controls(void) {
  if(PR_window)
    gtk_widget_destroy(PR_window);
  PR_window = NULL;
}


/* stop_pitch_input
   if not midi stop audio and aubio
 */
int stop_pitch_input(void) {
  DenemoGUI *gui = Denemo.gui;
  if(PR_timer)
    g_source_remove(PR_timer);

  if(PR_enter) 
    g_signal_handler_disconnect (PR_gui->scorearea, PR_enter);
  if(PR_leave) 
    g_signal_handler_disconnect (PR_gui->scorearea, PR_leave);
   PR_timer = PR_enter = PR_leave = 0;

   if(gui->input_source==INPUTAUDIO)
      terminate_pitch_recognition();
   else
     jackstop();
   if(PR_window) { 
     GtkWidget *temp = PR_window; PR_window = NULL, gtk_widget_destroy(temp);
   }
  PR_gui = NULL;

  return 0;
}


static stop_tuning_callback(){
  PR_tuning = FALSE;
  collect_data_for_tuning(PR_tuning);
  PR_indicator = NULL;
}




static void toggle_repeated_notes_allowed(){
  repeated_notes_allowed = !repeated_notes_allowed;
switch_back_to_main_window();
}



//eventually make this value control the loudness of a click
static void change_click_volume(GtkSpinButton *widget){
  PR_click = (guint)gtk_spin_button_get_value(widget);
  switch_back_to_main_window();
}


static void change_timer_rate(GtkSpinButton *widget, DenemoGUI *gui){
  PR_time = (guint)gtk_spin_button_get_value(widget);
  start_pitch_input();//FIXME do not call the whole of start_pitch_recognition, just the timer setting bit???
  switch_back_to_main_window();
}

  // toggle PR_insert to show where the notes detected should go, and start correct entry mode
static void toggle_insert(GtkButton *button, DenemoGUI *gui) {
  GtkAction *action;
  action = gtk_ui_manager_get_action (Denemo.ui_manager, PR_insert?"/MainMenu/EntryMenu/EditMode": "/MainMenu/EntryMenu/InsertMode");
  gtk_action_activate(action);
  PR_insert = !PR_insert;
  switch_back_to_main_window();
}

static gint draw_indicator (GtkWidget * widget, GdkEventExpose * event, gpointer data){
  int barwidth = 20;
  int centre = 400;
  double cent = log10f(PR_accurate_pitch / PR_target_pitch)*1200/log10f(2);
    int iCent=lround((2*cent+100)*4);// causes warning if not stdc=-c99, whose problem is this?
    //value 400 is perfect, 410 = 5/2 cents sharp
    if (iCent < 0) iCent= 0;
    if (iCent >  800) iCent=  800;
    GdkGC *gc;
    if(iCent<380)
      gc =gcs_redgc(); 
    else
      if(iCent>420)
	gc = gcs_bluegc(); //blue
      else
	gc =  gcs_greengc(); //green 	
    gdk_draw_rectangle (widget->window,
			gc, TRUE, iCent-barwidth/2,0,barwidth,320);
	
    gdk_draw_rectangle (widget->window,
			gcs_blackgc(), TRUE,centre-barwidth/8, 0, barwidth/4, 320);
  return TRUE;
}

static void toggle_tuning(GtkToggleButton *button, DenemoGUI *gui) {
  static int id;
  static GtkWidget *widget;
    PR_tuning = gtk_toggle_button_get_active (button);
    collect_data_for_tuning(PR_tuning);
    if(PR_tuning) {
      
      if(PR_indicator==NULL) {
	widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (widget), "Tuning Indicator");
      gtk_window_set_default_size (GTK_WINDOW (widget), 800, 100);
      g_signal_connect (G_OBJECT (widget), "destroy",
			G_CALLBACK (stop_tuning_callback), NULL); 
      GtkWidget *hbox = gtk_hbox_new (FALSE, 1);
      gtk_container_add (GTK_CONTAINER (widget), hbox);
      PR_indicator = gtk_drawing_area_new ();
      gtk_box_pack_start (GTK_BOX (hbox), PR_indicator, TRUE, TRUE, 0); 
      gtk_signal_connect (GTK_OBJECT (PR_indicator), "expose_event",
			  GTK_SIGNAL_FUNC (draw_indicator), gui);
      gtk_widget_show_all(widget);
      }
      gtk_window_present(GTK_WINDOW(widget));
      id = g_timeout_add (200, (GSourceFunc)measure_pitch_accurately, gui);
    }
  else
    if(id) {
      if(PR_indicator) gtk_widget_hide(widget);
      g_source_remove(id);
      id = 0;
    }
}
#define COLUMN_NAME (0)
#define COLUMN_PTR (1)

static void  temperament_changed_callback (GtkComboBox *combobox,  GtkListStore *list_store) {
  GtkTreeIter iter;
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter);
  gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
		      COLUMN_PTR, &PR_temperament, -1);
  g_string_assign(Denemo.prefs.temperament, PR_temperament->name);
}

static void create_pitch_recognition_window(DenemoGUI *gui) {
  GtkWidget *hbox, *hbox2;
  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *label;
  if(PR_window) {
    g_warning("unexpected call");
    return;
  }
  PR_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (PR_window), "Pitch Recognition Control");

  GtkWidget *main_vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_border_width (GTK_CONTAINER (main_vbox), 1);
  gtk_container_add (GTK_CONTAINER (PR_window), main_vbox);

  frame = gtk_frame_new( "Mode");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  GtkWidget *vbox = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), vbox,
		      TRUE, TRUE, 0);
  GtkWidget *radio_button = gtk_radio_button_new_with_label(NULL, "Overlay Pitches");

  g_signal_connect (G_OBJECT (radio_button), "toggled",
		    G_CALLBACK (toggle_insert), gui);
  button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_button), "Insert Notes");

  PR_insert = TRUE;
  toggle_insert(NULL, gui);
  gtk_box_pack_start (GTK_BOX (vbox), button,
		      TRUE, TRUE, 0);/* no need for callback */

  gtk_box_pack_start (GTK_BOX (vbox), radio_button,
		      TRUE, TRUE, 0);

  button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_button), "Tuning");
  g_signal_connect (G_OBJECT (button), "toggled",
		    G_CALLBACK (toggle_tuning), gui);
  gtk_box_pack_start (GTK_BOX (vbox), button,
		      TRUE, TRUE, 0);

  frame = gtk_frame_new( "Overlay Pitches");
  gtk_container_add (GTK_CONTAINER (hbox), frame);

  GtkWidget *vbox2 = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  hbox2 = gtk_hbox_new (FALSE, 1);

  gtk_box_pack_start (GTK_BOX (vbox2), hbox2,
		      TRUE, TRUE, 0);
  
  

  button = gtk_button_new_with_label("Clear Overlay"); //FIXME make this a proxy for the ClearOverlay action ??
  gtk_box_pack_start (GTK_BOX (hbox2), button,
		      TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (clear_tone_store), gui);
  hbox2 = gtk_hbox_new (FALSE, 1);

  gtk_box_pack_start (GTK_BOX (vbox2), hbox2,
		      TRUE, TRUE, 0);
  button = gtk_check_button_new_with_label("Continuous");
  gtk_box_pack_start (GTK_BOX (hbox2), button,
		      TRUE, TRUE, 0);
  PR_continuous = FALSE;
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (toggle_continuous), gui);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

  label = gtk_label_new("Click Volume");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  PR_click = 1;
  GtkAdjustment *spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new ((double)PR_click, 0.0, 1.0,
					   1.0, 1.0, 1.0);
  GtkWidget *spinner = gtk_spin_button_new (spinner_adj, 1.0, 1);
  gtk_box_pack_start (GTK_BOX (hbox2), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_click_volume), NULL);




  frame = gtk_frame_new( "Enharmonic selection");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  label = gtk_label_new("");
  button = gtk_button_new_with_label("flatten");
  gtk_box_pack_start (GTK_BOX (hbox), button,
		      TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (flatten), label);
  gchar *names = notenames(PR_temperament);
  
  gtk_label_set_markup(GTK_LABEL(label),names);
  g_free(names);
  gtk_box_pack_start (GTK_BOX (hbox), label,
		      TRUE, TRUE, 0);
  
  button = gtk_button_new_with_label("sharpen");
  gtk_box_pack_start (GTK_BOX (hbox), button,
		      TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (sharpen), label);

  frame = gtk_frame_new( "Detected note");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  PR_notelabel = gtk_label_new("---");
  gtk_box_pack_start (GTK_BOX (hbox), PR_notelabel,
		      TRUE, TRUE, 0); 
  PR_deviation = gtk_label_new("0.0");
  gtk_box_pack_start (GTK_BOX (hbox), PR_deviation,
		      TRUE, TRUE, 0);
  /* spinners to select silence, threshold, smoothing */ 
  
  frame = gtk_frame_new( "Pitch Recognition Parameters");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  hbox2 =  gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  
  label = gtk_label_new("Silence");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (-90.0, -1000.0,
					  100.0, 10.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 100.0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_silence), NULL);

  hbox2 =  gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  
  label = gtk_label_new("Threshold");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (1.0, 0.01,
					  100.0, 0.1, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 0.5, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_threshold), NULL);

  hbox2 =  gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  
  label = gtk_label_new("Smoothing");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (6.0, 0.0,
					  100.0, 1.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 0.5, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_smoothing), NULL);

  label = gtk_label_new("Onset");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (7.0, 0.0,
					  7.0, 1.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 0.5, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_onset_detection), NULL);



  /* spinners to constrain the note values */


  frame = gtk_frame_new( "Note validation criteria");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  hbox2 =  gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  
  label = gtk_label_new("Lowest Pitch");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (DEFAULT_LOW, 60.0, 2080.0,
					   10.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 100.0, 1);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_lowest_pitch), NULL);

  hbox2 =  gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  
  label = gtk_label_new("Highest Pitch");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (DEFAULT_HIGH, 120.0, 2400.0,
					   0.01, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 100.0, 1);
  gtk_box_pack_start (GTK_BOX (hbox2), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_highest_pitch), NULL);

  hbox2 =  gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  
  label = gtk_label_new("Greatest Interval");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (8.0, 1.0,
					  15.0, 1.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 1.0, 0);
  gtk_box_pack_start (GTK_BOX (hbox2), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_greatest_interval), NULL);

  /* options */

 frame = gtk_frame_new( "Input handling");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);


  
  label = gtk_check_button_new_with_label("Disable repeated notes");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (label), "clicked",
		    G_CALLBACK (toggle_repeated_notes_allowed), NULL);
  label = gtk_label_new("Transpose Input");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (0.0, -3.0,
					  3.0, 1.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 1.0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_transposition), NULL);

  label = gtk_label_new("Delay");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (DEFAULT_TIMER_RATE, 1.0,
					  500.0, 10.0, 1.0, 1.0);
  spinner = gtk_spin_button_new (spinner_adj, 10.0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (change_timer_rate), gui);

  frame = gtk_frame_new( "Frequency Measurement");
  gtk_container_add (GTK_CONTAINER (main_vbox), frame);
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new("Frequency smoothing");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  spinner_adj =
    (GtkAdjustment *) gtk_adjustment_new (0.25, 0.1,
					  1.0, 0.05, 0.05, 0.05);
  spinner = gtk_spin_button_new (spinner_adj, 0.5, 2);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (spinner), "value-changed",
		    G_CALLBACK (frequency_smoothing), NULL);
 

  GtkListStore *list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  GtkCellRenderer *renderer;
  GtkWidget *combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list_store));
  int i;
  for (i = 0; i < (gint) G_N_ELEMENTS (temperaments); i++)
    {GtkTreeIter iter;
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter,
			  COLUMN_NAME, temperaments[i]->name,
			  COLUMN_PTR, temperaments[i], -1);

      if(!strcmp(Denemo.prefs.temperament->str, temperaments[i]->name))
	 gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter); 
    }
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox),
				 renderer, "text", COLUMN_NAME);
  g_signal_connect(G_OBJECT (combobox),  "changed",
		    G_CALLBACK (temperament_changed_callback), list_store);
  gtk_box_pack_start (GTK_BOX (hbox ),
		      combobox, TRUE, TRUE, 0);



/* now show the window, but leave the main window with the focus */
  gtk_window_set_focus_on_map((GtkWindow *)PR_window, FALSE);
  gtk_widget_show_all(PR_window);
  gtk_window_set_focus((GtkWindow*)PR_window, NULL);
  gtk_widget_grab_focus(gui->scorearea);

  // FIXME make the visibility of the Frequency Measurement frame depend of the tuning toggle
  // sort out the size of the drawing area
  // sort out how to pass the deviation from in-tune to the draw_indicator function
  
}


gint setup_pitch_input(void){
  DenemoGUI *gui = Denemo.gui;
  if(PR_window) {
    gtk_window_present(GTK_WINDOW(PR_window));
    return 0;
  }
  if(PR_temperament==NULL)
    PR_temperament = default_temperament();
  if(gui->input_source==INPUTAUDIO?(initialize_pitch_recognition()==0):(jackmidi()==0)) {
    set_silence(-90.0);
    set_threshold(0.3);
    set_smoothing(6.0);
    transposition_required = 1.0;
    lowest_pitch = DEFAULT_LOW;
    highest_pitch = DEFAULT_HIGH;
    repeated_notes_allowed = TRUE;
    create_pitch_recognition_window(gui);
    //read_PRkeymap(gui);
    return 0;
  } else
    return -1;
}



static void 
scorearea_set_active(GtkWidget *widget, GdkEventCrossing *event, DenemoGUI *gui) {
  PR_enable = TRUE; 
  gtk_widget_draw(gui->scorearea, NULL);
}
static void 
scorearea_set_inactive(GtkWidget *widget, GdkEventCrossing *event, DenemoGUI *gui) {
  PR_enable = FALSE; 
  gtk_widget_draw(gui->scorearea, NULL);
}
void start_pitch_input(void) { 
  DenemoGUI *gui = Denemo.gui;
  if(PR_timer)
    g_source_remove(PR_timer);

  PR_timer = g_timeout_add (PR_time, (GSourceFunc)pitchentry, Denemo.gui);

  if(PR_timer==0)
    g_error("Timer id 0 - if valid the code needs re-writing (documentation not clear)");
  gtk_widget_add_events (gui->scorearea, GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK);
  PR_enter = g_signal_connect (G_OBJECT (gui->scorearea), "enter-notify-event",
			       G_CALLBACK (scorearea_set_active), (gpointer)gui);
  PR_leave = g_signal_connect (G_OBJECT (gui->scorearea), "leave-notify-event",
			       G_CALLBACK (scorearea_set_inactive), (gpointer)gui);
  PR_gui = gui;
}

gboolean pitch_recognition_system_active(void) {
  return PR_window!=NULL;
}

gboolean pitch_entry_active(DenemoGUI *gui) {
  return (PR_gui == gui) && PR_enable;
}

