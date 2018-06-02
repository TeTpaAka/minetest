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

#include "guiFormSpecParser.h"
#include "guiFormSpecMenuElement.h"
#include "log.h"
#include "util/string.h" // for parseColorString()

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

static void parseInput(const std::string &description, std::stack<std::unique_ptr<GUIFormSpecMenuElement>>  &stack) {
	std::vector<std::string> parts = split(description, ',');
	if (parts.size() < 1) {
		errorstream << "Invalid input element (" << parts.size() << "): '" << description << "'" << std::endl;
		return;
	}

	std::unique_ptr<GUIFormSpecMenuElement> &parent = stack.top();
	if (parent->isRect()) {
		std::unique_ptr<GUIFormSpecMenuElementInput> input =
			std::unique_ptr<GUIFormSpecMenuElementInput>(new GUIFormSpecMenuElementInput(std::move(*parent.get())));
		parent = std::move(input);
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
	{ "input", ELEMENT_INPUT },
	{ "text", ELEMENT_TEXT },
	{ "image", ELEMENT_IMAGE },
	{ "aspect", ELEMENT_ASPECT },
	{ "style", ELEMENT_STYLE }
};

void GUIFormSpecParser::parseElement(const std::string &element, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack,
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
		case ELEMENT_INPUT:
			parseInput(description, stack);
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
