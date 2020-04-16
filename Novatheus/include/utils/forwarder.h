#pragma once
#include "core/mutations.h"

namespace sf {
	class Texture;
	class Font;
}

namespace Utils {
	class AssetManager;

	class Forwarder {
	private:
		bool m_depreciated = false; //!< Whether or not this forwarder still has valid links.
	public:
		inline bool isDepreciated() { return m_depreciated; }
		inline void setDepreciated(bool newValue = true) { m_depreciated = newValue; }

		std::default_random_engine * p_rng;
		Utils::AssetManager * p_assetManager;

		Core::MutationTable * mp_mutationTable = nullptr;

		Forwarder(std::default_random_engine * rng, Utils::AssetManager * assetManager) :
			p_rng(rng),
			p_assetManager(assetManager) {
			mp_mutationTable = new Core::MutationTable();
			mp_mutationTable->setRandomEngine(p_rng);
		}

		~Forwarder() {
			delete mp_mutationTable;
		}
	};

	class HasForwarder {
	protected:
		Forwarder * mp_forwarder;

		HasForwarder(Forwarder * forwarder) : mp_forwarder(forwarder) {};
	public:
		inline Forwarder * getForwarder() { return mp_forwarder; }
		
		inline std::default_random_engine * getRNG() { return mp_forwarder->p_rng; }
		inline Utils::AssetManager * getAssetManager() { return mp_forwarder->p_assetManager; }
		sf::Texture * getTexture(std::string name);
		sf::Font * getFont(std::string name);
		inline Core::MutationTable* getMutationTable() { return mp_forwarder->mp_mutationTable; }
		inline Core::MutationTypes getRandomMutationType() { return mp_forwarder->mp_mutationTable->getRandomMutationType(); }
	};
}