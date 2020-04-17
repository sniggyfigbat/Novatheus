#include "pch.h"
#include "utils\utils.h"
#include "core\genome.h"

namespace Core {
	bool Genome::deleteNeuron(uint ID)
	{
		// Important! Note that this method is not used by pruneTree - any changes will need to be added to the method separately.
		bool retVal = false;
		
		auto targetIter = m_chromosomes.find(ID);
		if (targetIter == m_chromosomes.end()) { return false; }

		auto& target = targetIter->second;
		auto references = target.m_references;

		// It's conceivably possible that we could chain-delete something such that iterators get buggered.
		while (target.m_startingWeights.size() > 0) {
			uint weightID = target.m_startingWeights.begin()->first;

			if (weightID >= m_inputCount) {
				auto weightIter = m_chromosomes.find(weightID);
				if (weightIter != m_chromosomes.end()) {
					weightIter->second.m_references.erase(ID);
					if (weightIter->second.m_references.size() == 0) { retVal = true; }
				}
			}

			target.m_startingWeights.erase(target.m_startingWeights.begin());
		}

		m_chromosomes.erase(ID);

		for (auto r : references) {
			auto targetR = m_chromosomes.find(r);
			if (targetR != m_chromosomes.end()) {
				m_chromosomes[r].m_startingWeights.erase(ID);
				if (m_chromosomes[r].m_startingWeights.size() == 0) { retVal |= deleteNeuron(r); }
			}
		}

		return retVal;
	}

	void Genome::pruneTree()
	{
		do {
			if (NEURON_COUNT_MIN > m_chromosomes.size()) {
				int toAdd = 2 * (NEURON_COUNT_MIN - m_chromosomes.size());
				for (uint i = 0; i < toAdd; i++) { addRandomNeuron(); }
				WARN("Critically small tree size detected by Genome::pruneTree(). Added {0} new random neurons.", toAdd);
			}
			
			for (auto& c : m_chromosomes) { c.second.m_prunable = true; }

			// Set output nodes as non-prunable.
			auto rIter = m_chromosomes.rbegin();
			for (uint i = 0; i < m_outputCount; i++) {
				rIter->second.m_prunable = false;
				++rIter;
			}

			// Cycle through, marking prunables and non-prunables.
			std::vector<uint> prunableIDs;

			auto rIter = m_chromosomes.rbegin();
			for (; rIter != m_chromosomes.rend(); ++rIter) {
				if (!(rIter->second.m_prunable)) {
					// If not prunable, neither are its connected neurons.
					for (auto w : rIter->second.m_startingWeights) {
						if (w.first >= m_inputCount) { m_chromosomes[w.first].m_prunable = false; }
					}
				}
				else { prunableIDs.push_back(rIter->first); }
			}

			// Finally, do some pruning.
			for (uint id : prunableIDs) { m_chromosomes.erase(id); }
		} while (NEURON_COUNT_MIN > m_chromosomes.size());
	}

	uint Genome::addRandomNeuron()
	{
		uint minID = m_inputCount;
		uint maxID = NEURON_COUNT_MAX * 8u;

		uint newID;
		std::uniform_int_distribution idDist(minID, maxID);

		do { newID = idDist(*getRNG()); }
		while (m_chromosomes.find(newID) != m_chromosomes.end());
		
		// Add neuron.
		float startingBias = std::normal_distribution(0.0f, 2.0f)(*getRNG());
		m_chromosomes[newID] = Chromosome(startingBias);

		// Number of connections to add:
		int availableSlots = m_chromosomes.size() + m_inputCount - 1; // -1 for self-nonconnectibility.
		int maxSlots = std::min(availableSlots, NEURON_CONNECTION_COUNT_MAX);

		float average = std::min((float)NEURON_CONNECTION_COUNT_MAX * 0.25f, availableSlots * 0.5f); // Usually large numbers.
		average = std::max(average, std::min(10.0f, (float)availableSlots)); // Checked against consistently small numbers.

		std::normal_distribution<float> conDist(average, std::max(average * 0.25f, 1.0f));

		int conCount;
		do { int conCount = (int)std::round(conDist(*getRNG())); }
		while (conCount < 0 || conCount > maxSlots);

		for (uint i = 0; i < conCount; i++) { addRandomConnectionToNeuron(newID); }

		return newID;
	}

	void Genome::addRandomConnectionToNeuron(uint ID)
	{
		// Work out how many neurons to shift from current.
		std::normal_distribution<float> dist(0, std::max(((float)m_chromosomes.size() + (float)m_inputCount) * 0.25f, 1.0f));
		uint newLinkID = ID;

		bool readsInput = false; // Shift requires reading an input.

		bool invalid = true; // Shift requires a step off the beginning, or going above targetID.
		while (invalid || newLinkID == ID) {	// Repeatedly try shifting until you find a valid shift.
			invalid = false;
			readsInput = false;
			uint newLinkInputID = m_inputCount - 1;

			int shift = (int)std::round(dist(*getRNG()));
			auto iter = m_chromosomes.find(ID);

			uint lowestID = m_chromosomes.begin()->first;

			if (shift > 0) {
				for (int i = 0; i < shift && !invalid; i++) {
					++iter;
					if (iter == m_chromosomes.end()) { invalid = true; }
				}
			}
			else if (shift < 0) {
				shift = -shift;

				for (int i = 0; i < shift && !invalid; i++) {
					if (iter->first == lowestID) {
						if (readsInput) {
							if (newLinkInputID == 0) { invalid = true; }
							else { newLinkInputID--; }
						}
						else { readsInput = true; }
					}
					else { --iter; }
				}
			}

			if (!invalid) {
				if (readsInput) { newLinkID = newLinkInputID; }
				else { newLinkID = iter->first; }
			}
		}

		if (newLinkID < ID) {
			// Add to m_startingWeights.
			m_chromosomes[ID].m_startingWeights[newLinkID] = std::normal_distribution(0.0f, 0.5f)(*getRNG());
			if (!readsInput) { m_chromosomes[newLinkID].m_references.insert(ID); }
		}
		else {
			m_chromosomes[ID].m_references.insert(newLinkID);
			m_chromosomes[newLinkID].m_startingWeights[ID] = std::normal_distribution(0.0f, 0.5f)(*getRNG());
		}
	}

	void Genome::mutate(bool supermutate)
	{
		uint chromaCount = m_chromosomes.size();
		float chromaCountF = (float)chromaCount;
		
		// Normally, make roughly one mutation action per 5 neurons.
		// Make roughly one per 2 neurons if supermutated.
		uint mutations = 0;
		float average = supermutate ? chromaCountF * 0.5f : chromaCountF * 0.2f;
		std::normal_distribution<float> dist(average, std::max(chromaCountF * 0.15f, 1.0f));
		float number = dist(*getRNG());
		mutations = (uint)std::max((int)std::round(number), 0);

		bool requiresPruning = false;

		// Do mutations:
		for (uint i = 0; i < mutations; i++) {
			MutationTypes mt = getRandomMutationType(); // Properly weighted for appropriate types. See mutation.h, via forwarder.h.
			uint targetID = (uint)std::uniform_int_distribution<int>(0, (chromaCount - 1))(*getRNG());
			
			auto setNav = m_chromosomes.begin();
			std::next(setNav, targetID);
			targetID = setNav->first;
			auto& target = m_chromosomes[targetID];

			switch (mt) {
			case MutationTypes::NeuronAddition:
				if (m_chromosomes.size() < NEURON_COUNT_MAX) {
					addRandomNeuron();
					break;
				}
				// Intentional cascade. Doubles chance of tree reduction if at max size.
			case MutationTypes::NeuronDeletion:
				if (m_chromosomes.size() > NEURON_COUNT_MIN) {
					//if (deleteNeuron(targetID)) { pruneTree(); };
					requiresPruning |= deleteNeuron(targetID);
				}
				break;
			case MutationTypes::NeuronIDDrift:
				// Bounds
				uint max = *(target.m_references.begin());	// Exclusive
				uint min = m_inputCount - 1;				// Also exclusive.
				for (auto& c : target.m_startingWeights) { min = std::max(min, c.first); }

				// Generate new ID
				std::normal_distribution<float> dist((float)targetID, ((float)targetID) * 0.15f);
				uint newID = max;
				while (newID <= min || newID >= max || m_chromosomes.find(newID) != m_chromosomes.end()) {
					newID = (uint)std::round(dist(*getRNG()));
				}

				// Change links
				for (auto& c : target.m_startingWeights) {
					if (c.first >= m_inputCount) {
						auto& t = m_chromosomes[c.first];
						t.m_references.erase(targetID);
						t.m_references.insert(newID);
					}
				}
				for (auto& r : target.m_references) {
					auto& t = m_chromosomes[r];
					t.m_startingWeights[newID] = t.m_startingWeights[targetID];
					t.m_startingWeights.erase(targetID);
				}

				// Move Neuron.
				{
					auto nodeHandler = m_chromosomes.extract(targetID);
					nodeHandler.key() = newID;
					m_chromosomes.insert(std::move(nodeHandler));
				}
				break;
			case MutationTypes::NeuronBiasDrift:
				std::normal_distribution<float> dist(target.m_startingBias, target.m_startingBias * 0.25f);
				target.m_startingBias = dist(*getRNG());
				break;
			case MutationTypes::ConnectionAddition:
				if (target.m_startingWeights.size() < NEURON_CONNECTION_COUNT_MAX) {
					addRandomConnectionToNeuron(targetID);
				}
				break;
			case MutationTypes::ConnectionDeletion:
				if (target.m_startingWeights.size() > 1) {
					// Select connection
					uint targetWeightID = (uint)std::uniform_int_distribution<int>(0, (target.m_startingWeights.size() - 1))(*getRNG());
					targetWeightID = (std::next(target.m_startingWeights.begin(), targetWeightID))->first;

					// Delete it.
					target.m_startingWeights.erase(targetWeightID);
					if (targetWeightID >= m_inputCount) { m_chromosomes[targetWeightID].m_references.erase(targetID); }
				}
				else if (m_chromosomes.size() > NEURON_COUNT_MIN) {
					//if (deleteNeuron(targetID)) { pruneTree(); };
					requiresPruning |= deleteNeuron(targetID);
				}

				break;
			case MutationTypes::ConnectionIDDrift:
				// Select connection
				uint targetWeightID = (uint)std::uniform_int_distribution<int>(0, (target.m_startingWeights.size() - 1))(*getRNG());
				targetWeightID = (std::next(target.m_startingWeights.begin(), targetWeightID))->first;

				// Work out how many neurons to shift it by.
				std::normal_distribution<float> dist(0, std::max((chromaCount + (float)m_inputCount) * 0.1f, 1.0f));
				uint newID = targetID;

				bool readsInput = false; // Shift requires reading an input.

				bool invalid = true; // Shift requires a step off the beginning, or going above targetID.
				while (invalid) {	// Repeatedly try shifting until you find a valid shift.
					invalid = false;
					readsInput = false;
					uint newInputID = m_inputCount - 1;

					int shift = (int)std::round(dist(*getRNG()));
					auto iter = m_chromosomes.find(targetWeightID);
					
					uint lowestID = m_chromosomes.begin()->first;

					if (shift > 0) {
						for (int i = 0; i < shift && !invalid; i++) {
							++iter;
							if (iter->first >= targetID) { invalid = true; }
						}
					}
					else if (shift < 0) {
						shift = -shift;

						for (int i = 0; i < shift && !invalid; i++) {
							if (iter->first == lowestID) {
								if (readsInput) {
									if (newInputID == 0) { invalid = true; }
									else { newInputID--; }
								}
								else { readsInput = true; }
							}
							else { --iter; }
						}
					}

					if (!invalid) {
						if (readsInput) { newID = newInputID; }
						else { newID = iter->first; }
					}
				}

				if (newID != targetWeightID) {
					auto& existing = target.m_startingWeights.find(newID);

					if (existing != target.m_startingWeights.end()) {
						// Flip a coin, replace or discard.
						bool replace = (bool)std::uniform_int_distribution(0, 1)(*getRNG());
						if (replace) { existing->second = target.m_startingWeights[targetWeightID]; }
					}
					else {
						target.m_startingWeights[newID] = target.m_startingWeights[targetWeightID];
						if (!readsInput) { m_chromosomes[newID].m_references.insert(targetID); }
					}

					target.m_startingWeights.erase(targetWeightID);
					if (!readsInput) { m_chromosomes[targetWeightID].m_references.erase(targetID); }
				}
				break;
			case MutationTypes::ConnectionWeightDrift:
			default:
				// Select connection
				uint targetWeightID = (uint)std::uniform_int_distribution<int>(0, (target.m_startingWeights.size() - 1))(*getRNG());
				targetWeightID = (std::next(target.m_startingWeights.begin(), targetWeightID))->first;

				float sw = target.m_startingWeights[targetWeightID];
				std::normal_distribution<float> dist(sw, sw * 0.25f);
				target.m_startingWeights[targetWeightID] = dist(*getRNG());
				break;
			}
		}

		if (requiresPruning) { pruneTree(); }
	}
}