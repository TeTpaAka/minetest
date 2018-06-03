#include <string>
#include <stack>
#include <memory>

#include "client.h"

class StyleSpec;
class GUIFormSpecMenuElement;

class GUIFormSpecParser {
public:
	static std::unique_ptr<GUIFormSpecMenuElement> parse(const std::string &formspec, ISimpleTextureSource *tsrc,
		Client *client, InventoryManager *invmgr, const std::shared_ptr<StyleSpec> &style);
};
