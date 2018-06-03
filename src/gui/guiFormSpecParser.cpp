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

static void parseStyle(const Json::Value &jsonElem, std::shared_ptr<StyleSpec> &style, ISimpleTextureSource *tsrc)
{
	Json::Value jsonStyle = jsonElem["style"];

	if (jsonStyle.isObject()) {
		style = std::shared_ptr<StyleSpec>(new StyleSpec(*style));
		if (jsonStyle.isMember("button_standard")) {
			video::ITexture *tex = nullptr;
			if (jsonStyle["button_standard"].isString())
				tex = tsrc->getTexture(jsonStyle["button_standard"].asString());
			style->setButtonStandard(tex);
		}
		if (jsonStyle.isMember("button_hover")) {
			video::ITexture *tex = nullptr;
			if (jsonStyle["button_hover"].isString())
				tex = tsrc->getTexture(jsonStyle["button_hover"].asString());
			style->setButtonHover(tex);
		}
		if (jsonStyle.isMember("button_pressed")) {
			video::ITexture *tex = nullptr;
			if (jsonStyle["button_pressed"].isString())
				tex = tsrc->getTexture(jsonStyle["button_pressed"].asString());
			style->setButtonPressed(tex);
		}
		if (jsonStyle.isMember("text_align")) {
			std::string alignment = "center";
			if (jsonStyle["text_align"].isString())
				alignment = jsonStyle["text_align"].asString();
			if (alignment == "top") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_TOP);
			} else if (alignment == "topright") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_TOP | FORMSPEC_TEXT_ALIGN_RIGHT);
			} else if (alignment == "right") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_RIGHT);
			} else if (alignment == "bottomright") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_BOTTOM | FORMSPEC_TEXT_ALIGN_RIGHT);
			} else if (alignment == "bottom") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_BOTTOM);
			} else if (alignment == "bottomleft") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_BOTTOM | FORMSPEC_TEXT_ALIGN_LEFT);
			} else if (alignment == "left") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_LEFT);
			} else if (alignment == "topleft") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_TOP | FORMSPEC_TEXT_ALIGN_LEFT);
			} else if (alignment == "center") {
				style->setTextAlign(FORMSPEC_TEXT_ALIGN_CENTER);
			}
		}
		if (jsonStyle.isMember("inventory_background_color")) {
			video::SColor color;
			if (jsonStyle["inventory_background_color"].isString()) {
				if (parseColorString(jsonStyle["inventory_background_color"].asString(), color, false)) {
					style->setInventoryBGColor(color);
				}
			}
		}
		if (jsonStyle.isMember("inventory_border_color")) {
			video::SColor color;
			if (jsonStyle["inventory_border_color"].isString()) {
				if (parseColorString(jsonStyle["inventory_border_color"].asString(), color, false)) {
					style->setInventoryBGColor(color);
				}
			}
		}
		if (jsonStyle.isMember("inventory_border_width")) {
			if (jsonStyle["inventory_border_width"].isInt()) {
				style->setInventoryBorder(jsonStyle["inventory_border_width"].asInt());
			}
		}
	} else if (!jsonStyle.isNull()) {
		warningstream << "The style member of a formspec element has to be an object." << std::endl;
	}
}

static FormSpecElementType parseType(const Json::Value &jsonElem)
{
	Json::Value jsonType = jsonElem["type"];

	FormSpecElementType type = ELEMENT_RECT;

	if (jsonType.isString()) {
		std::string typeString = jsonType.asString();
		if (typeString == "inventory")
			type = ELEMENT_INVENTORY;
		else if (typeString == "button")
			type = ELEMENT_BUTTON;
		else if (typeString == "input")
			type = ELEMENT_INPUT;
		else
			warningstream << "Unknow formspec element \"" << typeString << "\"" << std::endl;
	} else if (!jsonType.isNull()) {
		warningstream << "The type member of a formspec element has to be a string." << std::endl;
	}
	return type;
}

static void parseRect(const Json::Value &jsonElem, GUIFormSpecMenuElement *elem) {
	Json::Value jsonRect = jsonElem["rect"];
	core::rect<float> rect = { 0.f, 0.f, 1.f, 1.f };
	if (jsonRect.isObject()) {
		if (jsonRect.isMember("x0") && jsonRect["x0"].isNumeric()) {
			rect.UpperLeftCorner.X = jsonRect["x0"].asFloat();
		}
		if (jsonRect.isMember("y0") && jsonRect["y0"].isNumeric()) {
			rect.UpperLeftCorner.Y = jsonRect["y0"].asFloat();
		}
		if (jsonRect.isMember("x1") && jsonRect["x1"].isNumeric()) {
			rect.LowerRightCorner.X = jsonRect["x1"].asFloat();
		}
		if (jsonRect.isMember("y1") && jsonRect["y1"].isNumeric()) {
			rect.LowerRightCorner.Y = jsonRect["y1"].asFloat();
		}
	} else if (!jsonRect.isNull()) {
		warningstream << "The rect member of a formspec element has to be an object." << std::endl;
	}
	elem->setDimensions(rect);
}

static void parseAspect(const Json::Value &jsonElem, GUIFormSpecMenuElement *elem) {
	Json::Value jsonAspect = jsonElem["aspect"];
	if (jsonAspect.isObject()) {
		if (jsonAspect.isMember("x") && jsonAspect.isMember("y") &&
			jsonAspect["x"].isNumeric() && jsonAspect["y"].isNumeric()) {
			float x, y;
			x = jsonAspect["x"].asFloat();
			y = jsonAspect["y"].asFloat();
			elem->setAspect(x/y);
		} else {
			warningstream << "Invalid aspect member in formspec element." << std::endl;
		}
	} else if (!jsonAspect.isNull()) {
		warningstream << "The aspect member of a formspec element has to be an object." << std::endl;
	}
}

static void parseBGColor(const Json::Value &jsonElem, GUIFormSpecMenuElement *elem) {
	Json::Value jsonBGColor = jsonElem["bgcolor"];
	if (jsonBGColor.isString()) {
		video::SColor color;
		if (parseColorString(jsonBGColor.asString(), color, false)) {
			elem->setBGColor(color);
		}
	} else if (!jsonBGColor.isNull()) {
		warningstream << "The bgcolor member of a formspec element has to be a string." << std::endl;
	}
}

static void parseImage(const Json::Value &jsonElem, GUIFormSpecMenuElement *elem, ISimpleTextureSource *tsrc) {
	Json::Value jsonImage = jsonElem["image"];
	if (jsonImage.isString()) {
		video::ITexture *tex = tsrc->getTexture(jsonImage.asString());
		elem->setImage(tex);
	} else if (!jsonImage.isNull()) {
		warningstream << "The image member of a formspec element has to be a string." << std::endl;
	}
}

static void parseText(const Json::Value &jsonElem, GUIFormSpecMenuElement *elem) {
	Json::Value jsonText = jsonElem["text"];
	if (jsonText.isString()) {
		elem->setText(jsonText.asString());
	} else if (!jsonText.isNull()) {
		warningstream << "The text member of a formspec element has to be a string." << std::endl;
	}
}

static void parseInventory(const Json::Value &jsonElem, GUIFormSpecMenuElementInventory *elem,
		InventoryManager *invmgr, Client *client)
{
	u16 columns, rows = 1;
	Json::Value val = jsonElem["columns"];
	if (val.isInt()) {
		columns = val.asInt();
	} else if (!val.isNull()) {
		warningstream << "The columns member of a formspec element has to be an integer." << std::endl;
	}
	val = jsonElem["rows"];
	if (val.isInt()) {
		rows = val.asInt();
	} else if (!val.isNull()) {
		warningstream << "The rows member of a formspec element has to be an integer." << std::endl;
	}
	elem->setInventoryDimensions(columns, rows);

	std::string location = "current_player";
	std::string list = "main";
	val = jsonElem["location"];
	if (val.isString()) {
		location = val.asString();
	} else if (!val.isNull()) {
		warningstream << "The location member of a formspec element has to be a string." << std::endl;
	}
	val = jsonElem["list"];
	if (val.isString()) {
		list = val.asString();
	} else if (!val.isNull()) {
		warningstream << "The list member of a formspec element has to be a string." << std::endl;
	}
	elem->setList(invmgr, location, list, client);
}

// forward decleration
static void parseElement(const Json::Value &jsonElem, GUIFormSpecMenuElement *parent, ISimpleTextureSource *tsrc,
		InventoryManager *invmgr, Client *client);
static void parseChildren(const Json::Value &jsonElem, GUIFormSpecMenuElement *parent, ISimpleTextureSource *tsrc,
		InventoryManager *invmgr, Client *client)
{
	Json::Value jsonChildren = jsonElem["children"];
	if (jsonChildren.isArray()) {
		Json::ArrayIndex size = jsonChildren.size();
		for (Json::ArrayIndex i = 0; i < size; ++i) {
			parseElement(jsonChildren[i], parent, tsrc, invmgr, client);
		}
	} else if (!jsonChildren.isNull()) {
		warningstream << "The children member of a formspec element has to be an array." << std::endl;
	}
}

static void parseElement(const Json::Value &jsonElem, GUIFormSpecMenuElement *parent, ISimpleTextureSource *tsrc,
		InventoryManager *invmgr, Client *client)
{
	if (!jsonElem.isObject()) {
		warningstream << "Formspec elements have to be objects." << std::endl;
		return;
	}
	std::shared_ptr<StyleSpec> style = parent->getStyle();
	parseStyle(jsonElem, style, tsrc);

	FormSpecElementType type = parseType(jsonElem);
	std::unique_ptr<GUIFormSpecMenuElement> formSpecElement;
	switch (type) {
		case ELEMENT_INVENTORY:
			formSpecElement = std::unique_ptr<GUIFormSpecMenuElement>(new GUIFormSpecMenuElementInventory(style));
			parseInventory(jsonElem, dynamic_cast<GUIFormSpecMenuElementInventory *>(formSpecElement.get()),
					invmgr, client);
			break;
		case ELEMENT_BUTTON:
			formSpecElement = std::unique_ptr<GUIFormSpecMenuElement>(new GUIFormSpecMenuElementButton(style));
			break;
		case ELEMENT_INPUT:
			formSpecElement = std::unique_ptr<GUIFormSpecMenuElement>(new GUIFormSpecMenuElementInput(style));
			break;
		default:
			formSpecElement = std::unique_ptr<GUIFormSpecMenuElement>(new GUIFormSpecMenuElement(style));
			break;
	}

	parseRect(jsonElem, formSpecElement.get());
	parseAspect(jsonElem, formSpecElement.get());
	parseBGColor(jsonElem, formSpecElement.get());
	parseText(jsonElem, formSpecElement.get());
	parseImage(jsonElem, formSpecElement.get(), tsrc);
	parseChildren(jsonElem, formSpecElement.get(), tsrc, invmgr, client);

	parent->addChild(formSpecElement);
}

std::unique_ptr<GUIFormSpecMenuElement> GUIFormSpecParser::parse(const std::string &formspec, ISimpleTextureSource *tsrc,
	Client *client, InventoryManager *invmgr, const std::shared_ptr<StyleSpec> &style)
{
	std::unique_ptr<GUIFormSpecMenuElement> main = std::unique_ptr<GUIFormSpecMenuElement>(new GUIFormSpecMenuElement(style));
	GUIFormSpecMenuElement *parent = main.get();

	Json::Value root;
	{
		// parse formspec
		std::istringstream iss(formspec);
		iss >> root;
	}
	parseElement(root, parent, tsrc, invmgr, client);

	return main;
}
