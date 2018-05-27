/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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
#include "constants.h"
#include "gamedef.h"
#include "keycode.h"
#include "util/strfnd.h"
#include <IGUIFont.h>
#include "client/renderingengine.h"
#include "log.h"
#include "client/tile.h" // ITextureSource
#include "gettime.h"
#include "gettext.h"
#include "mainmenumanager.h"
#include "porting.h"
#include "settings.h"
#include "client.h"
#include "fontengine.h"
#include "util/hex.h"
#include "util/numeric.h"
#include "util/string.h" // for parseColorString()
#include "guiscalingfilter.h"

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
	m_formspec_string(source),
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
	}
}

void GUIFormSpecMenuElement::setText(const std::string &t) {
	text = std::unique_ptr<TextSpec>(new TextSpec(t));
}


void TextSpec::addLine(const core::rect<s32> &arect, v2s32 pos,
		const core::dimension2d<u32> &dim, const core::stringw &line)
{
	if (alignment & FORMSPEC_TEXT_ALIGN_RIGHT) {
		pos.X = arect.LowerRightCorner.X - dim.Width;
	} else if (!(alignment & FORMSPEC_TEXT_ALIGN_LEFT)) {
		s32 offset = arect.getWidth() - dim.Width;
		pos.X += offset / 2;
	}
	lines.emplace_back(std::pair<core::stringw, core::rect<s32>>(line, core::rect<s32>(pos, dim)));
}

void TextSpec::rebuild(const core::rect<s32> &arect, gui::IGUIFont *font, StyleSpec *style) {
	lines.clear();
	u32 width = arect.getWidth();
	v2s32 pos = arect.UpperLeftCorner;

	alignment = style->getTextAlign();

	std::vector<std::string> raw_lines = split(text, '\n');
	for (const std::string &l : raw_lines) {
		std::wstring lw = utf8_to_wide(unescape_string(l));
		core::stringw lw_i(lw.c_str(), lw.size());
		core::dimension2d<u32> dim = font->getDimension(lw.c_str());
		if (dim.Width > width) {
			// word wrapping
			std::vector<core::stringw> words;
			lw_i.split(words, L" ", 1, true, true);
			core::stringw split_line;
			u32 line_width = 0;
			for (const core::stringw &w : words) {
				core::dimension2d<u32> word_dim = font->getDimension(w.c_str());
				line_width += word_dim.Width;
				if (line_width > width) {
					bool empty_line = split_line.size() == 0;
					// split line
					if (empty_line) {
						// don't create empty lines
						split_line = w;
						line_width = 0;
					}

					split_line.trim(L" ");
					dim = font->getDimension(split_line.c_str());
					addLine(arect, pos, dim, split_line);
					if (!empty_line) {
						split_line = w;
						line_width = word_dim.Width;
					}
					pos.Y += dim.Height;
				} else {
					split_line += w;
				}

			}
			split_line.trim(L" ");
			dim = font->getDimension(split_line.c_str());
			addLine(arect, pos, dim, split_line);
			pos.Y += dim.Height;
		} else {
			addLine(arect, pos, dim, lw_i);
			pos.Y += dim.Height;
		}
	}

	// adjust vertical position of the lines
	if (alignment & FORMSPEC_TEXT_ALIGN_BOTTOM) {
		s32 y = arect.LowerRightCorner.Y;
		for (auto line_it = lines.rbegin(); line_it != lines.rend(); ++line_it) {
			s32 height = line_it->second.getHeight();
			line_it->second.LowerRightCorner.Y = y;
			y -= height;
			line_it->second.UpperLeftCorner.Y = y;
		}
	} else if (!(alignment & FORMSPEC_TEXT_ALIGN_TOP)) {
		if (lines.size() >= 1) {
			s32 height = arect.getHeight();
			s32 textHeight = 0;
			textHeight = lines.back().second.LowerRightCorner.Y;
			textHeight -= lines[0].second.UpperLeftCorner.Y;
			s32 offset = (height - textHeight) / 2;
			for (auto &line : lines) {
				line.second.UpperLeftCorner.Y += offset;
				line.second.LowerRightCorner.Y += offset;
			}
		}
	}
}

void GUIFormSpecMenuElementInventory::rebuild(const core::rect<s32> &parent_rect, gui::IGUIFont *font) {
	// update base
	GUIFormSpecMenuElement::rebuild(parent_rect, font);

	float itemHeight = 4. * arect.getHeight() / (5. * y - 1.);
	float itemWidth = 13. * arect.getWidth() / (15. * x - 2.);

	if (itemHeight > itemWidth) {
		itemSize.Width = itemWidth;
		itemSize.Height = itemWidth;
		padding.X = 2 * itemWidth / 13.f;
		u16 numPads = y > 1 ? y - 1 : 2;
		padding.Y = (arect.getHeight() - y * itemSize.Height) / numPads;
	} else {
		itemSize.Width = itemHeight;
		itemSize.Height = itemHeight;
		u16 numPads = x > 1 ? x - 1 : 2;
		padding.X = (arect.getWidth() - x * itemSize.Width) / numPads;
		padding.Y = itemHeight / 4.f;
	}
}

void GUIFormSpecMenuElement::rebuild(const core::rect<s32> &parent_rect, gui::IGUIFont *font) {
	const s32 width = parent_rect.getWidth();
	const s32 height = parent_rect.getHeight();

	arect = core::rect<s32>(width * rect.UpperLeftCorner.X + parent_rect.UpperLeftCorner.X,
				height * rect.UpperLeftCorner.Y + parent_rect.UpperLeftCorner.Y,
				width * rect.LowerRightCorner.X + parent_rect.UpperLeftCorner.X,
				height * rect.LowerRightCorner.Y + parent_rect.UpperLeftCorner.Y);

	// adjust aspect ratio
	if (aspect != 0.f) {
		float actualAspect = static_cast<float>(arect.getWidth()) / static_cast<float>(arect.getHeight());
		if (actualAspect > aspect) {
			s32 newWidth = aspect * arect.getHeight();
			s32 offset = (arect.getWidth() - newWidth) / 2;
			arect.UpperLeftCorner.X += offset;
			arect.LowerRightCorner.X -= offset;
		} else if (actualAspect < aspect) {
			s32 newHeight = arect.getWidth() / aspect;
			s32 offset = (arect.getHeight() - newHeight) / 2;
			arect.UpperLeftCorner.Y += offset;
			arect.LowerRightCorner.Y -= offset;
		}
	}

	for (const std::unique_ptr<GUIFormSpecMenuElement> &e : children) {
		e->rebuild(arect, font);
	}

	if (text)
		text->rebuild(arect, font, styleSpec.get());
}

static void parseBeginRect(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack) {
	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	stack.emplace(std::unique_ptr<GUIFormSpecMenuElement>(new GUIFormSpecMenuElement(parent->getStyle())));

	std::vector<std::string> parts = split(description, ',');
	if (parts.size() < 4) {
		errorstream << "Invalid beginrect element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}
	core::rect<float> dimensions = core::rect<float>(
			stof(parts[0]), stof(parts[1]),
			stof(parts[2]), stof(parts[3]));

	stack.top()->setDimensions(dimensions);
}

static void parseEndRect(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack) {
	if (stack.size() <= 1) {
		errorstream << "Too many endrect[] elements." << std::endl;
	} else {
		std::unique_ptr<GUIFormSpecMenuElement> child = std::move(stack.top());
		stack.pop();
		stack.top()->addChild(child);
	}
}

static void parseBGColor(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>>  &stack) {
	std::vector<std::string> parts = split(description, ',');
	if (parts.size() < 1) {
		errorstream << "Invalid bgcolor element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}

	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	video::SColor color;
	if (parseColorString(parts[0], color, false)) {
		parent->setBGColor(color);
	}
}

static void parseInventory(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>>  &stack,
		Client *client, InventoryManager *invmgr) {
	std::vector<std::string> parts = split(description, ',');
	if (parts.size() < 5) {
		errorstream << "Invalid inventory element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}

	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	if (parent->isRect()) {
		std::unique_ptr<GUIFormSpecMenuElementInventory> inventory =
			std::unique_ptr<GUIFormSpecMenuElementInventory>(new GUIFormSpecMenuElementInventory(std::move(*parent.get())));
		inventory->setList(invmgr, parts[0], parts[1], client);

		u16 x = stoi(parts[2]);
		u16 y = stoi(parts[3]);

		inventory->setInventoryDimensions(x, y);
		parent = std::move(inventory);
	} else {
		errorstream << "Attempt to create more than one modifier." << std::endl;
	}
}

static void parseButton(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>>  &stack) {
	std::vector<std::string> parts = split(description, ',');
	if (parts.size() < 1) {
		errorstream << "Invalid button element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}

	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	if (parent->isRect()) {
		std::unique_ptr<GUIFormSpecMenuElementButton> button =
			std::unique_ptr<GUIFormSpecMenuElementButton>(new GUIFormSpecMenuElementButton(std::move(*parent.get())));
		parent = std::move(button);
	} else {
		errorstream << "Attempt to create more than one modifier." << std::endl;
	}
}

static void parseText(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>>  &stack) {
	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	parent->setText(description);
}

static void parseImage(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack,
		ISimpleTextureSource *tsrc)
{
	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	video::ITexture *tex = tsrc->getTexture(description);
	parent->setImage(tex);
}


static void parseAspect(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack) {
	std::vector<std::string> parts = split(description, ',');
	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	if (parts.size() != 2) {
		errorstream << "Invalid aspect element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}
	float aspect = stof(parts[0]) / stof(parts[1]);
	parent->setAspect(aspect);
}

static void parseStyle(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack,
		ISimpleTextureSource *tsrc)
{
	std::vector<std::string> parts = split(description, ',');
	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	if (parts.size() < 2) {
		errorstream << "Invalid style element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}
	std::shared_ptr<StyleSpec> newStyle = std::shared_ptr<StyleSpec>(new StyleSpec(*(parent->getStyle())));
	bool changed = false;
	if (parts[0] == "button_standard") {
		video::ITexture *tex = nullptr;
		if (!parts[1].empty())
			tex = tsrc->getTexture(parts[1]);
		newStyle->setButtonStandard(tex);
		changed = true;
	} else if (parts[0] == "button_hover") {
		video::ITexture *tex = nullptr;
		if (!parts[1].empty())
			tex = tsrc->getTexture(parts[1]);
		newStyle->setButtonHover(tex);
		changed = true;
	} else if (parts[0] == "button_pressed") {
		video::ITexture *tex = nullptr;
		if (!parts[1].empty())
			tex = tsrc->getTexture(parts[1]);
		newStyle->setButtonPressed(tex);
		changed = true;
	} else if (parts[0] == "text_align") {
		changed = true;
		if (parts[1] == "top") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_TOP);
		} else if (parts[1] == "topright") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_TOP | FORMSPEC_TEXT_ALIGN_RIGHT);
		} else if (parts[1] == "right") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_RIGHT);
		} else if (parts[1] == "bottomright") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_BOTTOM | FORMSPEC_TEXT_ALIGN_RIGHT);
		} else if (parts[1] == "bottom") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_BOTTOM);
		} else if (parts[1] == "bottomleft") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_BOTTOM | FORMSPEC_TEXT_ALIGN_LEFT);
		} else if (parts[1] == "left") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_LEFT);
		} else if (parts[1] == "topleft") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_TOP | FORMSPEC_TEXT_ALIGN_LEFT);
		} else if (parts[1] == "center") {
			newStyle->setTextAlign(FORMSPEC_TEXT_ALIGN_CENTER);
		}
	} else if (parts[0] == "inventory_background_color") {
		video::SColor color;
		if (parseColorString(parts[1], color, false)) {
			changed = true;
			newStyle->setInventoryBGColor(color);
		}
	} else if (parts[0] == "inventory_border_color") {
		video::SColor color;
		if (parseColorString(parts[1], color, false)) {
			changed = true;
			newStyle->setInventoryBorderColor(color);
		}
	} else if (parts[0] == "inventory_border_width") {
		changed = true;
		newStyle->setInventoryBorder(stoi(parts[1]));
	}



	if (changed) {
		parent->setStyle(newStyle);
	}
}

static std::unordered_map<std::string, FormSpecElementType> formSpecElementTypes = {
	{ "beginrect", ELEMENT_BEGINRECT },
	{ "endrect", ELEMENT_ENDRECT },
	{ "bgcolor", ELEMENT_BGCOLOR },
	{ "inventory", ELEMENT_INVENTORY },
	{ "button", ELEMENT_BUTTON },
	{ "text", ELEMENT_TEXT },
	{ "image", ELEMENT_IMAGE },
	{ "aspect", ELEMENT_ASPECT },
	{ "style", ELEMENT_STYLE }
};

static void parseElement(const std::string &element, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack,
		ISimpleTextureSource *tsrc, Client *client, InventoryManager *invmgr)
{
	//some prechecks
	if (element.empty())
		return;

	std::vector<std::string> parts = split(element,'[');

	// ugly workaround to keep compatibility
	if (parts.size() > 2) {
		if (trim(parts[0]) == "image") {
			for (unsigned int i=2;i< parts.size(); i++) {
				parts[1] += "[" + parts[i];
			}
		}
		else { return; }
	}

	if (parts.size() < 2) {
		return;
	}

	std::string typestring = trim(parts[0]);
	std::string description = trim(parts[1]);

	FormSpecElementType type = ELEMENT_BEGINRECT;

	try {
		type = formSpecElementTypes.at(typestring);
	} catch (const std::out_of_range &e) {
		warningstream << "Unknown DrawSpec: type=" << typestring << ", data=\"" << description << "\""
			<< std::endl;
		return;
	}

	switch (type) {
		case ELEMENT_BEGINRECT:
			parseBeginRect(description, stack);
			break;
		case ELEMENT_ENDRECT:
			parseEndRect(description, stack);
			break;
		case ELEMENT_BGCOLOR:
			parseBGColor(description, stack);
			break;
		case ELEMENT_INVENTORY:
			parseInventory(description, stack, client, invmgr);
			break;
		case ELEMENT_BUTTON:
			parseButton(description, stack);
			break;
		case ELEMENT_TEXT:
			parseText(description, stack);
			break;
		case ELEMENT_IMAGE:
			parseImage(description, stack, tsrc);
			break;
		case ELEMENT_ASPECT:
			parseAspect(description, stack);
			break;
		case ELEMENT_STYLE:
			parseStyle(description, stack, tsrc);
			break;
	}

	return;
}

void GUIFormSpecMenuNew::regenerateGui(v2u32 screensize)
{
	/* useless to regenerate without a screensize */
	if ((screensize.X <= 0) || (screensize.Y <= 0)) {
		return;
	}

	std::vector<std::string> elements = split(m_formspec_string,']');
	std::stack<std::unique_ptr<GUIFormSpecMenuElement>> form_stack;

	// insert start proxy
	defaultStyle = std::shared_ptr<StyleSpec>(new StyleSpec());
	form_stack.emplace(new GUIFormSpecMenuElement(defaultStyle));
	for (const std::string &element : elements) {
		parseElement(element, form_stack, m_tsrc, m_client, m_invmgr);
	}
	if (form_stack.size() != 1) {
		errorstream << "Mismatch of beginrect and endrect tags. Dropping formspec." << std::endl;
		forms = nullptr;
		quitMenu();
		return;
	}
	forms = std::move(form_stack.top());
	form_stack.pop();
	const std::vector<std::unique_ptr<GUIFormSpecMenuElement>> &children = forms->getChildren();
	if (children.size() < 1) {
		errorstream << "No windows defined. Dropping formspec." << std::endl;
		forms = nullptr;
		quitMenu();
		return;
	}

	m_font = g_fontengine->getFont();

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


// image drawing helper
static void draw2DImage(video::IVideoDriver *driver, video::ITexture *image, const core::rect<s32> &arect) {
	const video::SColor color(255,255,255,255);
	const video::SColor colors[] = {color,color,color,color};

	draw2DImageFilterScaled(driver, image, arect,
		core::rect<s32>(core::position2d<s32>(0,0),
				core::dimension2di(image->getOriginalSize())),
		nullptr/*&AbsoluteClippingRect*/, colors, true);
}


// style drawing
void StyleSpec::drawButtonStandard(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUISkin *skin) {
	if (buttonStandard)
		draw2DImage(driver, buttonStandard, arect);
	else
		skin->draw3DButtonPaneStandard(nullptr, arect, nullptr);
}
void StyleSpec::drawButtonHover(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUISkin *skin) {
	if (buttonHover)
		draw2DImage(driver, buttonHover, arect);
	else
		drawButtonStandard(arect, driver, skin);
}
void StyleSpec::drawButtonPressed(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUISkin *skin) {
	if (buttonPressed)
		draw2DImage(driver, buttonPressed, arect);
	else
		skin->draw3DButtonPanePressed(nullptr, arect, nullptr);
}

void StyleSpec::drawInventorySlot(video::IVideoDriver *driver, gui::IGUISkin *skin, const ItemStack &item,
		core::rect<s32> arect, Client *client, ItemRotationKind rot)
{
	// shrink the slot to fit the border
	if (border) {
		arect.UpperLeftCorner.X += border;
		arect.UpperLeftCorner.Y += border;
		arect.LowerRightCorner.X -= border;
		arect.LowerRightCorner.Y -= border;
	}

	// draw the background
	driver->draw2DRectangle(inventoryBGColor, arect, nullptr);
	
	// draw the border
	if (border) {
		s32 x1 = arect.UpperLeftCorner.X;
		s32 y1 = arect.UpperLeftCorner.Y;
		s32 x2 = arect.LowerRightCorner.X;
		s32 y2 = arect.LowerRightCorner.Y;
		driver->draw2DRectangle(inventoryBorderColor,
			core::rect<s32>(v2s32(x1 - border, y1 - border), v2s32(x2 + border, y1)), nullptr); //top
		driver->draw2DRectangle(inventoryBorderColor,
			core::rect<s32>(v2s32(x1 - border, y2), v2s32(x2 + border, y2 + border)), nullptr);
		driver->draw2DRectangle(inventoryBorderColor,
			core::rect<s32>(v2s32(x1 - border, y1), v2s32(x1, y2)), nullptr);
		driver->draw2DRectangle(inventoryBorderColor,
			core::rect<s32>(v2s32(x2, y1), v2s32(x2 + border, y2)), nullptr);
	}

	// draw the item
	if (!item.empty()) {
		drawItemStack(driver, skin->getFont(), item,
			arect, nullptr, client,
			rot);
	}
}


// draw text
void TextSpec::draw(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUIFont *font) {
	for (const std::pair<core::stringw, core::rect<s32>> &line : lines) {
		font->draw(line.first, line.second, video::SColor(0xFFFFFFFF));
	}
}

void GUIFormSpecMenuElement::drawText(video::IVideoDriver *driver, gui::IGUIFont *font) {
	if (text) {
		text->draw(arect, driver, font);
	}
}

// draw a regular rectangle
void GUIFormSpecMenuElement::drawChildren(video::IVideoDriver *driver, gui::IGUISkin *skin) {
	for (const std::unique_ptr<GUIFormSpecMenuElement> &e : children) {
		e->draw(driver, skin);
	}
}
void GUIFormSpecMenuElement::draw(video::IVideoDriver *driver, gui::IGUISkin* skin) {
	if (style & ELEMENT_STYLE_BGCOLOR) {
		driver->draw2DRectangle(bg, arect, nullptr);
	}
	if (style & ELEMENT_STYLE_IMAGE) {
		draw2DImage(driver, image, arect);
	}

	// draw the children
	drawChildren(driver, skin);
	
	// draw text
	drawText(driver, skin->getFont());
}

// draw a button
void GUIFormSpecMenuElementButton::draw(video::IVideoDriver *driver, gui::IGUISkin* skin) {
	if (clicked)
		styleSpec->drawButtonPressed(arect, driver, skin);
	else if (hovered)
		styleSpec->drawButtonHover(arect, driver, skin);
	else
		styleSpec->drawButtonStandard(arect, driver, skin);

	// draw the children
	drawChildren(driver, skin);
	
	// draw text
	drawText(driver, skin->getFont());
}

// draw an inventory
void GUIFormSpecMenuElementInventory::draw(video::IVideoDriver *driver, gui::IGUISkin* skin) {
	// draw the background etc.
	GUIFormSpecMenuElement::draw(driver, skin);

	if (!client) 
		return;

	InventoryLocation loc;
	loc.deSerialize(location);

	Inventory *inv = invmgr->getInventory(loc);
	if (!inv) {
		warningstream << "GUIFormSpecMenuElementInventory::draw(): "
				<< "The inventory location "
				<< "\"" << location << "\" doesn't exist"
				<< std::endl;
		return;
	}
	InventoryList *list = inv->getList(listname);
	if (!list) {
		warningstream << "GUIFormSpecMenuElementInventory::draw(): "
				<< "The inventory list "
				<< "\"" << listname << "\" doesn't exist"
				<< std::endl;
		return;
	}


	v2f pos(arect.UpperLeftCorner.X, arect.UpperLeftCorner.Y);
	bool finished = false;
	for (s32 i = 0; i < y; ++i) {
		// reset column
		pos.X = arect.UpperLeftCorner.X;
		for (s32 j = 0; j < x; ++j) {
			s32 item_i = i * x + j;
			if (item_i >= (s32)list->getSize()) {
				finished = true;
				break;
			}

			core::rect<s32> r(pos.X, pos.Y,
					pos.X + itemSize.Width,
			       		pos.Y + itemSize.Height);
			ItemStack item = list->getItem(item_i);

			styleSpec->drawInventorySlot(driver, skin, item, r, client, IT_ROT_SELECTED);
						pos.X += itemSize.Width + padding.X;
		}
		if (finished)
			break;
		pos.Y += itemSize.Height + padding.Y;
	}
}

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

enum ButtonEventType : u8
{
	BET_LEFT,
	BET_RIGHT,
	BET_MIDDLE,
	BET_WHEEL_UP,
	BET_WHEEL_DOWN,
	BET_UP,
	BET_DOWN,
	BET_MOVE,
	BET_OTHER
};

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

