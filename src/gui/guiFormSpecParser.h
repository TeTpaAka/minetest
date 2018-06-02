#include <string>
#include <stack>
#include <memory>

#include "client.h"

class GUIFormSpecMenuElement;

class GUIFormSpecParser {
public:
	static void parseElement(const std::string &element, std::stack<std::unique_ptr<GUIFormSpecMenuElement>> &stack,
		ISimpleTextureSource *tsrc, Client *client, InventoryManager *invmgr);
};
