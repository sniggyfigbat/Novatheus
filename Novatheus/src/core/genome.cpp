#include "pch.h"
#include "utils\utils.h"
#include "core\genome.h"

namespace Core {
	Genome::DeleteNeuronReturnStruct Genome::deleteNeuron(uint ID)
	{
		// Important! Note that this method is not used by pruneTree - any changes will need to be added to the method separately.
		auto targetIter = m_chromosomes.find(ID);
		if (targetIter == m_chromosomes.end()) { return Genome::DeleteNeuronReturnStruct(false, false); }

		auto& target = targetIter->second;
		auto references = target.m_references;

		bool requiresPruning = false;
		bool requiresOutputCleanup = target.m_isAnOutput;

		// It's conceivably possible that we could chain-delete something such that iterators get buggered.
		while (!target.m_startingWeights.empty()) {
			uint weightID = target.m_startingWeights.begin()->first;

			if (weightID >= m_inputCount) {
				auto weightIter = m_chromosomes.find(weightID);
				if (weightIter != m_chromosomes.end()) {
					weightIter->second.m_references.erase(ID);
					if (weightIter->second.m_references.empty()) { requiresPruning = true; }
				}
			}

			target.m_startingWeights.erase(target.m_startingWeights.begin());
		}

		m_chromosomes.erase(ID);

		for (auto r : references) {
			auto targetR = m_chromosomes.find(r);
			if (targetR != m_chromosomes.end()) {
				m_chromosomes[r].m_startingWeights.erase(ID);
				if (m_chromosomes[r].m_startingWeights.empty()) {
					auto returned = deleteNeuron(r);
					requiresPruning |= returned.m_requiresPruning;
					requiresOutputCleanup |= returned.m_requiresOutputCleanup;
				}
			}
		}

		return Genome::DeleteNeuronReturnStruct(requiresPruning, requiresOutputCleanup);
	}

	void Genome::pruneTree()
	{
		bool loop = false;
		do {
			loop = false;

			if (NEURON_COUNT_MIN > m_chromosomes.size()) {
				int toAdd = 2 * (NEURON_COUNT_MIN - m_chromosomes.size());
				for (uint i = 0; i < toAdd; i++) { addRandomNeuron(); }
				WARN("Critically small tree size detected by Genome::pruneTree(). Added {0} new random neurons.", toAdd);
			}
			
			// Here, m_procBool is 'prunable'.
			for (auto& c : m_chromosomes) { c.second.m_procBool1 = true; }

			// Set output nodes as non-prunable.
			auto rIter = m_chromosomes.rbegin();
			for (uint i = 0; i < m_outputCount; i++) {
				rIter->second.m_procBool1 = false;
				++rIter;
			}

			// Cycle through, marking prunables and non-prunables.
			std::vector<uint> prunableIDs;

			rIter = m_chromosomes.rbegin();
			for (; rIter != m_chromosomes.rend(); ++rIter) {
				if (!(rIter->second.m_procBool1)) {
					// If not prunable, neither are its connected neurons.
					for (auto w : rIter->second.m_startingWeights) {
						if (w.first >= m_inputCount) { m_chromosomes[w.first].m_procBool1 = false; }
					}
				}
				else { prunableIDs.push_back(rIter->first); }
			}

			// Finally, do some pruning.
			for (uint id : prunableIDs) { deleteNeuron(id); }

			// Oh, and cleanup the outputs.
			loop |= cleanupOutputs();
		} while (NEURON_COUNT_MIN > m_chromosomes.size() || loop);
	}

	bool Genome::cleanupOutputs()
	{
		// Doesn't move neurons, just determines which are outputs, and ensures that none of them reference one another.
		bool requiresPruning = false;
		bool loop = true;
		while (loop) {
			loop = false;
			std::vector<uint> outputIDs;
			outputIDs.reserve(m_outputCount);

			auto rIter = m_chromosomes.rbegin();
			for (uint i = 0; i < m_outputCount; i++) {
				rIter->second.m_isAnOutput = true;
				outputIDs.push_back(rIter->first);
				rIter++;
			}

			for (; rIter != m_chromosomes.rend(); ++rIter) { rIter->second.m_isAnOutput = false; }

			m_lowestOutputNeuronID = outputIDs.back();

			// Sort out startingWeights and direct references.
			rIter = m_chromosomes.rbegin();
			std::vector<uint> requiresDeletion;
			for (uint i = 0; i < m_outputCount; i++) {
				for (auto id : outputIDs) {
					rIter->second.m_startingWeights.erase(id);
					rIter->second.m_references.erase(id);
				}
				if (rIter->second.m_startingWeights.empty()) {
					// Just in case an output neuron is solely based on other output neurons. Somehow.
					requiresDeletion.push_back(rIter->first);
					loop = true;
				}

				rIter++;
			}

			for (auto id : requiresDeletion) {
				auto returned = deleteNeuron(id);
				loop |= returned.m_requiresOutputCleanup;
				requiresPruning |= returned.m_requiresPruning;
			}
		}

		return requiresPruning;
	}

	uint Genome::addRandomNeuron(bool allowOutput, bool rationalize)
	{
		uint minID = m_inputCount;
		uint maxID = allowOutput ? NEURON_COUNT_MAX * 8u : m_lowestOutputNeuronID;

		uint newID;
		std::uniform_int_distribution idDist(minID, maxID);

		do { newID = idDist(*getRNG()); }
		while (m_chromosomes.find(newID) != m_chromosomes.end());
		
		// Add neuron.
		float startingBias = 0.0f;
		std::normal_distribution biasDist(0.0f, 0.5f);
		do { startingBias = biasDist(*getRNG()); } while (startingBias == 0.0f);
		m_chromosomes[newID] = Chromosome(startingBias);
		auto& tc = m_chromosomes[newID];

		// Number of connections to add:
		uint availableSlots = m_chromosomes.size() + m_inputCount - 1; // -1 for self-nonconnectibility.
		int maxSlots = std::min(availableSlots, NEURON_CONNECTION_COUNT_MAX);

		float average = std::min((float)NEURON_CONNECTION_COUNT_MAX * 0.125f, (float)availableSlots * 0.25f); // Usually large numbers.
		average = std::min((float)availableSlots, std::max(7.0f, average)); // Checked against consistently small numbers.

		std::normal_distribution<float> conDist(average, std::max(average * 0.25f, 1.0f));

		int conCount;
		do { conCount = (int)std::round(conDist(*getRNG())); }
		while (conCount < 2 || conCount > maxSlots);

		for (uint i = 0; i < conCount; i++) { addRandomConnectionToNeuron(newID, true); }
		while (tc.m_startingWeights.empty()) { addRandomConnectionToNeuron(newID, true); }

		if (rationalize) { tc.rationaliseWeightings(); }

		if (tc.m_startingWeights.empty() && tc.m_references.empty()) {
			ERRORM("id{0}: Created neuron (id{1}) with no starting weights or references.", getID(), newID);
		}
		return newID;
	}

	uint Genome::addRandomConnectionToNeuron(uint ID, bool allowReferencedOutputs)
	{
		// Work out how many neurons to shift from current.
		float offset = (float)m_chromosomes.size() * 0.15f;
		offset = std::max(offset, 20.0f);

		float centre = std::uniform_int_distribution(0, 1)(*getRNG()) ? offset : -offset;
		std::normal_distribution<float> dist(centre, offset);
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

			if (shift == 0) { invalid = true; }
			else if (shift > 0) {
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

			if (newLinkID < ID && m_chromosomes[ID].m_startingWeights.find(newLinkID) != m_chromosomes[ID].m_startingWeights.end()) { invalid = true; }
			if (newLinkID > ID && m_chromosomes[ID].m_references.find(newLinkID) != m_chromosomes[ID].m_references.end()) { invalid = true; }
			if (newLinkID < ID && newLinkID >= m_lowestOutputNeuronID && !allowReferencedOutputs) { invalid = true; }
		}

		std::normal_distribution weightDist(0.0f, 1.0f);
		if (newLinkID < ID) {
			// Add to m_startingWeights.
			do { m_chromosomes[ID].m_startingWeights[newLinkID] = weightDist(*getRNG()); } while (m_chromosomes[ID].m_startingWeights[newLinkID] == 0.0f);
			if (!readsInput) { m_chromosomes[newLinkID].m_references.insert(ID); }
		}
		else {
			do { m_chromosomes[newLinkID].m_startingWeights[ID] = weightDist(*getRNG()); } while (m_chromosomes[newLinkID].m_startingWeights[ID] == 0.0f);
			m_chromosomes[ID].m_references.insert(newLinkID);
		}

		return newLinkID;
	}

	void Genome::moveNeuron(uint sourceID, uint destID, bool warnInvalidMove)
	{
		auto target = m_chromosomes.find(sourceID);
		if (target == m_chromosomes.end()) {
			if (warnInvalidMove) {
				WARN("id{0}: Invalid neuron move called in chromosome; No neuron found of source ID.", getID());
			}
			return;
		}

		auto dest = m_chromosomes.find(destID);
		bool destIsFilled = (dest != m_chromosomes.end());
		if (destIsFilled && warnInvalidMove) { INFO("id{0}: Questionable neuron move called in chromosome; Neuron already present at dest ID.", getID()); }
		
		if (!destIsFilled) {
			// Change links
			for (auto& c : target->second.m_startingWeights) {
				if (c.first >= m_inputCount) {
					auto& t = m_chromosomes[c.first];
					t.m_references.erase(sourceID);
					t.m_references.insert(destID);
				}
			}
			for (auto& r : target->second.m_references) {
				auto& t = m_chromosomes[r];
				t.m_startingWeights[destID] = t.m_startingWeights[sourceID];
				t.m_startingWeights.erase(sourceID);
			}

			// Move Neuron.
			auto nodeHandler = m_chromosomes.extract(sourceID);
			nodeHandler.key() = destID;
			m_chromosomes.insert(std::move(nodeHandler));

			return;
		}

		// Already a neuron at dest ID.

		// Backward references
		for (auto iter = target->second.m_startingWeights.begin(); iter != target->second.m_startingWeights.end() && (dest->second.m_startingWeights.size() < NEURON_CONNECTION_COUNT_MAX); ++iter) {
			dest->second.m_startingWeights[iter->first] = iter->second;
			if (iter->first >= m_inputCount) { m_chromosomes[iter->first].m_references.insert(destID); }
		}
		for (auto iter = target->second.m_startingWeights.begin(); iter != target->second.m_startingWeights.end(); ++iter) {
			// This could leave hanging neurons, but it's kind of an edge case so I don't really care.
			if (iter->first >= m_inputCount) { m_chromosomes[iter->first].m_references.erase(sourceID); }
		}

		// Forward references.
		for (auto iter = target->second.m_references.begin(); iter != target->second.m_references.end(); ++iter) {
			dest->second.m_references.insert(*iter);
			m_chromosomes[*iter].m_startingWeights[destID] = m_chromosomes[*iter].m_startingWeights[sourceID];
			m_chromosomes[*iter].m_startingWeights.erase(sourceID);
		}

		m_chromosomes.erase(sourceID);

		return;
	}

	Genome::Genome(Utils::Forwarder* forwarder, uint populationID, uint inputCount, uint outputCount, uint generation) :
		Utils::HasForwarder(forwarder),
		m_populationID(populationID),
		m_inputCount(inputCount),
		m_outputCount(outputCount),
		m_generation(generation)
	{

	}

	Genome::Genome(Utils::Forwarder* forwarder, uint populationID, uint inputCount, uint outputCount, bool detailedOutput) :
		Utils::HasForwarder(forwarder),
		m_populationID(populationID),
		m_inputCount(inputCount),
		m_outputCount(outputCount)
	{
		if (detailedOutput) { INFO("id{0}: Generating random genome...", getID()); }

		if (outputCount > NEURON_COUNT_MIN) { WARN("id{0}: Output count exceeds NEURON_COUNT_MIN in genome constructor.", getID()); }

		// Learning Rate:
		m_startLRExponent = std::normal_distribution<float>(-4.0, 1.0f)(*getRNG());
		m_LRExponentDelta = std::normal_distribution<float>(-6.0, 1.0f)(*getRNG());
		////m_startLRExponent = 0.5f;
		//m_LRExponentDelta = -3.5f;

		float interval = ((float)(NEURON_COUNT_MAX - NEURON_COUNT_MIN)) * 0.5f;
		uint desiredNeuronCount = std::clamp((uint)std::normal_distribution(interval + (float)NEURON_COUNT_MIN, 0.15f * interval)(*getRNG()), NEURON_COUNT_MIN, NEURON_COUNT_MAX);

		if (detailedOutput) { INFO("id{0}: Random genome will have ~{1} neurons, learning rate exponent {2}->{3}.", getID(), desiredNeuronCount, m_startLRExponent, m_LRExponentDelta); }

		for (uint i = 0; i < desiredNeuronCount; i++) {
			addRandomNeuron(true, false);
			if (detailedOutput && (i % 512u) == 0u) { INFO("id{0}: Generated {1}/{2} random neurons.", getID(), i, desiredNeuronCount); }
		}

		if (detailedOutput) { INFO("id{0}: Cleaning, pruning, rationalising...", getID()); }
		cleanupOutputs();
		pruneTree();
		for (auto& c : m_chromosomes) { c.second.rationaliseWeightings(); }

		if (detailedOutput) { INFO("id{0}: Generated random genome with {1} neurons, learning rate exponent {2}->{3}.", getID(), m_chromosomes.size(), m_startLRExponent, m_LRExponentDelta); }
	}

	Genome::Genome(Utils::Forwarder* forwarder, std::ifstream& source, bool detailedOutput) :
		Utils::HasForwarder(forwarder)
	{
		Utils::FileInHandler fih(source);
		
		fih.readItem(m_populationID);			// uint
		fih.readItem(m_generation);				// uint

		fih.readItem(m_tested);					// bool
		fih.readItem(m_rank);					// uint
		fih.readItem(m_metrics.m_trainingBufferAverageCost);	// float
		fih.readItem(m_metrics.m_trainingBufferAverageCACost);	// float
		fih.readItem(m_metrics.m_trainingBufferAccuracy);		// float
		fih.readItem(m_metrics.m_testingBufferAverageCost);		// float
		fih.readItem(m_metrics.m_testingBufferAverageCACost);	// float
		fih.readItem(m_metrics.m_testingBufferAccuracy);		// float

		fih.readItem(m_inputCount);				// uint
		fih.readItem(m_outputCount);			// uint

		fih.readItem(m_lowestOutputNeuronID);	// uint

		fih.readItem(m_startLRExponent);		// float
		fih.readItem(m_LRExponentDelta);		// float

		uint cs;
		fih.readItem(cs);						// uint
		for (uint c = 0; c < cs; c++) {
			if (detailedOutput && (c % 512u) == 0u) {
				INFO("id{0}: Reading chromosome {1}/{2}...", getID(), c, cs);
			}
			
			float startingBias, weight;
			bool isAnOutput;
			uint id, ws, rs;

			fih.readItem(id);					// uint (ID)

			fih.readItem(startingBias);			// float
			fih.readItem(isAnOutput);			// bool

			m_chromosomes[id] = Chromosome(startingBias, isAnOutput);
			auto& t = m_chromosomes[id];

			fih.readItem(ws);					// uint
			for (uint w = 0; w < ws; w++) {
				fih.readItem(id);				// uint (ID)
				fih.readItem(weight);			// float (weight)

				t.m_startingWeights[id] = weight;
			}

			fih.readItem(rs);					// uint
			for (uint r = 0; r < rs; r++) {
				fih.readItem(id);				// uint (ID)
				t.m_references.insert(id);
			}
		}
	}

	void Chromosome::rationaliseWeightings()
	{
		// Xavier initialisation.
		float factor = std::pow((float)m_startingWeights.size(), -1.1);
		for (auto& w : m_startingWeights) { w.second *= factor; }
	}

	void Genome::mutate(bool supermutate)
	{
		if (supermutate) { INFO("id{0}: Super-Mutating...", getID()); }
		else { INFO("id{0}: Mutating...", getID()); }

		m_tested = false;
		
		uint chromaCount = m_chromosomes.size();
		float chromaCountF = (float)chromaCount;
		
		// Normally, make roughly one mutation action per 10 neurons.
		// Make roughly one per 5 neurons if supermutated.
		uint mutations = 0;
		float average = supermutate ? chromaCountF * 0.2f : chromaCountF * 0.1f;
		std::normal_distribution<float> mutCountDist(average, std::max(chromaCountF * 0.15f, 1.0f));
		float number = mutCountDist(*getRNG());
		mutations = (uint)std::max((int)std::round(number), 0);

		bool requiresPruning = false;
		bool requiresOutputCleanup = false;

		// Do mutations:
		for (uint i = 0; i < mutations; i++) {
			chromaCount = m_chromosomes.size();
			chromaCountF = (float)chromaCount;

			MutationTypes mt = getRandomMutationType(); // Properly weighted for appropriate types. See mutation.h, via forwarder.h.
			uint targetID = (uint)std::uniform_int_distribution<int>(0, (chromaCount - 1))(*getRNG());

			auto setNav = m_chromosomes.begin();
			std::advance(setNav, targetID);
			targetID = setNav->first;
			auto& target = m_chromosomes[targetID];

			bool targetIsAnOutput = (targetID >= m_lowestOutputNeuronID);

			switch (mt) {
			case MutationTypes::NeuronAddition:
			{
				if (m_chromosomes.size() < NEURON_COUNT_MAX) {
					addRandomNeuron();
					break;
				}
			}
			// Intentional cascade. Doubles chance of tree reduction if at max size.
			case MutationTypes::NeuronDeletion:
			{
				if (m_chromosomes.size() > NEURON_COUNT_MIN && !targetIsAnOutput) {
					//if (deleteNeuron(targetID)) { pruneTree(); };
					auto ret = deleteNeuron(targetID);
					requiresPruning |= ret.m_requiresPruning;
					requiresOutputCleanup |= ret.m_requiresOutputCleanup;
				}
			}
			break;
			case MutationTypes::NeuronIDDrift:
			{
				// Bounds
				uint max, min;
				if (targetIsAnOutput) {
					auto iter = m_chromosomes.find(targetID);

					bool isLast = (targetID == m_chromosomes.rbegin()->first);
					max = isLast ? NEURON_COUNT_MAX * 8u : (++iter)->first;

					bool isFirst = (targetID == m_chromosomes.begin()->first);
					if (!isFirst) {
						--iter;
						--iter;
						min = iter->first;
					}
					else { min = min = m_inputCount - 1; }
				}
				else {
					max = m_lowestOutputNeuronID;		// Exclusive
					if (!target.m_references.empty()) { max = std::min(max, *(target.m_references.begin())); }
					min = m_inputCount - 1;				// Also exclusive.
					if (!target.m_startingWeights.empty()) { min = std::max(min, target.m_startingWeights.rbegin()->first); }
				}

				// Generate new ID
				std::normal_distribution<float> newIDDist((float)targetID, ((float)targetID) * 0.15f);
				uint newID = max;
				while (newID <= min || newID >= max || m_chromosomes.find(newID) != m_chromosomes.end()) {
					newID = (uint)std::round(newIDDist(*getRNG()));
				}

				moveNeuron(targetID, newID);
			}
				break;
			case MutationTypes::NeuronBiasDrift:
			{
				std::normal_distribution<float> dist(target.m_startingBias, std::max(std::abs(target.m_startingBias * 0.25f), 0.01f));
				do { target.m_startingBias = dist(*getRNG()); } while (target.m_startingBias == 0.0f);
			}
				break;
			case MutationTypes::ConnectionAddition:
			{
				if (target.m_startingWeights.size() < NEURON_CONNECTION_COUNT_MAX) {
					uint newCon = addRandomConnectionToNeuron(targetID);
					if (newCon < targetID) {
						target.m_startingWeights[newCon] *= std::sqrt(1.0f / (float)target.m_startingWeights.size());
					}
					else {
						m_chromosomes[newCon].m_startingWeights[targetID] *= std::sqrt(1.0f / (float)m_chromosomes[newCon].m_startingWeights.size());
					}
				}
			}
				break;
			case MutationTypes::ConnectionDeletion:
			{
				if (target.m_startingWeights.size() > 1) {
					// Select connection
					uint targetWeightID = (uint)std::uniform_int_distribution<int>(0, (target.m_startingWeights.size() - 1))(*getRNG());
					targetWeightID = (std::next(target.m_startingWeights.begin(), targetWeightID))->first;

					// Delete it.
					target.m_startingWeights.erase(targetWeightID);
					if (targetWeightID >= m_inputCount) { m_chromosomes[targetWeightID].m_references.erase(targetID); }
				}
				else if (!targetIsAnOutput && m_chromosomes.size() > NEURON_COUNT_MIN) {
					//if (deleteNeuron(targetID)) { pruneTree(); };
					auto ret = deleteNeuron(targetID);
					requiresPruning |= ret.m_requiresPruning;
					requiresOutputCleanup |= ret.m_requiresOutputCleanup;
				}
			}
				break;
			case MutationTypes::ConnectionIDDrift:
			{
				// Select connection
				uint targetWeightID = (uint)std::uniform_int_distribution<int>(0, (target.m_startingWeights.size() - 1))(*getRNG());
				targetWeightID = (std::next(target.m_startingWeights.begin(), targetWeightID))->first;

				// Work out how many neurons to shift it by.
				std::normal_distribution<float> shiftDist(0, std::max((chromaCount + (float)m_inputCount) * 0.1f, 1.0f));
				uint newID = targetID;

				bool readsInput = false; // Shift requires reading an input.

				bool invalid = true; // Shift requires a step off the beginning, or going above targetID.
				while (invalid) {	// Repeatedly try shifting until you find a valid shift.
					invalid = false;
					readsInput = false;
					uint newInputID = m_inputCount - 1;

					int shift = (int)std::round(shiftDist(*getRNG()));
					auto iter = m_chromosomes.find(targetWeightID);


					uint lowestID = m_chromosomes.begin()->first;

					if (shift > 0) {
						if (targetWeightID < m_inputCount) {
							int toExit = m_inputCount - targetWeightID;
							if (shift > toExit) {
								shift -= toExit;
								iter = m_chromosomes.begin();
							}
							else {
								readsInput = true;
								newInputID = targetWeightID + shift;
								shift = 0;
							}
						}
						
						for (int i = 0; i < shift && !invalid; i++) {
							++iter;
							if (iter->first >= targetID) { invalid = true; }
						}
					}
					else if (shift < 0) {
						shift = -shift;

						if (targetWeightID < m_inputCount) {
							iter = m_chromosomes.begin();
							readsInput = true;
							newInputID = targetWeightID;
						}

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
					else if (targetWeightID < m_inputCount) {
						readsInput = true;
						newInputID = targetWeightID;
					}

					if (!invalid) {
						if (readsInput) { newID = newInputID; }
						else { newID = iter->first; }
					}

					if (newID >= m_lowestOutputNeuronID) { invalid = true; }
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
					if (targetWeightID >= m_inputCount) { m_chromosomes[targetWeightID].m_references.erase(targetID); }
				}
			}
				break;
			case MutationTypes::LRStartDrift:
			{
				m_startLRExponent = std::normal_distribution<float>(m_startLRExponent, 0.5f)(*getRNG());
			}
			break;
			case MutationTypes::LREndDrift:
			{
				m_LRExponentDelta = std::normal_distribution<float>(m_LRExponentDelta, 0.5f)(*getRNG());
			}
			break;
			case MutationTypes::ConnectionWeightDrift:
			default:
			{
				// Select connection
				uint targetWeightID = (uint)std::uniform_int_distribution<int>(0, (target.m_startingWeights.size() - 1))(*getRNG());
				targetWeightID = (std::next(target.m_startingWeights.begin(), targetWeightID))->first;

				float sw = target.m_startingWeights[targetWeightID];
				std::normal_distribution<float> dist(sw, std::max(std::abs(sw * 0.25f), 0.001f));
				target.m_startingWeights[targetWeightID] = dist(*getRNG());
			}
				break;
			}

			if (m_chromosomes.begin()->second.m_startingWeights.empty()) {
				ERRORM("id{0}: Detected discrepancy during mutation.", getID());
			}
		}

		if (requiresOutputCleanup) { requiresPruning |= cleanupOutputs(); }
		if (requiresPruning) { pruneTree(); }

		if (supermutate) { INFO("id{0}: Super-Mutated.", getID()); }
		else { INFO("id{0}: Mutated.", getID()); }
	}

	Genome * Genome::operator+(Genome * other)
	{
		Genome * child = nullptr;
		
		INFO("Starting child-creation operation between id{0} ({1} neurons) and id{2} ({3} neurons)...", getID(), m_chromosomes.size(), other->getID(), other->m_chromosomes.size());

		bool allShipshape = false;
		while (!allShipshape) {
			allShipshape = true;
			if (child != nullptr) { delete child; }
			child = new Genome(getForwarder(), m_populationID, m_inputCount, m_outputCount, m_generation + 1);

			// Learning Rate
			child->m_startLRExponent = (m_startLRExponent > other->m_startLRExponent) ? std::uniform_real_distribution<float>(other->m_startLRExponent, m_startLRExponent)(*getRNG()) : std::uniform_real_distribution<float>(m_startLRExponent, other->m_startLRExponent)(*getRNG());
			child->m_LRExponentDelta = (m_LRExponentDelta > other->m_LRExponentDelta) ? std::uniform_real_distribution<float>(other->m_LRExponentDelta, m_LRExponentDelta)(*getRNG()) : std::uniform_real_distribution<float>(m_LRExponentDelta, other->m_LRExponentDelta)(*getRNG());

			// Here, m_procBool1 is 'used in child'.
			for (auto& c : m_chromosomes) { c.second.m_procBool1 = false; }
			for (auto& c : other->m_chromosomes) { c.second.m_procBool1 = false; }

			if (m_inputCount != other->m_inputCount) { WARN("id{0}: Input counts do not match on paired genomes!", getID()); }
			if (m_outputCount != other->m_outputCount) { WARN("id{0}: Output counts do not match on paired genomes!", getID()); }

			std::vector<bool> outputToParentA(m_outputCount);
			std::vector<uint> outputNeuronID(m_outputCount);

			std::uniform_int_distribution boolDist(0, 1);
			for (uint i = 0; i < m_outputCount; i++) {
				// Here we're chosing which parent to take a given output neuron from.
				outputToParentA[i] = (bool)boolDist(*getRNG());

				// Get the chosen output neuron, and its ID.
				auto rIter = outputToParentA[i] ? m_chromosomes.rbegin() : other->m_chromosomes.rbegin();
				int steps = m_outputCount - i - 1;
				for (int j = 0; j < steps; j++) { rIter++; }
				outputNeuronID[i] = rIter->first;

				// Traverse up the tree from the chosen output neuron
				std::queue<uint> idsToChain;
				idsToChain.push(outputNeuronID[i]);
				Genome& target = outputToParentA[i] ? *this : *other;
				while (!idsToChain.empty()) {
					uint id = idsToChain.front();

					if (!target.m_chromosomes[id].m_procBool1) {
						// If hasn't already been chained...
						target.m_chromosomes[id].m_procBool1 = true;

						for (auto tID : target.m_chromosomes[id].m_startingWeights) {
							if (tID.first >= m_inputCount) { idsToChain.push(tID.first); }
						}
					}

					idsToChain.pop();
				}

			}

			// Go through each parent and move relevant nodes across into child->
			// In child, procbool1 will now mean 'belonged to parent A, rather than parent B'.

			std::map<uint, uint> movedNeuronOverrides; // This is clumsy, but it works because it'll only ever be relevant for the second parent.
			bool inParentA = true; // Changes to false in second iteration.
			Genome * p = this; // Changes to other in second iteration.
			
			for (uint i = 0; i < 2 && allShipshape; i++) {
				for (auto iter = p->m_chromosomes.begin(); iter != p->m_chromosomes.end() && allShipshape; iter++) {
					if (iter->second.m_procBool1) {
						if (child->m_chromosomes.find(iter->first) == child->m_chromosomes.end()) {
							child->m_chromosomes[iter->first] = iter->second;

							// Because we've only flood-filled the trees from the outputs backward,
							// forward references may be broken. We'll need to rebuild them from scratch.
							auto& tc = child->m_chromosomes[iter->first];
							tc.m_procBool1 = (i == 0);
							tc.m_references.clear();
							std::vector<uint> toChange;
							for (auto& sw : tc.m_startingWeights) {
								if (movedNeuronOverrides.find(sw.first) != movedNeuronOverrides.end()) {
									child->m_chromosomes[movedNeuronOverrides[sw.first]].m_references.insert(iter->first);
									toChange.push_back(sw.first);
								}
								else if (sw.first >= m_inputCount) { child->m_chromosomes[sw.first].m_references.insert(iter->first); }
							}
							for (auto id : toChange) {
								uint destID = movedNeuronOverrides[id];
								if (tc.m_startingWeights.find(destID) != tc.m_startingWeights.end()) {
									ERRORM("id{0}: tc.m_startingtWeights already contaings connection to a movedNeuronOverrides id. Something has gone badly wrong.", getID());
								}

								tc.m_startingWeights[destID] = tc.m_startingWeights[id];
								tc.m_startingWeights.erase(id);
							}

							if (child->m_chromosomes[iter->first].m_startingWeights.empty()) {
								INFO("id(0): Detected invalid neuron while transferring from parent (id{1}) to child (id{2}). Adding random connections until valid.", getID(), p->getID(), child->getID());
								while (child->m_chromosomes[iter->first].m_startingWeights.empty()) {
									child->addRandomConnectionToNeuron(iter->first);
								}
								INFO("id(0): Resolved invalid neuron.", getID());
							}
						}
						else {
							// There's already a node in child of that id.
							// So long as neither are an output, we'll combine it.
							// If it is, things get messy.
							if (inParentA) {
								ERRORM("id{0}: Existing chromosome of inserting ID detected in first iteration of genome merging. Something has gone badly wrong.", getID());
							}

							if (!child->m_chromosomes[iter->first].m_isAnOutput && !iter->second.m_isAnOutput) {
								// Neither are an output.
								auto& sc = p->m_chromosomes[iter->first];
								auto& tc = child->m_chromosomes[iter->first];

								uint desiredWeightCount;
								{
									uint larger, smaller;
									if (sc.m_startingWeights.size() < tc.m_startingWeights.size()) {
										larger = tc.m_startingWeights.size();
										smaller = sc.m_startingWeights.size();
									}
									else {
										smaller = tc.m_startingWeights.size();
										larger = sc.m_startingWeights.size();
									}

									float sigma = (larger - smaller) * 0.5f;
									sigma = std::max(sigma, 1.0f);

									desiredWeightCount = (uint)std::normal_distribution((float)larger, sigma)(*getRNG());
								}

								if ((bool)boolDist(*getRNG())) {
									tc.m_startingBias = sc.m_startingBias;
									tc.m_procBool1 = false;
								}
								for (auto& sw : sc.m_startingWeights) {
									uint swID = sw.first;
									if (movedNeuronOverrides.find(swID) != movedNeuronOverrides.end()) { swID = movedNeuronOverrides[swID]; }

									if (tc.m_startingWeights.find(swID) == tc.m_startingWeights.end()) {
										// New connection
										tc.m_startingWeights[swID] = sw.second;
										if (swID >= m_inputCount) { child->m_chromosomes[swID].m_references.insert(iter->first); }
									}
									else {
										// Existing connection.
										if ((bool)boolDist(*getRNG())) { tc.m_startingWeights[swID] = sw.second; }
									}
								}

								desiredWeightCount = std::clamp(desiredWeightCount, 2u, (uint)tc.m_startingWeights.size());
								while (tc.m_startingWeights.size() > desiredWeightCount) {
									auto delIt = tc.m_startingWeights.begin();
									uint offset = std::uniform_int_distribution<uint>(0, tc.m_startingWeights.size() - 1)(*getRNG());
									std::advance(delIt, offset);

									if (delIt->first >= m_inputCount) { child->m_chromosomes[delIt->first].m_references.erase(iter->first); }
									tc.m_startingWeights.erase(delIt);
								}

								if (tc.m_startingWeights.empty()) {
									INFO("id(0): Detected invalid neuron while transferring from parent (id{1}) to child (id{2}). Adding random connections until valid.", getID(), p->getID(), child->getID());
									while (tc.m_startingWeights.empty()) {
										child->addRandomConnectionToNeuron(iter->first);
									}
									INFO("id(0): Resolved invalid neuron.", getID());
								}
							}
							else {
								// Ah, fuck. It's an output.
								INFO("id{0}: Detected conflicting output node IDs in genome combination; Resolving...", getID());

								auto& sc = p->m_chromosomes[iter->first];
								auto& tc = child->m_chromosomes[iter->first];

								// Try moving the incoming node's ID forward.
								uint earliestSlot = (uint)std::max(sc.m_startingWeights.rbegin()->first + 1u, m_inputCount); // Inclusive
								uint choice = 0;
								for (uint j = iter->first - 1; j >= earliestSlot && choice == 0; --j) {
									if (child->m_chromosomes.find(j) == child->m_chromosomes.end()) { choice = j; }
								}

								if (choice == 0) {
									// Oh dear. Failure. This'll trigger a total flush and retry.
									WARN("id{0}: Detected conflicting output node IDs in genome combination; Failed to resolve.", getID());
									allShipshape = false;
								}
								else {
									INFO("id{0}: Detected conflicting output node IDs in genome combination; Successfully resolved.", getID());
									// Hoorah! Add a few overrides, copy to the new slot!
									movedNeuronOverrides[iter->first] = choice;
									for (uint j = 0; j < m_outputCount; j++) {
										if (outputNeuronID[j] == iter->first && inParentA == outputToParentA[j]) {
											outputNeuronID[j] = choice;
										}
									}

									child->m_chromosomes[choice] = iter->second;

									// Because we're only flood-filling the trees from the outputs backward,
									// forward references may be broken. We'll need to rebuild them from scratch.
									auto& tc = child->m_chromosomes[choice];
									tc.m_procBool1 = (i == 0);
									tc.m_references.clear();
									for (auto& sw : tc.m_startingWeights) {
										if (movedNeuronOverrides.find(sw.first) != movedNeuronOverrides.end()) { child->m_chromosomes[movedNeuronOverrides[sw.first]].m_references.insert(choice); }
										else if (sw.first >= m_inputCount) { child->m_chromosomes[sw.first].m_references.insert(choice); }
									}

									if (tc.m_startingWeights.empty()) {
										INFO("id(0): Detected invalid neuron while transferring from parent (id{1}) to child (id{2}). Adding random connections until valid.", getID(), p->getID(), child->getID());
										while (tc.m_startingWeights.empty()) {
											child->addRandomConnectionToNeuron(choice);
										}
										INFO("id(0): Resolved invalid neuron.", getID());
									}
								}
							}
						}
					}
				}

				inParentA = false;
				p = other;
			}

			// Right! We have successfully created a hybrid monstrosity!
			// Now, lets make sure the output nodes are lined up correctly.
			// Then, we'll try merging random adjacent nodes to tie the two separate networks into one.
			
			// First, we find any non-output neurons that have too high an ID, and try to move them backward.
			if (allShipshape) {
				auto rIter = child->m_chromosomes.rbegin();
				std::vector<uint> tooHighIDs;
				uint foundoutputs = 0;
				uint lowestFoundOutputID = rIter->first;

				for (; rIter != child->m_chromosomes.rend() && foundoutputs < m_outputCount; ++rIter) {
					if (rIter->second.m_isAnOutput) {
						foundoutputs++;
						lowestFoundOutputID = rIter->first;
					}
					else {
						tooHighIDs.push_back(rIter->first);
					}
				}

				if (foundoutputs != m_outputCount) {
					ERROR("Wrong number of outputs detected in child genome! Restarting child-creation process...");
					allShipshape = false;
				}
				else if (!tooHighIDs.empty()) {
					bool foundABigEnoughGap = false;
					uint lowestAvailableID; // Inclusive
					uint availabilityGap; // Number of free IDs.

					++rIter; // Move from pointing at the lowest output, to pointing at the highest valid non-output.

					while (rIter != child->m_chromosomes.rend() && !foundABigEnoughGap) {
						lowestAvailableID = rIter->first + 1; // Inclusive
						availabilityGap = lowestFoundOutputID - lowestAvailableID;

						if (availabilityGap >= tooHighIDs.size()) { foundABigEnoughGap = true; }
						else {
							tooHighIDs.push_back(rIter->first);
							++rIter;
						}
					}

					if (!foundABigEnoughGap) {
						WARN("id{0}: Could not find a large enough gap to rationalise non-output neurons in genome child creation. Restarting child-creation process...", getID());
						allShipshape = false;
					}
					else {
						int increment = (int)(availabilityGap / (tooHighIDs.size() + 1));
						uint currentDest = lowestFoundOutputID;
						for (auto iter = tooHighIDs.begin(); iter != tooHighIDs.end(); ++iter) {
							currentDest -= increment;
							child->moveNeuron(*iter, currentDest);
						}
					}
				}
			}

			// Next, we'll go through all the output neurons, and swap their IDs around as necessary to get them in the right order.
			if (allShipshape) {
				std::vector<uint> sortedOutputNeuronID = outputNeuronID;
				std::sort(outputNeuronID.begin(), outputNeuronID.end());

				std::map<uint, Chromosome> sortedOutputs;
				for (uint i = 0; i < m_outputCount; i++) {
					sortedOutputs[sortedOutputNeuronID[i]] = child->m_chromosomes[outputNeuronID[i]];
				}

				for (uint i = 0; i < m_outputCount; i++) {
					child->m_chromosomes[sortedOutputNeuronID[i]] = sortedOutputs[sortedOutputNeuronID[i]];
					auto& tc = child->m_chromosomes[sortedOutputNeuronID[i]];

					for (auto& sw : tc.m_startingWeights) {
						child->m_chromosomes[sw.first].m_references.erase(outputNeuronID[i]);
					}
				}

				// The following happens separately to avoid neurons referenced by multiple from having
				// new connections deleted by accident.
				for (uint i = 0; i < m_outputCount; i++) {
					auto& tc = child->m_chromosomes[sortedOutputNeuronID[i]];

					for (auto& sw : tc.m_startingWeights) {
						child->m_chromosomes[sw.first].m_references.insert(sortedOutputNeuronID[i]);
					}
				}
			}

			// Last hard bit: merge random adjacent non-output nodes.
			uint desiredNodeCount;
			if (allShipshape) {
				// Work out how many nodes we actually want.
				desiredNodeCount = (m_chromosomes.size() + other->m_chromosomes.size()) / 2u;
				desiredNodeCount = std::clamp(desiredNodeCount, NEURON_COUNT_MIN, NEURON_COUNT_MAX);
				std::normal_distribution dncDist((float)desiredNodeCount, (float)desiredNodeCount * 0.15f);
				do { desiredNodeCount = (uint)dncDist(*getRNG()); }
				while (
					desiredNodeCount < NEURON_COUNT_MIN &&
					desiredNodeCount > NEURON_COUNT_MAX &&
					desiredNodeCount > child->m_chromosomes.size()
					);

				std::uniform_int_distribution boolDist(0, 1);

				bool runOutOfMerges = false;
				while (child->m_chromosomes.size() > desiredNodeCount && !runOutOfMerges) {
					int difference = (int)(child->m_chromosomes.size() - desiredNodeCount); // How many merges are needed.
					int increment = (int)(child->m_chromosomes.size() / difference); // Roughly how many neurons per merge.

					uint merges = 0;
					int i = 1;
					int nextIncrement = (int)(increment / 2);
					auto lastIter = child->m_chromosomes.begin();
					auto iter = child->m_chromosomes.begin();
					++iter;

					while (merges < difference && iter != child->m_chromosomes.end()) {
						if (i >= nextIncrement && !iter->second.m_isAnOutput) {
							if (lastIter->second.m_procBool1 != iter->second.m_procBool1) {
								// Time for another merge, not an output, and from different parents.

								// Starting Bias
								bool overrideFirst = boolDist(*getRNG());
								if (overrideFirst) {
									lastIter->second.m_procBool1 = iter->second.m_procBool1;
									lastIter->second.m_startingBias = iter->second.m_startingBias;
								}

								// Desired Weight count
								uint desiredWeightCount;
								{
									uint larger, smaller;
									if (lastIter->second.m_startingWeights.size() < iter->second.m_startingWeights.size()) {
										larger = iter->second.m_startingWeights.size();
										smaller = lastIter->second.m_startingWeights.size();
									}
									else {
										smaller = iter->second.m_startingWeights.size();
										larger = lastIter->second.m_startingWeights.size();
									}

									float sigma = (larger - smaller) * 0.5f;
									sigma = std::max(sigma, 1.0f);

									desiredWeightCount = (uint)std::normal_distribution((float)larger, sigma)(*getRNG());
								}

								// Remove interlinks
								lastIter->second.m_references.erase(iter->first);
								iter->second.m_startingWeights.erase(lastIter->first);

								// Starting Weights.
								for (auto& sw : iter->second.m_startingWeights) {
									if (sw.first > m_inputCount) {
										child->m_chromosomes[sw.first].m_references.erase(iter->first);
									}

									auto esw = lastIter->second.m_startingWeights.find(sw.first);
									if (esw != lastIter->second.m_startingWeights.end()) {
										// Already exists, merge.
										esw->second = boolDist(*getRNG()) ? sw.second : esw->second;
									}
									else {
										// Add it.
										lastIter->second.m_startingWeights[sw.first] = sw.second;
										if (sw.first > m_inputCount) {
											child->m_chromosomes[sw.first].m_references.insert(lastIter->first);
										}
									}
								}

								desiredWeightCount = std::clamp(desiredWeightCount, 2u, (uint)lastIter->second.m_startingWeights.size());
								while (lastIter->second.m_startingWeights.size() > desiredWeightCount) {
									auto delIt = lastIter->second.m_startingWeights.begin();
									uint offset = std::uniform_int_distribution<uint>(0, lastIter->second.m_startingWeights.size() - 1)(*getRNG());
									std::advance(delIt, offset);

									if (delIt->first >= m_inputCount) { child->m_chromosomes[delIt->first].m_references.erase(lastIter->first); }
									lastIter->second.m_startingWeights.erase(delIt);
								}

								// References
								for (auto& r : iter->second.m_references) {
									auto& t = child->m_chromosomes[r];
									auto esw = t.m_startingWeights.find(lastIter->first);
									if (esw != t.m_startingWeights.end()) {
										esw->second = boolDist(*getRNG()) ? t.m_startingWeights[iter->first] : esw->second;
									}
									else {
										t.m_startingWeights[lastIter->first] = t.m_startingWeights[iter->first];
										lastIter->second.m_references.insert(r);
									}

									t.m_startingWeights.erase(iter->first);
								}

								iter = child->m_chromosomes.erase(iter);
								lastIter = iter;
								--lastIter;
								if (iter == child->m_chromosomes.end()) { --iter; }

								++merges;
								nextIncrement += increment;
							}
						}

						++i;
						++lastIter;
						++iter;
					}
					if (merges == 0u) { runOutOfMerges = true; }
				}

				// We've run out of good merges.
				// Instead, just delete random neurons.
				if (runOutOfMerges) {
					INFO("id{0}: Ran out of valid merges during neuron culling process. Will proceed to delete {1} neurons at random...", getID(), (child->m_chromosomes.size() - desiredNodeCount));
				}

				while (child->m_chromosomes.size() > desiredNodeCount) {
					uint choice = std::uniform_int_distribution(0u, (uint)child->m_chromosomes.size() - (m_outputCount + 1))(*getRNG());
					auto choiceIter = child->m_chromosomes.begin();
					std::advance(choiceIter, choice);

					child->deleteNeuron(choiceIter->first);
				}
			}

			// Finally: prune tree and rationalise outputs. If necessary, add random neurons and repeat.
			if (allShipshape) {
				bool goRoundAgain;
				do {
					goRoundAgain = false;
					
					child->cleanupOutputs();
					child->pruneTree();

					if (child->m_chromosomes.size() < NEURON_COUNT_MIN) {
						goRoundAgain = true;
						int neuronsToAdd = desiredNodeCount - child->m_chromosomes.size();
						for (uint i = 0; i < neuronsToAdd; i++) { child->addRandomNeuron(false); }
					}
				} while (goRoundAgain);
			}

			// All done! And, if allShipShape, then we'll return a brand new child!
			if (!allShipshape) {
				WARN("Child creation between id{0} and id{1} deteced failure. Looping and retrying...", getID(), other->getID());
			}
		}
		INFO("Completed child-creation operation between id{0} and id{1}. Child (id{2}) has {3} neurons.", getID(), other->getID(), child->getID(), child->m_chromosomes.size());
		return child;
	}

	void Genome::writeToFile(std::ofstream & file)
	{
		Utils::FileOutHandler foh(file);
		
		foh.writeItem(m_populationID);			// uint
		foh.writeItem(m_generation);			// uint

		foh.writeItem(m_tested);				// bool
		foh.writeItem(m_rank);					// uint
		foh.writeItem(m_metrics.m_trainingBufferAverageCost);	// float
		foh.writeItem(m_metrics.m_trainingBufferAverageCACost);	// float
		foh.writeItem(m_metrics.m_trainingBufferAccuracy);		// float
		foh.writeItem(m_metrics.m_testingBufferAverageCost);	// float
		foh.writeItem(m_metrics.m_testingBufferAverageCACost);	// float
		foh.writeItem(m_metrics.m_testingBufferAccuracy);		// float

		foh.writeItem(m_inputCount);			// uint
		foh.writeItem(m_outputCount);			// uint

		foh.writeItem(m_lowestOutputNeuronID);	// uint

		foh.writeItem(m_startLRExponent);		// float
		foh.writeItem(m_LRExponentDelta);		// float

		foh.writeItem((uint)m_chromosomes.size());	// uint
		for (auto& c : m_chromosomes) {
			foh.writeItem(c.first);			// uint (ID)

			foh.writeItem(c.second.m_startingBias);	// float
			foh.writeItem(c.second.m_isAnOutput);		// bool

			foh.writeItem((uint)c.second.m_startingWeights.size());	// uint
			for (auto& w : c.second.m_startingWeights) {
				foh.writeItem(w.first);	// uint (ID)
				foh.writeItem(w.second);	// float (weight)
			}

			foh.writeItem((uint)c.second.m_references.size());	// uint
			for (auto& r : c.second.m_references) {
				foh.writeItem(r);			// uint (ID)
			}
		}
	}
}