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

#include "core/iconmanager.hpp"
#include "util/i18n.hpp"

#include <gtkmm/stockitem.h>
#include <gtkmm/icontheme.h>

Gtk::StockID Gobby::IconManager::STOCK_SAVE_ALL("gobby-save-all");
Gtk::StockID Gobby::IconManager::STOCK_USERLIST("gobby-user-list");
Gtk::StockID Gobby::IconManager::STOCK_DOCLIST("gobby-document-list");
Gtk::StockID Gobby::IconManager::STOCK_CHAT("gobby-chat");
Gtk::StockID Gobby::IconManager::STOCK_USER_COLOR_INDICATOR(
	"gobby-user-color-indicator");

// TODO: The save-all icon does not match the save icon for toolbar
// or menu sized items. It is not yet enabled therefore.
Gobby::IconManager::IconManager():
	m_is_save_all(GtkCompat::create_icon_set()),
	m_is_userlist(GtkCompat::create_icon_set()),
	m_is_doclist(GtkCompat::create_icon_set()),
	m_is_chat(GtkCompat::create_icon_set()),
	m_is_user_color_indicator(GtkCompat::create_icon_set()),
	m_icon_factory(Gtk::IconFactory::create())
{
	Gtk::IconTheme::get_default()->append_search_path(PUBLIC_ICONS_DIR);
	Gtk::IconTheme::get_default()->append_search_path(PRIVATE_ICONS_DIR);

	Gtk::StockItem save_all_item(STOCK_SAVE_ALL, _("Save All"));
	//m_icon_factory->add(STOCK_SAVE_ALL, m_is_save_all);

	Gtk::IconSource userlist_source;
	userlist_source.set_icon_name("user-list");
	m_is_userlist->add_source(userlist_source);
	Gtk::StockItem userlist_item(STOCK_USERLIST, _("User list") );
	m_icon_factory->add(STOCK_USERLIST, m_is_userlist);

	Gtk::IconSource doclist_source;
	doclist_source.set_icon_name("document-list");
	m_is_doclist->add_source(doclist_source);
	Gtk::StockItem doclist_item(STOCK_DOCLIST, _("Document list") );
	m_icon_factory->add(STOCK_DOCLIST, m_is_doclist);

	Gtk::IconSource chat_source;
	chat_source.set_icon_name("chat");
	m_is_chat->add_source(chat_source);
	Gtk::StockItem chat_item(STOCK_CHAT, _("Chat") );
	m_icon_factory->add(STOCK_CHAT, m_is_chat);

	Gtk::IconSource user_color_indicator_source;
	user_color_indicator_source.set_icon_name("user-color-indicator");
	m_is_user_color_indicator->add_source(user_color_indicator_source);
	Gtk::StockItem user_color_indicator_item(STOCK_USER_COLOR_INDICATOR,
	                                         _("User Color Indicator"));
	m_icon_factory->add(STOCK_USER_COLOR_INDICATOR,
	                    m_is_user_color_indicator);

	m_icon_factory->add_default();
}
