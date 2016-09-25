#include <evince-view.h>
#include <errno.h>
#include <math.h>
#include <glib/gstdio.h>
#include "export/print.h"

static GtkWidget *DenemoMarkupArea;
static void
set_printarea_doc (EvDocument * doc)
{
  EvDocumentModel *model;
  model = g_object_get_data (G_OBJECT (DenemoMarkupArea), "model");     //there is no ev_view_get_model(), when there is use it
  if (model == NULL)
    {
      model = ev_document_model_new_with_document (doc);
      ev_view_set_model ((EvView *) DenemoMarkupArea, model);
      g_object_set_data (G_OBJECT (DenemoMarkupArea), "model", model);  //there is no ev_view_get_model(), when there is use it
    }
  else
    {
      g_object_unref (ev_document_model_get_document (model));  //FIXME check if this releases the file lock on windows.s
      ev_document_model_set_document (model, doc);
    }
}

static void
set_printarea (GError ** err)
{
  GFile *file;
  gchar *filename = Denemo.printstatus->printname_pdf[Denemo.printstatus->cycle];
  //g_debug("using %s\n", filename);
  if (Denemo.printstatus->invalid == 0)
    Denemo.printstatus->invalid = (g_file_test (filename, G_FILE_TEST_EXISTS)) ? 0 : 3;
  file = g_file_new_for_commandline_arg (filename);
  //g_free(filename);
  gchar *uri = g_file_get_uri (file);
  g_object_unref (file);
  EvDocument *doc = ev_document_factory_get_document (uri, err);
  //gint x = 0, y = 0, hupper, hlower, vupper, vlower;//store current position for reloading
  //get_window_position(&x, &y, &hupper, &hlower, &vupper, &vlower);
  if (*err)
    {
      g_warning ("Trying to read the pdf file %s gave an error: %s", uri, (*err)->message);
      Denemo.printstatus->invalid = 3;
      gtk_widget_queue_draw (DenemoMarkupArea);
    }
  else
    set_printarea_doc (doc);
  return;
}

void
markupview_finished (G_GNUC_UNUSED GPid pid, gint status, gboolean print)
{
#if GLIB_CHECK_VERSION(2,34,0)
  {
    GError *err = NULL;
    if (!g_spawn_check_exit_status (status, &err))
      g_warning ("Lilypond did not end successfully: %s", err->message);
  }
#endif
  g_spawn_close_pid (Denemo.printstatus->printpid);
  //g_debug("background %d\n", Denemo.printstatus->background);
  if (Denemo.printstatus->background == STATE_NONE)
    {
      process_lilypond_errors ((gchar *) get_printfile_pathbasename ());
    }
  else
    {
      if (LilyPond_stderr != -1)
        close (LilyPond_stderr);
      LilyPond_stderr = -1;
    }
  Denemo.printstatus->printpid = GPID_NONE;
  GError *err = NULL;
  set_printarea (&err);
}


void
install_markup_preview (GtkWidget * top_vbox, gchar *tooltip)
{
  GtkWidget *main_vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (top_vbox), main_vbox);
  ev_init ();
  DenemoMarkupArea = (GtkWidget *) ev_view_new ();
  GtkWidget *score_and_scroll_box = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (GTK_WIDGET (score_and_scroll_box), 600, 200);
  gtk_box_pack_start (GTK_BOX (main_vbox), score_and_scroll_box, TRUE, TRUE, 0);  
  gtk_container_add (GTK_CONTAINER (score_and_scroll_box), DenemoMarkupArea);
  gtk_widget_show_all (main_vbox);
}
