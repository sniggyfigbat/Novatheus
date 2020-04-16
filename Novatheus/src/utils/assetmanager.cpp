#include "pch.h"
#include "utils/assetmanager.h"

namespace Utils {
	sf::Texture * AssetManager::getTexture(std::string name)
	{
		auto found = m_textures.find(name);
		if (found == m_textures.end()) {
			std::string filepath = "../assets/" + name + ".png";
			try {
				sf::Texture * newTexture = new sf::Texture;
				if (!newTexture->loadFromFile(filepath))
				{
					WARN("Could not find/load texture '{0}'! ('{1}')", name, filepath);
					return nullptr;
				}

				m_textures.insert(std::pair<std::string, sf::Texture *>(name, newTexture));
				return newTexture;
			}
			catch (...) {
				ERRORM("Could not find/load texture '{0}'! ('{1}')", name, filepath);
				return nullptr;
			}
		}
		return (found->second);
	}
	sf::Font * AssetManager::getFont(std::string name)
	{
		auto found = m_fonts.find(name);
		if (found == m_fonts.end()) {
			std::string filepath = "../assets/" + name + ".ttf";
			try {
				sf::Font * newFont = new sf::Font;
				if (!newFont->loadFromFile(filepath))
				{
					WARN("Could not find/load font '{0}'! ('{1}')", name, filepath);
					return nullptr;
				}

				m_fonts.insert(std::pair<std::string, sf::Font *>(name, newFont));
				return newFont;
			}
			catch (...) {
				ERRORM("Could not find/load font '{0}'! ('{1}')", name, filepath);
				return nullptr;
			}
		}
		return (found->second);
	}
	AssetManager::~AssetManager()
	{
		for (auto iter = m_textures.begin(); iter != m_textures.end(); ++iter) {
			delete iter->second;
		}
		for (auto iter = m_fonts.begin(); iter != m_fonts.end(); ++iter) {
			delete iter->second;
		}
	}
}