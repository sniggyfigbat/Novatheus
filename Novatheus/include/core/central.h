#pragma once
#include "utils\forwarder.h"
#include "core\dataset.h"
#include "core\network.h"

namespace Core {
	class CentralController {
	public:
		enum class RunState { Completed, Running, Awaiting };
	private:
		std::default_random_engine * mp_rng;
		Utils::AssetManager * mp_assetManager;
		Utils::Forwarder * mp_forwarder;
		Dataset * mp_dataset = nullptr;

		std::vector<Genome*> mvp_generation;
		std::vector<uint> m_rouletteWheel;

		Genome * mp_genome = nullptr;
		Network * mp_network = nullptr;

		void generateRandomNetwork(bool detailedOutput = false);
		void generateRandomPopulation();

		void savePopulation();
		void loadPopulation(uint popID, uint generation = 0);
		void stepPopulation();
		void runPopulation(uint genLimit = 0u);	// 0 means run indefinitely.

		std::mutex m_popRunStatesMutex = std::mutex();
		std::array<CentralController::RunState, GEN_WIDTH> m_popRunStates;
		std::vector<Metrics> m_metricsBuffer;

		std::queue<std::pair<std::string, std::vector<std::string>>> m_commandQueue; // Each command may come with a list of params.
		void executeCommand(std::pair<std::string, std::vector<std::string>> commandPair);	// Returns 
		bool m_orderedToQuit = false;

		template <class SquishifierType>
		Core::Metrics trainTestAndCrossval(Core::Genome * genome, uint batches);
	public:
		CentralController();
		~CentralController();

		bool runLoop(); // Returns true if program should quit.
	};
}