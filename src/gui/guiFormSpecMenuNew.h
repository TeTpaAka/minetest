/*
Minetest
Copyright (C) 2013-2018 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <utility>
#include <memory>

#include "irrlichttypes_extrabloated.h"
#include "inventorymanager.h"
#include "modalMenu.h"
#include "guiFormSpecMenuElement.h"
#include "network/networkprotocol.h"
#include "client/joystick_controller.h"
#include "client/hud.h" // drawItemStack
#include "util/string.h"
#include "util/enriched_string.h"

class GUIFormSpecMenuNew : public GUIModalMenu
{
public:
	GUIFormSpecMenuNew(JoystickController *joystick,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr,
			Client *client,
			ISimpleTextureSource *tsrc,
			const std::string &source,
			bool remap_dbl_click = true);

	~GUIFormSpecMenuNew();

	static void create(GUIFormSpecMenuNew *&cur_formspec, Client *client,
			JoystickController *joystick, const std::string &source);
	void setFormSource(const std::string &source) {
		formspec_string = source;
	}

	void regenerateGui(v2u32 screensize);

	void drawMenu();
	
	bool preprocessEvent(const SEvent& event);
	bool OnEvent(const SEvent& event);

#ifdef __ANDROID__
	bool getAndroidUIInput();
#endif

protected:
	InventoryManager *m_invmgr;
	ISimpleTextureSource *m_tsrc;
	Client *m_client;

	bool needsReparse = true;
	std::string formspec_string;

	v2s32 m_pointer;
	v2s32 m_old_pointer;  // Mouse position after previous mouse event

	bool m_allowclose = true;

	void hover(GUIFormSpecMenuElement *element);

private:
	JoystickController *m_joystick;

	std::unique_ptr<GUIFormSpecMenuElement> forms = nullptr;

	GUIFormSpecMenuElement *hovered = nullptr;
	GUIFormSpecMenuElement *clicked = nullptr;
	GUIFormSpecMenuElement *focused = nullptr;

	void tryClose();

	/**
	 * check if event is part of a double click
	 * @param event event to evaluate
	 * @return true/false if a doubleclick was detected
	 */
	bool DoubleClickDetection(const SEvent event);

	struct clickpos
	{
		v2s32 pos;
		s64 time;
	};
	clickpos m_doubleclickdetect[2];

	gui::IGUIFont *m_font = nullptr;
	std::shared_ptr<StyleSpec> defaultStyle;

#ifdef __ANDROID__
	v2s32 m_down_pos;
	std::string m_JavaDialogFieldName;
#endif

	/* If true, remap a double-click (or double-tap) action to ESC. This is so
	 * that, for example, Android users can double-tap to close a formspec.
	*
	 * This value can (currently) only be set by the class constructor
	 * and the default value for the setting is true.
	 */
	bool m_remap_dbl_click;

};

