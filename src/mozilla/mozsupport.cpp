/**
 * @file mozsupport.cpp C++ portion of GtkMozEmbed support
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2008 Alexander Sack <asac@ubuntu.com>
 *
 * The preference handling was taken from the Galeon source
 *
 *  Copyright (C) 2000 Marco Pesenti Gritti 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mozilla-config.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

// for GLUE we _must_ not use MOZILLA_INTERNAL_API 
#ifndef XPCOM_GLUE
#  define MOZILLA_INTERNAL_API
#endif

#include "mozsupport.h"
#include <gtk/gtk.h>

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>

// if we use the glue (since 1.9), we must explicitly get the gtkmozembed symbols
#ifdef XPCOM_GLUE
#  include <gtkmozembed_glue.cpp>
#endif

// some includes were moved with 1.9, so we need to switch here
#ifdef XPCOM_GLUE
#include <nsIDOMKeyEvent.h>
#include <nsNetCID.h>
#else
#include <dom/nsIDOMKeyEvent.h>
#include <necko/nsNetCID.h>
#endif

#include <nsIWebBrowser.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMWindow.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsIIOService.h>
#include <nsCOMPtr.h>

#include <nsServiceManagerUtils.h>

extern "C" {
#include "conf.h"
#include "ui/ui_itemlist.h"
}

extern "C" 
gint mozsupport_key_press_cb (GtkWidget *widget, gpointer ev)
{
	nsIDOMKeyEvent	*event = (nsIDOMKeyEvent*)ev;
	PRUint32	keyCode = 0;
	PRBool		alt, ctrl, shift;
	
	/* This key interception is necessary to catch
	   spaces which are an internal Mozilla key binding.
	   All other combinations like Ctrl-Space, Alt-Space
	   can be caught with the GTK code in ui_mainwindow.c.
	   This is a bad case of code duplication... */
	
	event->GetCharCode (&keyCode);
	if (keyCode == nsIDOMKeyEvent::DOM_VK_SPACE) {
     		event->GetShiftKey (&shift);
     		event->GetCtrlKey (&ctrl);
     		event->GetAltKey (&alt);

		/* Do trigger scrolling if the skimming hotkey is 
		  <Space> with a modifier. Other cases (Space+modifier)
		  are handled in src/ui/ui_mainwindow.c and if we
		  get <Space> with a modifier here it needs no extra
		  handling. */
		if ((0 == conf_get_int_value (BROWSE_KEY_SETTING)) &&
		    !(alt | shift | ctrl)) {
			if (mozsupport_scroll_pagedown(widget) == FALSE)
				on_next_unread_item_activate (NULL, NULL);
			return TRUE;
		}
	}	
	return FALSE;
}
/**
 * Takes a pointer to a mouse event and returns the mouse
 *  button number or -1 on error.
 */
extern "C" 
gint mozsupport_get_mouse_event_button(gpointer event) {
	gint	button = 0;
	
	g_return_val_if_fail (event, -1);

	/* the following lines were found in the Galeon source */	
	nsIDOMMouseEvent *aMouseEvent = (nsIDOMMouseEvent *) event;
	aMouseEvent->GetButton ((PRUint16 *) &button);

	/* for some reason we get different numbers on PPC, this fixes
	 * that up... -- MattA */
	if (button == 65536)
	{
		button = 1;
	}
	else if (button == 131072)
	{
		button = 2;
	}

	return button;
}

extern "C" void
mozsupport_set_zoom (GtkWidget *embed, gfloat aZoom)
{
	nsCOMPtr<nsIWebBrowser>		mWebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		mDOMWindow;
	
	gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (embed), getter_AddRefs (mWebBrowser));
	if (NULL == mWebBrowser) {
		g_warning ("mozsupport_set_zoom(): Could not retrieve browser...");
		return;
	}
	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));
	if (NULL == mDOMWindow) {
		g_warning ("mozsupport_set_zoom(): Could not retrieve DOM window...");
		return;
	}
	mDOMWindow->SetTextZoom (aZoom);
}

extern "C" gfloat
mozsupport_get_zoom (GtkWidget *embed)
{
	nsCOMPtr<nsIWebBrowser>		mWebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		mDOMWindow;
	float zoom;
	
	gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (embed), getter_AddRefs (mWebBrowser));
	if (NULL == mWebBrowser) {
		g_warning ("mozsupport_get_zoom(): Could not retrieve browser...");
		return 1.0;
	}
	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));	
	if (NULL == mDOMWindow) {
		g_warning ("mozsupport_get_zoom(): Could not retrieve DOM window...");
		return 1.0;
	}
	mDOMWindow->GetTextZoom (&zoom);	
	return zoom;
}

extern "C" void mozsupport_scroll_to_top(GtkWidget *embed) {
	nsCOMPtr<nsIWebBrowser>		WebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		DOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), getter_AddRefs(WebBrowser));
	WebBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));	
	if(NULL == DOMWindow) {
		g_warning("could not retrieve DOM window...");
		return;
	}

	DOMWindow->ScrollTo(0, 0);
}

extern "C" gboolean mozsupport_scroll_pagedown(GtkWidget *embed) {
	gint initial_y, final_y;
	nsCOMPtr<nsIWebBrowser>		WebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		DOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), getter_AddRefs(WebBrowser));
	WebBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));	
	if(NULL == DOMWindow) {
		g_warning("could not retrieve DOM window...");
		return FALSE;
	}

	DOMWindow->GetScrollY(&initial_y);
	DOMWindow->ScrollByPages(1);
	DOMWindow->GetScrollY(&final_y);
	
	return initial_y != final_y;
}

/* the following code is from the Galeon source mozilla/mozilla.cpp */

extern "C" gboolean
mozsupport_save_prefs (void)
{
	nsCOMPtr<nsIPrefService> prefService = 
				 do_GetService (NS_PREFSERVICE_CONTRACTID);
	g_return_val_if_fail (prefService != nsnull, FALSE);

	nsresult rv = prefService->SavePrefFile (nsnull);
	return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozsupport_preference_set: set a string mozilla preference
 */
extern "C" gboolean
mozsupport_preference_set(const char *preference_name, const char *new_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	/*It is legitimate to pass in a NULL value sometimes. So let's not
	 *assert and just check and return.
	 */
	if (!new_value) return FALSE;

	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));

	if (pref)
	{
		nsresult rv = pref->SetCharPref (preference_name, new_value);            
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * mozsupport_preference_set_boolean: set a boolean mozilla preference
 */
extern "C" gboolean
mozsupport_preference_set_boolean (const char *preference_name,
				gboolean new_boolean_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);
  
	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));
  
	if (pref)
	{
		nsresult rv = pref->SetBoolPref (preference_name,
				new_boolean_value ? PR_TRUE : PR_FALSE);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * mozsupport_preference_set_int: set an integer mozilla preference
 */
extern "C" gboolean
mozsupport_preference_set_int (const char *preference_name, int new_int_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));

	if (pref)
	{
		nsresult rv = pref->SetIntPref (preference_name, new_int_value);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * Set Mozilla caching to on or off line mode
 */
extern "C" void
mozsupport_set_offline_mode (gboolean offline)
{
	nsresult rv;

	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
	if (NS_SUCCEEDED(rv))
	{
		rv = io->SetOffline(offline);
		//if (NS_SUCCEEDED(rv)) return TRUE;
	}
	//return FALSE;
}


/* helpers for binaries linked against XPCOM_GLUE */
#ifdef XPCOM_GLUE

/**
 * load xpcom through glue.
 * When using the glue you have to call this method before doing
 * anything else. It finds the GRE, loads the xpcom libs,
 * maps the gtkmozbemd symbols and intializes xpcom by setting
 * the path and component path.
 *
 * the caller still has to call gtk_moz_embed_push_startup ()
 */
extern "C" gboolean
mozsupport_xpcom_init ()
{
	static const GREVersionRange greVersion = {
		"1.9a", PR_TRUE,
		"1.9.*", PR_TRUE
	};
	char xpcomLocation[4096];
	nsresult rv = GRE_GetGREPathWithProperties (&greVersion, 1, nsnull, 0, xpcomLocation, 4096);
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED (rv));
	// Startup the XPCOM Glue that links us up with XPCOM.
	rv = XPCOMGlueStartup(xpcomLocation);
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED (rv));
	rv = GTKEmbedGlueStartup();
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED (rv));
	rv = GTKEmbedGlueStartupInternal();
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED (rv));
	char *lastSlash = strrchr (xpcomLocation, '/');
	if (lastSlash)
		*lastSlash = '\0';
	gtk_moz_embed_set_path (xpcomLocation);

	return TRUE;
}

extern "C" gboolean
mozsupport_xpcom_shutdown ()
{
	return NS_SUCCEEDED (XPCOMGlueShutdown ());
}
#endif


/* helpers for binaries linked against XPCOM_GLUE */
#ifdef XPCOM_GLUE

/**
 * load xpcom through glue.
 * When using the glue you have to call this method before doing
 * anything else. It finds the GRE, loads the xpcom libs,
 * maps the gtkmozbemd symbols and intializes xpcom by setting
 * the path and component path.
 *
 * the caller still has to call gtk_moz_embed_push_startup()
 */
extern "C" gboolean
mozsupport_xpcom_init ()
{
	static const GREVersionRange greVersion = {
		"1.9a", PR_TRUE,
		"1.9.*", PR_TRUE
	};
	char xpcomLocation[4096];
	nsresult rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0, xpcomLocation, 4096);
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED(rv));
	// Startup the XPCOM Glue that links us up with XPCOM.
	rv = XPCOMGlueStartup(xpcomLocation);
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED(rv));
	rv = GTKEmbedGlueStartup();
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED(rv));
	rv = GTKEmbedGlueStartupInternal();
	NS_ENSURE_SUCCESS (rv, NS_SUCCEEDED(rv));
	char *lastSlash = strrchr(xpcomLocation, '/');
	if (lastSlash)
		*lastSlash = '\0';
	gtk_moz_embed_set_path(xpcomLocation);

	return TRUE;
}

extern "C" gboolean
mozsupport_xpcom_shutdown ()
{
	return NS_SUCCEEDED(XPCOMGlueShutdown());
}
#endif
