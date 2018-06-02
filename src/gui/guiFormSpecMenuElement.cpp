#include "guiFormSpecMenuElement.h"

#include <cstdlib>
#include <utility>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <limits>
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
#include "guiscalingfilter.h"

void GUIFormSpecMenuElement::setText(const std::string &t) {
	std::wstring lw = utf8_to_wide(unescape_string(t));
	text = std::unique_ptr<TextSpec>(new TextSpec(core::stringw(lw.c_str(), lw.size())));
}


void TextSpec::addLine(const core::rect<s32> &arect, v2s32 &pos,
		const core::dimension2d<u32> &dim, const core::stringw &line)
{
	v2s32 shift_pos = pos;
	if (alignment & FORMSPEC_TEXT_ALIGN_RIGHT) {
		shift_pos.X = arect.LowerRightCorner.X - dim.Width;
	} else if (!(alignment & FORMSPEC_TEXT_ALIGN_LEFT)) {
		s32 offset = arect.getWidth() - dim.Width;
		shift_pos.X += offset / 2;
	}
	lines.emplace_back(std::pair<core::stringw, core::rect<s32>>(line, core::rect<s32>(shift_pos, dim)));
	pos.Y += dim.Height;
}

void TextSpec::rebuild(const core::rect<s32> &arect, StyleSpec *style) {
	gui::IGUIFont *font = style->getFont();
	alignment = style->getTextAlign();

	lines.clear();
	u32 width = arect.getWidth();

	std::vector<std::pair<u32, u32>> line_ranges;

	u32 line_width = 0;
	u32 line_begin = 0;
	u32 word_begin = 0;
	u32 word_end = 0;

	bool last_space = true;
	u32 size = text.size();

	// first line
	for (u32 i = 0; i <= size; ++i) {
		if (text[i] == L' ' || text[i] == L'\n' || i == size) {
			// word including spaces
			core::stringw word = text.subString(word_end, i - word_end);
			core::dimension2d<u32> dim = font->getDimension(word.c_str());
			line_width += dim.Width;
			if (line_width > width) {
				// word wrap
				line_ranges.push_back(std::pair<u32, u32>(line_begin, word_end));
				line_begin = word_begin;
				// word excluding spaces
				word = text.subString(word_begin, i - word_begin);
				dim = font->getDimension(word.c_str());
				line_width = dim.Width;
			}
			word_end = i;
			if (text[i] == L'\n' || i == size) {
				// forced line break
				line_ranges.push_back(std::pair<u32, u32>(line_begin, i));
				line_begin = i + 1;
				word_end = i + 1;
				line_width = 0;
				last_space = true;
			}

			last_space = true;
		} else {
			// regular character
			if (last_space) {
				word_begin = i;
				last_space = false;
			}
		}
	}

	v2s32 pos = arect.UpperLeftCorner;
	for (u32 i = 0; i < line_ranges.size(); ++i) {
		const core::stringw line = text.subString(line_ranges[i].first, line_ranges[i].second - line_ranges[i].first);
		const core::dimension2d<u32> dim = font->getDimension(line.c_str());
		addLine(arect, pos, dim, line);
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
	// find the cursor position
	if (cursor_pos != (u32)-1) {
		size_t i = 0;
		for ( ; i < line_ranges.size(); ++i) {
			if (cursor_pos < line_ranges[i].first) {
				// the cursor is in the previous line
				break;
			}
		}
		// we can safely assume, i is not 0, since cursor_pos is unsigned and the first line starts at 0
		--i;
		auto &l = lines[i];
		u32 end = cursor_pos - line_ranges[i].first;
		core::dimension2d<u32> dim = font->getDimension(l.first.subString(0, end).c_str());
		core::dimension2d<u32> cursor_dim = font->getDimension(L"_");
		cursor = core::rect<s32>(l.second.UpperLeftCorner + v2s32(dim.Width, 0), cursor_dim);
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
		text->rebuild(arect, styleSpec.get());
}

void GUIFormSpecMenuElementInput::focusChange(const bool focus) {
	text->setCursorVisibility(focus);
}

void GUIFormSpecMenuElementInput::inputText(const core::stringw &input) {
	const core::stringw &t = text->get();
	u32 length = t.size();
	core::stringw newString = t.subString(0, cursor_pos);
	newString += input;
	newString += t.subString(cursor_pos, length - cursor_pos);
	text->set(newString);
	++cursor_pos;
}

void GUIFormSpecMenuElementInput::keyDown(const SEvent::SKeyInput &k) {
	if (k.Key == KEY_BACK && cursor_pos != 0) {
		const core::stringw &t = text->get();
		u32 length = t.size();
		core::stringw newString = t.subString(0, cursor_pos - 1);
		newString += t.subString(cursor_pos, length - cursor_pos);
		text->set(newString);
		--cursor_pos;
	} else if (k.Key == KEY_RETURN) {
		// irrlicht seems to treat \r as the return character
		inputText(core::stringw(L"\n"));
	} else if (!iswcntrl(k.Char) && !k.Control) {
#if defined(__linux__) && (IRRLICHT_VERSION_MAJOR == 1 && IRRLICHT_VERSION_MINOR < 9)
		wchar_t wc = L'_';
		mbtowc( &wc, (char *)&k.Char, sizeof(k.Char));
		inputText(core::stringw(&wc, 1));
#else
		inputText(core::stringw(&k.Char, 1);
#endif
	}
	text->setCursorPos(cursor_pos);
	text->rebuild(arect, styleSpec.get());
}


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
void TextSpec::draw(core::rect<s32> arect, video::IVideoDriver *driver, gui::IGUIFont *font) const {
	for (const std::pair<core::stringw, core::rect<s32>> &line : lines) {
		font->draw(line.first, line.second, video::SColor(0xFFFFFFFF));
	}

	// draw the cursor
	if (cursor_visibility) {
		font->draw(L"_", cursor, video::SColor(0xFFFF0000));
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
