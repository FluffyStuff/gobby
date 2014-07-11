/* Gobby - GTK-based collaborative text editor
 * Copyright (C) 2008-2014 Armin Burgmeier <armin@arbur.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "commands/browser-context-commands.hpp"
#include "operations/operation-open-multiple.hpp"
#include "util/i18n.hpp"

#include <libinfgtk/inf-gtk-permissions-dialog.h>

#include <gtkmm/icontheme.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/stock.h>
#include <giomm/file.h>

// TODO: Use file tasks for the commands, once we made them public

Gobby::BrowserContextCommands::BrowserContextCommands(Gtk::Window& parent,
                                                      Browser& browser,
                                                      FileChooser& chooser,
                                                      Operations& operations,
						      const Preferences& prf):
	m_parent(parent), m_browser(browser), m_file_chooser(chooser),
	m_operations(operations), m_preferences(prf), m_popup_menu(NULL)
{
	m_populate_popup_handler = g_signal_connect(
		m_browser.get_view(), "populate-popup",
		G_CALLBACK(on_populate_popup_static), this);
}

Gobby::BrowserContextCommands::~BrowserContextCommands()
{
	g_signal_handler_disconnect(m_browser.get_view(),
	                            m_populate_popup_handler);
}

void Gobby::BrowserContextCommands::on_menu_node_removed()
{
	g_assert(m_popup_menu != NULL);

	// This calls deactivate, causing the watch to be removed.
	m_popup_menu->popdown();
}

void Gobby::BrowserContextCommands::on_menu_deactivate()
{
	m_watch.reset(NULL);
}

void Gobby::BrowserContextCommands::on_populate_popup(Gtk::Menu* menu)
{
	// TODO: Can this happen? Should we close the old popup here?
	g_assert(m_popup_menu == NULL);

	// Cancel previous attempts
	m_watch.reset(NULL);
	m_dialog.reset(NULL);

	InfBrowser* browser;
	InfBrowserIter iter;
	if(!m_browser.get_selected(&browser, &iter))
		return;

	InfBrowserStatus browser_status;
	g_object_get(G_OBJECT(browser), "status", &browser_status, NULL);
	if(browser_status != INF_BROWSER_OPEN)
		return;

	InfBrowserIter dummy_iter = iter;
	bool is_subdirectory = inf_browser_is_subdirectory(browser, &iter);
	bool is_toplevel = !inf_browser_get_parent(browser, &dummy_iter);

	// Watch the node, and close the popup menu when the node
	// it refers to is removed.
	m_watch.reset(new NodeWatch(browser, &iter));
	m_watch->signal_node_removed().connect(sigc::mem_fun(
		*this, &BrowserContextCommands::on_menu_node_removed));

	menu->signal_deactivate().connect(sigc::mem_fun(
		*this, &BrowserContextCommands::on_menu_deactivate));

	// Add "Disconnect" menu option if the connection
	// item has been clicked at
	if(is_toplevel && INFC_IS_BROWSER(browser))
	{
		Gtk::ImageMenuItem* disconnect_item = Gtk::manage(
			new Gtk::ImageMenuItem(_("_Disconnect from Server"), true));
		disconnect_item->set_image(*Gtk::manage(new Gtk::Image(
			Gtk::Stock::DISCONNECT, Gtk::ICON_SIZE_MENU)));
		disconnect_item->signal_activate().connect(sigc::bind(
			sigc::mem_fun(*this,
				&BrowserContextCommands::on_disconnect),
			INFC_BROWSER(browser)));
		disconnect_item->show();
		menu->append(*disconnect_item);

		// Separator
		Gtk::SeparatorMenuItem* sep_item =
			Gtk::manage(new Gtk::SeparatorMenuItem);
		sep_item->show();
		menu->append(*sep_item);
	}

	// Create Document
	Gtk::ImageMenuItem* new_document_item = Gtk::manage(
		new Gtk::ImageMenuItem(_("Create Do_cument..."),
		                       true));
	new_document_item->set_image(*Gtk::manage(new Gtk::Image(
		Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU)));
	new_document_item->signal_activate().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_new),
		browser, iter, false));
	new_document_item->set_sensitive(is_subdirectory);
	new_document_item->show();
	menu->append(*new_document_item);

	// Create Directory

	// Check whether we have the folder-new icon, fall back to
	// Stock::DIRECTORY otherwise
	Glib::RefPtr<Gdk::Screen> screen = menu->get_screen();
	Glib::RefPtr<Gtk::IconTheme> icon_theme(
		Gtk::IconTheme::get_for_screen(screen));

	Gtk::Image* new_directory_image = Gtk::manage(new Gtk::Image);
	if(icon_theme->lookup_icon("folder-new",
	                           Gtk::ICON_SIZE_MENU,
	                           Gtk::ICON_LOOKUP_USE_BUILTIN))
	{
		new_directory_image->set_from_icon_name(
			"folder-new", Gtk::ICON_SIZE_MENU);
	}
	else
	{
		new_directory_image->set(
			Gtk::Stock::DIRECTORY, Gtk::ICON_SIZE_MENU);
	}

	Gtk::ImageMenuItem* new_directory_item = Gtk::manage(
		new Gtk::ImageMenuItem(_("Create Di_rectory..."),
		                       true));
	new_directory_item->set_image(*new_directory_image);
	new_directory_item->signal_activate().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_new),
		browser, iter, true));
	new_directory_item->set_sensitive(is_subdirectory);
	new_directory_item->show();
	menu->append(*new_directory_item);

	// Open Document
	Gtk::ImageMenuItem* open_document_item = Gtk::manage(
		new Gtk::ImageMenuItem(_("_Open Document..."), true));
	open_document_item->set_image(*Gtk::manage(new Gtk::Image(
		Gtk::Stock::OPEN, Gtk::ICON_SIZE_MENU)));
	open_document_item->signal_activate().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_open),
		browser, iter));
	open_document_item->set_sensitive(is_subdirectory);
	open_document_item->show();
	menu->append(*open_document_item);

	// Separator
	Gtk::SeparatorMenuItem* sep_item =
		Gtk::manage(new Gtk::SeparatorMenuItem);
	sep_item->show();
	menu->append(*sep_item);

	// Permissions
	Gtk::ImageMenuItem* permissions_item = Gtk::manage(
		new Gtk::ImageMenuItem(_("_Permissions..."), true));
	permissions_item->set_image(*Gtk::manage(new Gtk::Image(
		Gtk::Stock::PROPERTIES, Gtk::ICON_SIZE_MENU)));
	permissions_item->signal_activate().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_permissions),
		browser, iter));
	permissions_item->show();
	menu->append(*permissions_item);

	// Rename
	Gtk::ImageMenuItem* rename_item = Gtk::manage(
		new Gtk::ImageMenuItem(_("Re_name"), true));
	rename_item->set_image(*Gtk::manage(new Gtk::Image(
		Gtk::Stock::EDIT, Gtk::ICON_SIZE_MENU)));
	rename_item->signal_activate().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_rename),
		browser, iter));
	rename_item->set_sensitive(!is_toplevel);
	rename_item->show();
	menu->append(*rename_item);

	// Delete
	Gtk::ImageMenuItem* delete_item = Gtk::manage(
		new Gtk::ImageMenuItem(_("D_elete"), true));
	delete_item->set_image(*Gtk::manage(new Gtk::Image(
		Gtk::Stock::DELETE, Gtk::ICON_SIZE_MENU)));
	delete_item->signal_activate().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_delete),
		browser, iter));
	delete_item->set_sensitive(!is_toplevel);
	delete_item->show();
	menu->append(*delete_item);
	
	m_popup_menu = menu;
	menu->signal_selection_done().connect(
		sigc::mem_fun(
			*this,
			&BrowserContextCommands::on_popdown));
}

void Gobby::BrowserContextCommands::on_popdown()
{
	m_popup_menu = NULL;
}

void Gobby::BrowserContextCommands::on_disconnect(InfcBrowser* browser)
{
	InfXmlConnection* connection = infc_browser_get_connection(browser);
	InfXmlConnectionStatus status;
	g_object_get(G_OBJECT(connection), "status", &status, NULL);

	if(status != INF_XML_CONNECTION_CLOSED &&
	   status != INF_XML_CONNECTION_CLOSING)
	{
		inf_xml_connection_close(connection);
	}
}

void Gobby::BrowserContextCommands::on_new(InfBrowser* browser,
                                           InfBrowserIter iter,
                                           bool directory)
{
	m_watch.reset(new NodeWatch(browser, &iter));
	m_watch->signal_node_removed().connect(sigc::mem_fun(
		*this, &BrowserContextCommands::on_dialog_node_removed));

	std::auto_ptr<EntryDialog> entry_dialog(
		new EntryDialog(
			m_parent,
			directory ? _("Choose a name for the directory")
			          : _("Choose a name for the document"),
			directory ? _("_Directory Name:")
			          : _("_Document Name:")));

	entry_dialog->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	entry_dialog->add_button(_("C_reate"), Gtk::RESPONSE_ACCEPT)
		->set_image(*Gtk::manage(new Gtk::Image(
			Gtk::Stock::NEW, Gtk::ICON_SIZE_BUTTON)));

	entry_dialog->set_text(directory ? _("New Directory")
	                                   : _("New Document"));
	entry_dialog->signal_response().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_new_response),
		browser, iter, directory));

	m_dialog = entry_dialog;
	m_dialog->present();
}

void Gobby::BrowserContextCommands::on_open(InfBrowser* browser,
                                            InfBrowserIter iter)
{
	m_watch.reset(new NodeWatch(browser, &iter));
	m_watch->signal_node_removed().connect(sigc::mem_fun(
		*this, &BrowserContextCommands::on_dialog_node_removed));

	std::auto_ptr<FileChooser::Dialog> file_dialog(
		new FileChooser::Dialog(
			m_file_chooser, m_parent,
			_("Choose a text file to open"),
			Gtk::FILE_CHOOSER_ACTION_OPEN));
	file_dialog->signal_response().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_open_response),
		browser, iter));

	file_dialog->set_select_multiple(true);

	m_dialog = file_dialog;
	m_dialog->present();
}

void Gobby::BrowserContextCommands::on_permissions(InfBrowser* browser,
                                                   InfBrowserIter iter)
{
	m_watch.reset(new NodeWatch(browser, &iter));
	m_watch->signal_node_removed().connect(sigc::mem_fun(
		*this, &BrowserContextCommands::on_dialog_node_removed));

	InfGtkPermissionsDialog* dlg = inf_gtk_permissions_dialog_new(
		m_parent.gobj(), static_cast<GtkDialogFlags>(0),
		browser, &iter);
	std::auto_ptr<Gtk::Dialog> permissions_dialog(
		Glib::wrap(GTK_DIALOG(dlg)));

	permissions_dialog->add_button(Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);

	permissions_dialog->signal_response().connect(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_permissions_response));

	m_dialog = permissions_dialog;
	m_dialog->present();
}

void Gobby::BrowserContextCommands::on_rename(InfBrowser* browser,
                                              InfBrowserIter iter)
{
	bool directory = inf_browser_is_subdirectory(browser, &iter);
	m_watch.reset(new NodeWatch(browser, &iter));
	m_watch->signal_node_removed().connect(sigc::mem_fun(
		*this, &BrowserContextCommands::on_dialog_node_removed));

	std::auto_ptr<EntryDialog> entry_dialog(
		new EntryDialog(
			m_parent,
			directory ? _("Choose a name for the directory")
			          : _("Choose a name for the document"),
			directory ? _("_Directory Name:")
			          : _("_Document Name:")));

	entry_dialog->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	entry_dialog->add_button(_("_Rename"), Gtk::RESPONSE_ACCEPT)
		->set_image(*Gtk::manage(new Gtk::Image(
			Gtk::Stock::EDIT, Gtk::ICON_SIZE_BUTTON)));

	entry_dialog->set_text(inf_browser_get_node_name(browser, &iter));
	entry_dialog->signal_response().connect(sigc::bind(
		sigc::mem_fun(*this,
			&BrowserContextCommands::on_rename_response),
		browser, iter));

	m_dialog = entry_dialog;
	m_dialog->present();
}

void Gobby::BrowserContextCommands::on_delete(InfBrowser* browser,
                                              InfBrowserIter iter)
{
	m_operations.delete_node(browser, &iter);
}

void Gobby::BrowserContextCommands::on_dialog_node_removed()
{
	m_watch.reset(NULL);
	m_dialog.reset(NULL);
}

void Gobby::BrowserContextCommands::on_new_response(int response_id,
                                                    InfBrowser* browser,
						    InfBrowserIter iter,
						    bool directory)
{
	EntryDialog* entry_dialog = static_cast<EntryDialog*>(m_dialog.get());

	if(response_id == Gtk::RESPONSE_ACCEPT)
	{
		if(directory)
		{
			// TODO: Select the newly created directory in tree
			m_operations.create_directory(
				browser, &iter, entry_dialog->get_text());
		}
		else
		{
			m_operations.create_document(
				browser, &iter, entry_dialog->get_text());
		}
	}

	m_watch.reset(NULL);
	m_dialog.reset(NULL);
}

void Gobby::BrowserContextCommands::on_open_response(int response_id,
                                                     InfBrowser* browser,
                                                     InfBrowserIter iter)
{
	FileChooser::Dialog* dialog =
		static_cast<FileChooser::Dialog*>(m_dialog.get());
	if(response_id == Gtk::RESPONSE_ACCEPT)
	{
		Glib::SListHandle<Glib::ustring> uris = dialog->get_uris();

		OperationOpenMultiple::uri_list uri_list(
			uris.begin(), uris.end());

		m_operations.create_documents(
			browser, &iter, m_preferences, uri_list);
	}

	m_watch.reset(NULL);
	m_dialog.reset(NULL);
}

void Gobby::BrowserContextCommands::on_permissions_response(int response_id)
{
	m_watch.reset(NULL);
	m_dialog.reset(NULL);
}

void Gobby::BrowserContextCommands::on_rename_response(int response_id,
                                                       InfBrowser* browser,
                                                       InfBrowserIter iter)
{
	EntryDialog* entry_dialog = static_cast<EntryDialog*>(m_dialog.get());

	if(response_id == Gtk::RESPONSE_ACCEPT)
	{
		m_operations.rename_node(browser, &iter, entry_dialog->get_text());
	}

	m_watch.reset(NULL);
	m_dialog.reset(NULL);
}