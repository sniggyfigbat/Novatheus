#pragma once

#include "utils/logger.h"
#include <SFML/Graphics.hpp>

namespace Utils {
	class AssetManager {
	private:
		std::map<std::string, sf::Texture *> m_textures;
		std::map<std::string, sf::Font *> m_fonts;
	public:
		sf::Texture * getTexture(std::string name);
		sf::Font * getFont(std::string name);
		
		AssetManager() {}
		~AssetManager();
	};
}