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
#include "client/joystick_controller.h"
#include "client/hud.h" // drawItemStack

class InventoryManager;
class ISimpleTextureSource;
class Client;

typedef enum {
	ELEMENT_BEGINRECT,
	ELEMENT_ENDRECT,
	ELEMENT_BGCOLOR,
	ELEMENT_INVENTORY,
	ELEMENT_BUTTON,
	ELEMENT_INPUT,
	ELEMENT_TEXT,
	ELEMENT_IMAGE,
	ELEMENT_ASPECT,
	ELEMENT_STYLE
} FormSpecElementType;

typedef enum {
	ELEMENT_STYLE_NONE    = 0x00,
	ELEMENT_STYLE_BGCOLOR = 0x01,
	ELEMENT_STYLE_IMAGE   = 0x02
} FormSpecElementStyle;

typedef enum {
	FORMSPEC_TEXT_ALIGN_CENTER = 0x00,
	FORMSPEC_TEXT_ALIGN_LEFT = 0x01,
	FORMSPEC_TEXT_ALIGN_RIGHT = 0x02,
	FORMSPEC_TEXT_ALIGN_TOP = 0x04,
	FORMSPEC_TEXT_ALIGN_BOTTOM = 0x08
} FormspecTextAlign;

class StyleSpec {
public:
	StyleSpec(gui::IGUIFont *font) : font(font) { };
	StyleSpec(const StyleSpec &spec) :
		font(spec.font),
		buttonStandard(spec.buttonStandard),
		buttonHover(spec.buttonHover),
		buttonPressed(spec.buttonPressed),
		inventoryBGColor(spec.inventoryBGColor),
		inventoryBorderColor(spec.inventoryBorderColor),
		border(spec.border),
		align(spec.align) { }
	void drawButtonStandard(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUISkin *skin);
	void drawButtonHover(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUISkin *skin);
	void drawButtonPressed(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUISkin *skin);
	void drawInventorySlot(video::IVideoDriver *driver, gui::IGUISkin *skin, const ItemStack &item,
		core::rect<s32> arect, Client *client, ItemRotationKind rot);

	void setButtonStandard(video::ITexture *tex) { buttonStandard = tex; }
	void setButtonHover(video::ITexture *tex) { buttonHover = tex; }
	void setButtonPressed(video::ITexture *tex) { buttonPressed = tex; }
	void setInventoryBGColor(const video::SColor &color) { inventoryBGColor = color; }
	void setInventoryBorderColor(const video::SColor &color) { inventoryBorderColor = color; }
	void setInventoryBorder(s32 border) { this->border = border; }
	void setTextAlign(u8 align) { this->align = align; }

	gui::IGUIFont *getFont() const { return font; }
	u8 getTextAlign() { return align; }
private:
	gui::IGUIFont *font = nullptr;
	video::ITexture *buttonStandard = nullptr;
	video::ITexture *buttonHover = nullptr;
	video::ITexture *buttonPressed = nullptr;

	video::SColor inventoryBGColor = video::SColor(255,128,128,128);
	video::SColor inventoryBorderColor = video::SColor(200,0,0,0);
	s32 border = 1;

	u8 align = FORMSPEC_TEXT_ALIGN_CENTER;
};

class TextSpec {
public:
	TextSpec(const core::stringw &t) : text(t) { }
	void draw(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUIFont *font) const;

	void rebuild(const core::rect<s32> &arect, StyleSpec *style);
	void setCursorPos(u32 pos) { cursor_pos = pos; }
	void setCursorVisibility(const bool visibility) { cursor_visibility = visibility; }
	u32 size() const { return text.size(); }

	void set(const core::stringw &t) { text = t; }
	const core::stringw &get() const { return text; }
private:
	void addLine(const core::rect<s32> &arect, v2s32 &pos,
		const core::dimension2d<u32> &dim, const core::stringw &line);
	core::stringw text;
	std::vector<std::pair<core::stringw, core::rect<s32>>> lines;
	core::rect<s32> cursor;
	u32 cursor_pos = -1; // for input types
	u8 alignment = FORMSPEC_TEXT_ALIGN_CENTER;
	bool cursor_visibility = false;
};

class GUIFormSpecMenuElement {
public:
	GUIFormSpecMenuElement(const std::shared_ptr<StyleSpec> &style) {
		styleSpec = style;
	}
	GUIFormSpecMenuElement(GUIFormSpecMenuElement &&element) :
		rect(element.rect),
	       	bg(element.bg),
		image(element.image),
		styleSpec(std::move(element.styleSpec)),
		text(std::move(element.text)),
		children(std::move(element.children)),
       		style(element.style){ }

	void addChild(std::unique_ptr<GUIFormSpecMenuElement> &element) {
		children.push_back(std::move(element));
	}

	const std::vector<std::unique_ptr<GUIFormSpecMenuElement>> &getChildren() {
		return children;
	}
	bool hasChildren() { return children.size() > 0; }

	void setDimensions(const core::rect<float> &r) { rect = r; }
	const core::rect<float> &getDimensions() { return rect; }
	void setAspect(float aspect) { this->aspect = aspect;  }
	void setBGColor(const video::SColor &c) { bg = c; style |= ELEMENT_STYLE_BGCOLOR; }
	void setImage(video::ITexture *img) { image = img; style |= ELEMENT_STYLE_IMAGE; }
	void setText(const std::string &t);

	const std::shared_ptr<StyleSpec> getStyle() { return styleSpec; }
	void setStyle(const std::shared_ptr<StyleSpec> styleSpec) {
		this->styleSpec = styleSpec;
	}

	virtual void rebuild(const core::rect<s32> &parent_rect, gui::IGUIFont *font);

	virtual bool isRect() { return true; }

	virtual void hover(bool hovering) { }
	virtual GUIFormSpecMenuElement *getElementAtPos(const v2s32 &pos) {
		if (!arect.isPointInside(pos))
			return nullptr;
		GUIFormSpecMenuElement *element = nullptr;
		// iterate backwards to get the last drawn Element first due to layering.
		for (auto it = children.rbegin(); it != children.rend(); ++it) {
			element = (*it)->getElementAtPos(pos);
			if (element)
				return element;
		}
		return nullptr;
	}

	virtual void mouseDown(const v2s32 &pos) { }
	virtual void mouseUp(const v2s32 &pos) { }
	virtual void keyDown(const SEvent::SKeyInput &k) { }
	
	virtual void focusChange(const bool focus) { }

	virtual void draw(video::IVideoDriver *driver, gui::IGUISkin *skin);

protected:
	virtual void drawChildren(video::IVideoDriver *driver, gui::IGUISkin *skin);
	void drawText(video::IVideoDriver *driver, gui::IGUIFont *font);

	core::rect<float> rect = core::rect<float>(0.f, 0.f, 1.f, 1.f);
	core::rect<s32> arect;
	video::SColor bg;

	video::ITexture *image = nullptr;

	std::shared_ptr<StyleSpec> styleSpec;

	std::unique_ptr<TextSpec> text = nullptr;
private:
	std::vector<std::unique_ptr<GUIFormSpecMenuElement>> children;

	float aspect = 0.f;

	u8 style = ELEMENT_STYLE_NONE;
};

class GUIFormSpecMenuElementButton : public GUIFormSpecMenuElement {
public:
	GUIFormSpecMenuElementButton(GUIFormSpecMenuElement &&element) :
		GUIFormSpecMenuElement(std::move(element)) { }

	virtual void draw(video::IVideoDriver *driver, gui::IGUISkin *skin);
	virtual bool isRect() { return false; }

	virtual void hover(bool hovering) { hovered = hovering; }
	virtual GUIFormSpecMenuElement *getElementAtPos(const v2s32 &pos) {
		if (arect.isPointInside(pos))
			return this;
		return nullptr;
	}
	virtual void mouseDown(const v2s32 &pos) { clicked = true; }
	virtual void mouseUp(const v2s32 &pos) { clicked = false; }
private:
	bool hovered = false;
	bool clicked = false;
};

class GUIFormSpecMenuElementInventory : public GUIFormSpecMenuElement {
public:
	GUIFormSpecMenuElementInventory(GUIFormSpecMenuElement &&element) :
		GUIFormSpecMenuElement(std::move(element)) { }

	virtual void rebuild(const core::rect<s32> &parent_rect, gui::IGUIFont *font);
	virtual void draw(video::IVideoDriver *driver, gui::IGUISkin *skin);

	void setInventoryDimensions(u16 x, u16 y) { this->x = x; this->y = y; }
	void setList(InventoryManager *invmgr, const std::string &location, const std::string &listname, Client *client) {
		this->invmgr = invmgr;
		this->location = location;
		this->listname = listname;
		this->client = client;
	}

private:
	Client *client = nullptr;
	InventoryManager *invmgr;
	std::string location;
	std::string listname;

	u16 x, y = 1;
	v2f padding;
	core::dimension2d<float> itemSize;
};

class GUIFormSpecMenuElementInput : public GUIFormSpecMenuElement {
public:
	virtual GUIFormSpecMenuElement *getElementAtPos(const v2s32 &pos) {
		if (arect.isPointInside(pos))
			return this;
		return nullptr;
	}
	GUIFormSpecMenuElementInput(GUIFormSpecMenuElement &&element) :
		GUIFormSpecMenuElement::GUIFormSpecMenuElement(std::move(element))
	{
		// make sure we have a TextSpec
		if (!text)
			text = std::unique_ptr<TextSpec>(new TextSpec(L""));
		// set the cursor to the end
		cursor_pos = text->size();
		text->setCursorPos(cursor_pos);
	}

	virtual void focusChange(const bool focus);
	virtual void keyDown(const SEvent::SKeyInput &k);
private:
	void inputText(const core::stringw &input);

	u32 cursor_pos;
};

