#include "pch.h"
#include "utils\utils.h"
#include "core\genome.h"

namespace Core {
	void Genome::mutate(bool supermutate)
	{
		uint chromaCount = m_usedChromosomeIDs.size();
		float chromaCountF = (float)chromaCount;
		
		// Normally, make roughly one mutation action per 5 neurons.
		// Make roughly one per 2 neurons if supermutated.
		uint mutations = 0;
		float average = supermutate ? chromaCountF * 0.5f : chromaCountF * 0.2f;
		std::normal_distribution<float> dist(average, std::max(chromaCountF * 0.15f, 1.0f));
		float number = dist(*getRNG());
		mutations = (uint)std::max((int)std::round(number), 0);

		// Do mutations:
		for (uint i = 0; i < mutations; i++) {
			MutationTypes mt = getRandomMutationType(); // Properly weighted for appropriate types. See mutation.h, via forwarder.h.
			uint targetID = (uint)std::uniform_int_distribution<int>(0, (chromaCount - 1))(*getRNG());
			
			auto setNav = m_usedChromosomeIDs.begin();
			for (uint j = 0; j < targetID; j++) { ++setNav; }
			targetID = *(setNav);

			switch (mt) {
			case MutationTypes::NeuronAddition:
				break;
			case MutationTypes::NeuronDeletion:
				break;
			case MutationTypes::NeuronIDDrift:
				break;
			case MutationTypes::NeuronBiasDrift:
				float sb = m_chromosomes[targetID].m_startingBias;
				std::normal_distribution<float> dist(sb, sb * 0.25f);
				m_chromosomes[targetID].m_startingBias = dist(*getRNG());
				break;
			case MutationTypes::ConnectionAddition:
				break;
			case MutationTypes::ConnectionDeletion:
				break;
			case MutationTypes::ConnectionIDDrift:
				// Select connection
				uint targetWeightIndex = (uint)std::uniform_int_distribution<int>(0, (m_chromosomes[targetID].m_startingWeights.size() - 1))(*getRNG());

				// TODO: Cont.

				// Work out how many neurons to shift it by.
				//std::normal_distribution<float> dist(average, std::max(chromaCountF * 0.15f, 1.0f));
				//float number = dist(*getRNG());
				//mutations = (uint)std::max((int)std::round(number), 0);
				break;
			case MutationTypes::ConnectionWeightDrift:
			default:
				uint targetWeightIndex = (uint)std::uniform_int_distribution<int>(0, (m_chromosomes[targetID].m_startingWeights.size() - 1))(*getRNG());
				float sw = m_chromosomes[targetID].m_startingWeights[targetWeightIndex].m_value;
				std::normal_distribution<float> dist(sw, sw * 0.25f);
				m_chromosomes[targetID].m_startingWeights[targetWeightIndex].m_value = dist(*getRNG());
				break;
				break;
			}
		}
	}
}