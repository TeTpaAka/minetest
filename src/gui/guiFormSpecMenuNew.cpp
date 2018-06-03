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


#include <cstdlib>
#include <utility>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <limits>
#include "guiFormSpecMenuNew.h"
#include "guiFormSpecParser.h"
#include "constants.h"
#include "gamedef.h"
#include "keycode.h"
#include <IGUIFont.h>
#include "client.h"
#include "client/renderingengine.h"
#include "log.h"
#include "client/tile.h" // ITextureSource
#include "gettime.h"
#include "mainmenumanager.h"
#include "fontengine.h"

/*
	GUIFormSpecMenuNew
*/
GUIFormSpecMenuNew::GUIFormSpecMenuNew(JoystickController *joystick,
		gui::IGUIElement *parent, s32 id, IMenuManager *menumgr,
		Client *client, ISimpleTextureSource *tsrc, const std::string &source,
		bool remap_dbl_click):
	GUIModalMenu(RenderingEngine::get_gui_env(), parent, id, menumgr),
	m_invmgr(client),
	m_tsrc(tsrc),
	m_client(client),
	formspec_string(source),
	m_joystick(joystick),
	m_remap_dbl_click(remap_dbl_click)
#ifdef __ANDROID__
	, m_JavaDialogFieldName("")
#endif
{
	m_doubleclickdetect[0].time = 0;
	m_doubleclickdetect[1].time = 0;

	m_doubleclickdetect[0].pos = v2s32(0, 0);
	m_doubleclickdetect[1].pos = v2s32(0, 0);
}

GUIFormSpecMenuNew::~GUIFormSpecMenuNew()
{
	removeChildren();
}

void GUIFormSpecMenuNew::create(GUIFormSpecMenuNew *&cur_formspec, Client *client,
	JoystickController *joystick, const std::string &source)
{
	if (cur_formspec == nullptr) {
		cur_formspec = new GUIFormSpecMenuNew(joystick, guiroot, -1, &g_menumgr,
			client, client->getTextureSource(), source);

		/*
			Caution: do not call (*cur_formspec)->drop() here --
			the reference might outlive the menu, so we will
			periodically check if *cur_formspec is the only
			remaining reference (i.e. the menu was removed)
			and delete it in that case.
		*/

	} else {
		cur_formspec->setFormSource(source);
		cur_formspec->needsReparse = true;
	}
}

void GUIFormSpecMenuNew::regenerateGui(v2u32 screensize)
{
	/* useless to regenerate without a screensize */
	if ((screensize.X <= 0) || (screensize.Y <= 0)) {
		return;
	}

	if (needsReparse) {
		m_font = g_fontengine->getFont();
		std::shared_ptr<StyleSpec> style(new StyleSpec(m_font));
		forms = GUIFormSpecParser::parse(formspec_string, m_tsrc, m_client, m_invmgr, style);
	}

	forms->rebuild(core::rect<s32>(0, 0, screensize.X, screensize.Y), m_font);
}

#ifdef __ANDROID__
// TODO actually do it
bool GUIFormSpecMenuNew::getAndroidUIInput()
{
	/* no dialog shown */
	if (m_JavaDialogFieldName == "") {
		return false;
	}

	/* still waiting */
	if (porting::getInputDialogState() == -1) {
		return true;
	}

	std::string fieldname = m_JavaDialogFieldName;
	m_JavaDialogFieldName = "";

	/* no value abort dialog processing */
	if (porting::getInputDialogState() != 0) {
		return false;
	}

	for(std::vector<FieldSpec>::iterator iter =  m_fields.begin();
			iter != m_fields.end(); ++iter) {

		if (iter->fname != fieldname) {
			continue;
		}
		IGUIElement* tochange = getElementFromId(iter->fid);

		if (tochange == 0) {
			return false;
		}

		if (tochange->getType() != irr::gui::EGUIET_EDIT_BOX) {
			return false;
		}

		std::string text = porting::getInputDialogValue();

		((gui::IGUIEditBox*) tochange)->
			setText(utf8_to_wide(text).c_str());
	}
	return false;
}
#endif

void GUIFormSpecMenuNew::drawMenu()
{
	if (!forms)
		return;

	gui::IGUISkin* skin = Environment->getSkin();
	sanity_check(skin != NULL);
	gui::IGUIFont *old_font = skin->getFont();
	skin->setFont(m_font);

	video::IVideoDriver* driver = Environment->getVideoDriver();

	forms->draw(driver, skin);

	skin->setFont(old_font);
}


bool GUIFormSpecMenuNew::preprocessEvent(const SEvent& event)
{
	return false;
}

/******************************************************************************/
bool GUIFormSpecMenuNew::DoubleClickDetection(const SEvent event)
{
	/* The following code is for capturing double-clicks of the mouse button
	 * and translating the double-click into an EET_KEY_INPUT_EVENT event
	 * -- which closes the form -- under some circumstances.
	 *
	 * There have been many github issues reporting this as a bug even though it
	 * was an intended feature.  For this reason, remapping the double-click as
	 * an ESC must be explicitly set when creating this class via the
	 * /p remap_dbl_click parameter of the constructor.
	 */

	if (!m_remap_dbl_click)
		return false;

	if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
		m_doubleclickdetect[0].pos  = m_doubleclickdetect[1].pos;
		m_doubleclickdetect[0].time = m_doubleclickdetect[1].time;

		m_doubleclickdetect[1].pos  = m_pointer;
		m_doubleclickdetect[1].time = porting::getTimeMs();
	}
	else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) {
		u64 delta = porting::getDeltaMs(m_doubleclickdetect[0].time, porting::getTimeMs());
		if (delta > 400) {
			return false;
		}

		double squaredistance =
				m_doubleclickdetect[0].pos
				.getDistanceFromSQ(m_doubleclickdetect[1].pos);

		if (squaredistance > (30*30)) {
			return false;
		}

		SEvent translated;
		//translate doubleclick to escape
		memset(&translated, 0, sizeof(SEvent));
		translated.EventType = irr::EET_KEY_INPUT_EVENT;
		translated.KeyInput.Key         = KEY_ESCAPE;
		translated.KeyInput.Control     = false;
		translated.KeyInput.Shift       = false;
		translated.KeyInput.PressedDown = true;
		translated.KeyInput.Char        = 0;
		OnEvent(translated);

		// no need to send the key up event as we're already deleted
		// and no one else did notice this event
		return true;
	}

	return false;
}

void GUIFormSpecMenuNew::tryClose()
{
	if (m_allowclose) {
		quitMenu();
	} else {
	}
}

bool GUIFormSpecMenuNew::OnEvent(const SEvent& event)
{
	if (event.EventType==EET_KEY_INPUT_EVENT) {
		KeyPress kp(event.KeyInput);
		if (event.KeyInput.PressedDown && (
				(kp == EscapeKey) || (kp == CancelKey) ||
				((m_client != NULL) && (kp == getKeySetting("keymap_inventory"))))) {
			tryClose();
			return true;
		}

		if (m_client != NULL && event.KeyInput.PressedDown &&
				(kp == getKeySetting("keymap_screenshot"))) {
			m_client->makeScreenshot();
		}
		if (event.KeyInput.PressedDown) {
			if (focused) {
				// let the current seleceted menu element deal with it
				focused->keyDown(event.KeyInput);
			}
		}
		return true;
	} else if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == gui::EGET_ELEMENT_FOCUS_LOST
				&& isVisible()) {
			if (!canTakeFocus(event.GUIEvent.Element)) {
				infostream<<"GUIFormSpecMenuNew: Not allowing focus change."
						<<std::endl;
				// Returning true disables focus change
				return true;
			}
		}
	} else if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		// make sure we always have the current hovered element
		m_pointer = v2s32(event.MouseInput.X, event.MouseInput.Y);
		hover(forms->getElementAtPos(m_pointer));

		switch(event.MouseInput.Event) {
		case EMIE_MOUSE_MOVED:
			break;
		case EMIE_LMOUSE_PRESSED_DOWN:
			if (clicked)
				// make sure to always release current clicked elements,
				// even if we missed the mouse up event
				clicked->mouseUp(m_pointer);
			clicked = hovered;
			if (clicked)
				clicked->mouseDown(m_pointer);
			if (focused)
				focused->focusChange(false);
			focused = clicked;
			if (focused)
				focused->focusChange(true);
			break;
		case EMIE_LMOUSE_LEFT_UP:
			if (clicked)
				clicked->mouseUp(m_pointer);
			clicked = nullptr;
		default:
			// ignore everything else
			break;
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}
void GUIFormSpecMenuNew::hover(GUIFormSpecMenuElement *element) {
		if (hovered) {
			if (hovered == element)
				return;
			hovered->hover(false);
		}
		hovered = element;
		if (hovered)
			hovered->hover(true);
	}

