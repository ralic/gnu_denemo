#include "keyboard.h"
#include "kbd-custom.h"
#include "view.h"
#include <string.h>



/*
 * translate a keybinding from the format used in denemo keymaprc file to the
 * format understood by gtk_accelerator_parse. The output is an allocated string
 * that must be freed by the caller.
 */
static gchar *
translate_binding_dnm_to_gtk (const gchar *dnm_binding)
{
  /* hold is now "modifiers+keyname" or just "keyname" */
  guint len, i;
  gchar **tokens = g_strsplit (dnm_binding, "+", 0);
  gchar *res, *save;
  len = g_strv_length(tokens);
  if (len == 0)
      res = NULL;
  else if (len == 1)
      res = g_strdup(dnm_binding);
  else {
      res = "";
      for (i = 0; i < len - 1; i++) {
          save = res;
          res = g_strconcat(res, "<", tokens[i], ">", NULL);
          if (save[0])
            g_free(save);
      }
      save = res;
      res = g_strconcat(res, tokens[len-1], NULL);
      g_free(save);
  }
  g_strfreev(tokens);
  return res;
}

/*
 * translate a keybinding from the format used in denemo keymaprc file to the
 * format understood by gtk_accelerator_parse. The output is an allocated string
 * that must be freed by the caller.
 */
static gchar *
translate_binding_gtk_to_dnm (const gchar *gtk_binding)
{
  gchar *res = "", *save, *next, *mod;
  const gchar *cur = gtk_binding;
  while (cur[0] == '<') {
      next = strchr(cur, '>');
      if (!next) {
          cur = NULL;
          if (res[0])
            g_free(res);
          return NULL;
      }
      mod = g_strndup(cur + 1, next - cur - 1);
      if (res[0]) {
        save = res;
        res = g_strconcat(res, "+", NULL);
        g_free(save);
      }
      save = res;
      res = g_strconcat(res, mod, NULL);
      g_free(mod);
      if (save[0])
        g_free(save);
      cur = next + 1;
  }
  if (!res[0])
      return g_strdup(gtk_binding);
  else {
      save = res;
      res = g_strconcat(res, "+", cur, NULL);
      g_free(save);
      return res;
  }
}

static gint
get_state (gchar * key)
{
  gint ret = -1;
  if (0 == strcmp (key, "Ctrl"))
    ret = 4;
  else if (0 == strcmp (key, "Shift"))
    ret = 1;
  else if (0 == strcmp (key, "Alt"))
    ret = 8;
  else if (0 == strcmp (key, "Ctrl+Shift"))
    ret = 5;
  else if (0 == strcmp (key, "Alt+Shift"))
    ret = 9;
  else if (0 == strcmp (key, "Alt+Ctrl"))
    ret = 12;
  else if (0 == strcmp (key, "Alt+Ctrl+Shift"))
    ret = 13;

  return ret;
}



static void
parseScripts (xmlDocPtr doc, xmlNodePtr cur, keymap * the_keymap)
{
  cur = cur->xmlChildrenNode;
  gint command_number = -1;
  guint keyval = 0;
  gboolean is_script = FALSE;
  GdkModifierType state = 0;
  DenemoGUI *gui = Denemo.gui;
  for ( ;cur; cur = cur->next)
    {
      //keyval variables
      xmlChar *name, *menupath, *label, *tooltip, *scheme;

      if (0 == xmlStrcmp (cur->name, (const xmlChar *) "command"))
	{
	  if (cur->xmlChildrenNode == NULL)
            {
              g_warning ("Empty children node found in keymap file\n");
            }
	  else
	    {
	      name = 
		xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
#ifdef DEBUG
	      g_print ("Action %s\n", (gchar *) name);
#endif /*DEBUG*/
	    }
	} else if (0 == xmlStrcmp (cur->name, (const xmlChar *) "scheme")) {
	scheme = 
		xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	is_script = TRUE;
      }
      else if (0 == xmlStrcmp (cur->name, (const xmlChar *) "menupath")) {
	menupath = 
		xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
      }
      else if (0 == xmlStrcmp (cur->name, (const xmlChar *) "label")) {
	label = 
		xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
      }
      else if (0 == xmlStrcmp (cur->name, (const xmlChar *) "tooltip")) {
	tooltip = 
	  xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	/* by convention this is the last of the fields defining a scheme script menu item */
	if(is_script) {
	  name = name?name:"NoName";
	  label = label?label:"No label";
	  menupath = menupath?menupath:"/MainMenu/Other";
	  scheme = scheme?scheme:";;empty script\n";
	  tooltip = tooltip?tooltip:"No indication what this done beyond the name and label :(";


	  //FIXME duplicate code with view.c *************
	  GtkAction *action = gtk_action_new(name,label,tooltip,NULL);
	  GtkActionGroup *action_group;
	  GList *groups = gtk_ui_manager_get_action_groups (Denemo.ui_manager);
	  action_group = GTK_ACTION_GROUP(groups->data); //FIXME assuming the one we want is first
	  gtk_action_group_add_action(action_group, action);
	  g_object_set_data(G_OBJECT(action), "scheme", scheme);
	  g_object_set_data(G_OBJECT(action), "menupath", menupath);
	  g_signal_connect (G_OBJECT (action), "activate",
			    G_CALLBACK (activate_script), gui);
	  gtk_ui_manager_add_ui(Denemo.ui_manager,gtk_ui_manager_new_merge_id(Denemo.ui_manager), 
				menupath,
				name, name, GTK_UI_MANAGER_AUTO, FALSE);
	  GSList *h = gtk_action_get_proxies (action);//what is a proxy? any widget that callbacks the action
	  for(;h;h=h->next) {
	    attach_set_accel_callback(h->data, action, gui);
	  }
	  //g_print("registering %s\n", name);
	  register_command(Denemo.prefs.the_keymap, action, name, label, tooltip, activate_script);
	  //end duplicate code **************
	  
	 
	}// is_script
	// we are not as yet re-writing tooltips etc on builtin commands
      }

      
    }
 alphabeticalize_commands(Denemo.prefs.the_keymap);
}


static void
parseBindings (xmlDocPtr doc, xmlNodePtr cur, keymap * the_keymap)
{
  cur = cur->xmlChildrenNode;
  gint command_number = -1;
  guint keyval = 0;
  gboolean is_script = FALSE;
  GdkModifierType state = 0;
  DenemoGUI *gui = Denemo.gui;
  while (cur != NULL)
    {
      //keyval variables
      xmlChar *name, *menupath, *label, *tooltip, *scheme;

      if (0 == xmlStrcmp (cur->name, (const xmlChar *) "command"))
	{
	  if (cur->xmlChildrenNode == NULL)
            {
              g_warning ("Empty children node found in keymap file\n");
            }
	  else
	    {
	      name = 
		xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
#ifdef DEBUG
	      g_print ("Action %s\n", (gchar *) name);
#endif /*DEBUG*/
	    }
	} else if (0 == xmlStrcmp (cur->name, (const xmlChar *) "bind"))
	{
	  command_number = lookup_index_from_name (the_keymap, (gchar *) name);
	  //g_print("Found bind node for action %s %d\n", name, command_number);
	  if (cur->xmlChildrenNode == NULL)
            {
              g_warning ("Empty children node found in keymap file\n");
            }
	  else
	    {
	      xmlChar *tmp = 
		xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	      if (tmp)
		{
		  gchar *gtk_binding = translate_binding_dnm_to_gtk((gchar *) tmp);
          if (gtk_binding) {
            dnm_accelerator_parse(gtk_binding, &keyval, &state);
#ifdef DEBUG
		    g_print ("binding %s, keyval %d, state %d, Command Number %d\n",
                  gtk_binding, keyval, state, command_number);
#endif
		    if (command_number != -1)
		        add_keybinding_from_idx (the_keymap, keyval, state,
                        command_number, POS_LAST);
		  
		    g_free (gtk_binding);
          } else {
              g_warning("Could not parse binding", (gchar *) tmp);
          }
		  xmlFree (tmp);
		}
	    }
	}
      cur = cur->next;
    }
} // parseBindings




static void
parseCommands (xmlDocPtr doc, xmlNodePtr cur, keymap * the_keymap)
{
  xmlNodePtr ncur = cur->xmlChildrenNode;
  int i;
  for(i=0;i<2;i++)//Two passes, as all Commands have to be added before bindings
  for (ncur = cur->xmlChildrenNode; ncur; ncur = ncur->next)
    {
      if ((0 == xmlStrcmp (ncur->name, (const xmlChar *) "builtin")) ||
	  (0 == xmlStrcmp (ncur->name, (const xmlChar *) "script")))
	{
	  i?parseBindings (doc, ncur, the_keymap):parseScripts (doc, ncur, the_keymap);
	}
    }
}

static void
parseKeymap (xmlDocPtr doc, xmlNodePtr cur, keymap * the_keymap)
{
  cur = cur->xmlChildrenNode;

  while (cur != NULL)
    {
      if (0 == xmlStrcmp (cur->name, (const xmlChar *) "map"))
	{
	  parseCommands (doc, cur, the_keymap);
	}
      cur = cur->next;
    }


}

/* returns 0 on success
 * negative on failure
 */
gint
load_xml_keymap (gchar * filename, keymap * the_keymap)
{
  gint ret = -1;
  xmlDocPtr doc;
  //xmlNsPtr ns;
  xmlNodePtr rootElem;
  if(filename==NULL)
    return ret;
  doc = xmlParseFile (filename);

  if (doc == NULL)
    {
      g_warning ("Could not read XML file %s", filename);
      return ret;
    }

  rootElem = xmlDocGetRootElement (doc);
  if (rootElem == NULL)
    {
      g_warning ("Empty Document\n");
      xmlFreeDoc (doc);
      return ret;
    }
  //g_print ("RootElem %s\n", rootElem->name);
  if (xmlStrcmp (rootElem->name, (const xmlChar *) "Denemo"))
    {
      g_warning ("Document has wrong type\n");
      xmlFreeDoc (doc);
      return ret;
    }
  rootElem = rootElem->xmlChildrenNode;
  while (rootElem != NULL)
    {
#ifdef DEBUG
      g_print ("RootElem %s\n", rootElem->name);
#endif
      if (0 == xmlStrcmp (rootElem->name, (const xmlChar *) "commands"))
	{

	  parseKeymap (doc, rootElem, the_keymap);
	}
      rootElem = rootElem->next;
    }
  ret = 0;
  xmlFreeDoc (doc);
  return ret;
}


static void
write_xml_keybinding_info (gchar *kb_name, xmlNodePtr node)
{
  gchar *dnm_binding = translate_binding_gtk_to_dnm(kb_name);
#ifdef DEBUG
  g_print ("binding is : (dnm) %s, (gtk) %s \n", dnm_binding, kb_name);
#endif
  xmlNewTextChild (node, NULL, (xmlChar *) "bind", (xmlChar *) dnm_binding);
  g_free(dnm_binding);

}

gint
save_xml_keymap (gchar * filename, keymap * the_keymap)
{
  gint i, ret = -1;
  xmlDocPtr doc;
  //xmlNsPtr ns;
  xmlNodePtr parent, child, command;

  doc = xmlNewDoc ((xmlChar *) "1.0");
  doc->xmlRootNode = parent = xmlNewDocNode (doc, NULL, (xmlChar *) "Denemo",
					     NULL);
  child = xmlNewChild (parent, NULL, (xmlChar *) "commands", NULL);

  xmlNewTextChild (child, NULL, (xmlChar *) "title", (xmlChar *) "A Denemo Keymap");
  xmlNewTextChild (child, NULL, (xmlChar *) "author", (xmlChar *) "AT, JRR, RTS");

  parent = xmlNewChild (child, NULL, (xmlChar *) "map", NULL);

  for (i = 0; i < keymap_size(the_keymap); i++)
    {
     
      gpointer action = lookup_action_from_idx(the_keymap, i);
      gchar *scheme = action?g_object_get_data(action, "scheme"):NULL;
      if(scheme) 
	child = xmlNewChild (parent, NULL, (xmlChar *) "script", NULL);
      else
	child = xmlNewChild (parent, NULL, (xmlChar *) "builtin", NULL);
      
      gchar *name = lookup_name_from_idx(the_keymap, i);
#ifdef DEBUG
      g_print ("%s \n", name);
#endif	
      xmlNewTextChild (child, NULL, (xmlChar *) "command",
		       (xmlChar *) name);  
      if(scheme) 	
	xmlNewTextChild (child, NULL, (xmlChar *) "scheme",
			 (xmlChar *) scheme);
      
      gchar *menupath = action?g_object_get_data(action, "menupath"):NULL;
      if(menupath)
	xmlNewTextChild (child, NULL, (xmlChar *) "menupath",
			 (xmlChar *) menupath);
      
      gchar *label =   lookup_label_from_idx (the_keymap, i);
      if(label)
	xmlNewTextChild (child, NULL, (xmlChar *) "label",
			 (xmlChar *) label);
      
      
      gchar *tooltip = lookup_tooltip_from_idx (the_keymap, i);
      if(tooltip)
	xmlNewTextChild (child, NULL, (xmlChar *) "tooltip",
			 (xmlChar *) tooltip);
      
      
      keymap_foreach_command_binding (the_keymap, i,
				      (GFunc) write_xml_keybinding_info, child);
      
    }

  xmlSaveFormatFile (filename, doc, 1);

  xmlFreeDoc (doc);
  return ret;
}

static 	void show_type(GtkWidget *widget, gchar *message) {
    g_print("%s%s\n",message, widget?g_type_name(G_TYPE_FROM_INSTANCE(widget)):"NULL widget");
  }

static gint
parseMenu(xmlNodePtr rootElem, gchar *path, DenemoGUI *gui ) {
  for ( rootElem = rootElem->xmlChildrenNode;rootElem; rootElem = rootElem->next)
    {
      if(0 == xmlStrcmp (rootElem->name, (const xmlChar *) "menubar") ||
	 0 == xmlStrcmp (rootElem->name, (const xmlChar *) "menu") ||
	 0 == xmlStrcmp (rootElem->name, (const xmlChar *) "menubar") ||
	 0 == xmlStrcmp (rootElem->name, (const xmlChar *) "toolbar"))
	/* ignoring popup menus */ {
	gchar *name = (gchar *) xmlGetProp (rootElem, (xmlChar *) "name");
	if(name==NULL)
	  name = (gchar *) xmlGetProp (rootElem, (xmlChar *) "action");
	
	if(name) {
	  gchar *str = g_strdup_printf("%s%s%s", path, "/", name);
	  GtkWidget *widget = gtk_ui_manager_get_widget (Denemo.ui_manager, str); 
	  if(widget) {
	    g_object_set_data(G_OBJECT(widget), "menupath", str);
	    //show_type(widget, "The type is ");
	    //g_print("set %p %s\n",widget, str);
	    parseMenu(rootElem, str, gui);
	  }
	  else
	    g_warning("no object for %s\n", str);
	}

      }
      if(0 == xmlStrcmp (rootElem->name, (const xmlChar *) "menuitem")) {
	gchar *name = (gchar *) xmlGetProp (rootElem, (xmlChar *) "action");
       if(name) {
	  gchar *str = g_strdup_printf("%s%s%s", path, "/", name);
	  GtkWidget *widget = gtk_ui_manager_get_widget (Denemo.ui_manager, str);
	  g_free(str);
	  //show_type(widget, "The type is ");
	  //g_print("set %p %s\n",widget, path);
	  if(widget) {
	    g_object_set_data(G_OBJECT(widget), "menupath", g_strdup(path));
	  }
       }
      }
    }
  return 0;
}

/* 
 * attaches the menu hierarchy path to each menuitem widget in the passed file (denemoui.xml)
 * returns 0 on success
 * negative on failure
 */
gint
parse_paths (gchar * filename, DenemoGUI *gui)
{
  gint ret = -1;
  xmlDocPtr doc;
  //xmlNsPtr ns;
  xmlNodePtr rootElem;
  if(filename==NULL)
    return ret;
  doc = xmlParseFile (filename);

  if (doc == NULL)
    {
      g_warning ("Could not read XML file %s", filename);
      return ret;
    }

  rootElem = xmlDocGetRootElement (doc);
  if (rootElem == NULL)
    {
      g_warning ("Empty Document\n");
      xmlFreeDoc (doc);
      return ret;
    }
  //g_print ("RootElem %s\n", rootElem->name);
  if (xmlStrcmp (rootElem->name, (const xmlChar *) "ui"))
    {
      g_warning ("Document has wrong type\n");
      xmlFreeDoc (doc);
      return ret;
    }
  gchar *path="";
  parseMenu(rootElem, path, gui);
  ret = 0;
  xmlFreeDoc (doc);
  return ret;
}
