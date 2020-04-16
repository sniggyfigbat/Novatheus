#include "pch.h"

#include "utils/forwarder.h"
#include "utils/assetmanager.h"

namespace Utils {
	sf::Texture * HasForwarder::getTexture(std::string name)
	{
		return mp_forwarder->p_assetManager->getTexture(name);
	}

	sf::Font * HasForwarder::getFont(std::string name)
	{
		return mp_forwarder->p_assetManager->getFont(name);
	}
}