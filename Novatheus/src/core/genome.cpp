#include "pch.h"
#include "utils\utils.h"
#include "core\genome.h"

namespace Core {
	Genome::DeleteNeuronReturnStruct Genome::deleteNeuron(uint ID)
	{
		// Important! Note that this method is not used by pruneTree - any changes will need to be added to the method separately.
		auto targetIter = m_chromosomes.find(ID);
		if (targetIter == m_chromosomes.end()) { return false; }

		auto& target = targetIter->second;
		auto references = target.m_references;

		bool requiresPruning = false;
		bool requiresOutputCleanup = target.m_isAnOutput;

		// It's conceivably possible that we could chain-delete something such that iterators get buggered.
		while (target.m_startingWeights.size() > 0) {
			uint weightID = target.m_startingWeights.begin()->first;

			if (weightID >= m_inputCount) {
				auto weightIter = m_chromosomes.find(weightID);
				if (weightIter != m_chromosomes.end()) {
					weightIter->second.m_references.erase(ID);
					if (weightIter->second.m_references.size() == 0) { requiresPruning = true; }
				}
			}

			target.m_startingWeights.erase(target.m_startingWeights.begin());
		}

		m_chromosomes.erase(ID);

		for (auto r : references) {
			auto targetR = m_chromosomes.find(r);
			if (targetR != m_chromosomes.end()) {
				m_chromosomes[r].m_startingWeights.erase(ID);
				if (m_chromosomes[r].m_startingWeights.size() == 0) {
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

			auto rIter = m_chromosomes.rbegin();
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
			for (uint id : prunableIDs) { m_chromosomes.erase(id); }

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
				if (rIter->second.m_startingWeights.size() == 0) {
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

	uint Genome::addRandomNeuron(bool allowOutput)
	{
		uint minID = m_inputCount;
		uint maxID = allowOutput ? NEURON_COUNT_MAX * 8u : m_lowestOutputNeuronID;

		uint newID;
		std::uniform_int_distribution idDist(minID, maxID);

		do { newID = idDist(*getRNG()); }
		while (m_chromosomes.find(newID) != m_chromosomes.end());
		
		// Add neuron.
		float startingBias = std::normal_distribution(0.0f, 2.0f)(*getRNG());
		m_chromosomes[newID] = Chromosome(startingBias);

		// Number of connections to add:
		uint availableSlots = m_chromosomes.size() + m_inputCount - 1; // -1 for self-nonconnectibility.
		int maxSlots = std::min(availableSlots, NEURON_CONNECTION_COUNT_MAX);

		float average = std::min((float)NEURON_CONNECTION_COUNT_MAX * 0.25f, (float)availableSlots * 0.5f); // Usually large numbers.
		average = std::max(average, std::min(10.0f, (float)availableSlots)); // Checked against consistently small numbers.

		std::normal_distribution<float> conDist(average, std::max(average * 0.25f, 1.0f));

		int conCount;
		do { int conCount = (int)std::round(conDist(*getRNG())); }
		while (conCount < 0 || conCount > maxSlots);

		for (uint i = 0; i < conCount; i++) { addRandomConnectionToNeuron(newID, true); }

		return newID;
	}

	void Genome::addRandomConnectionToNeuron(uint ID, bool allowReferencedOutputs)
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

			if (newLinkID < ID && newLinkID >= m_lowestOutputNeuronID && !allowReferencedOutputs) { invalid = true; }
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

	void Genome::moveNeuron(uint sourceID, uint destID, bool warnInvalidMove)
	{
		auto target = m_chromosomes.find(sourceID);
		if (target == m_chromosomes.end()) {
			if (warnInvalidMove) { INFO("Invalid neuron move called in chromosome; No neuron found of source ID."); }
			return;
		}

		auto dest = m_chromosomes.find(destID);
		bool destIsFilled = (dest != m_chromosomes.end());
		if (destIsFilled && warnInvalidMove) { INFO("Questionable neuron move called in chromosome; Neuron already present at dest ID."); }
		
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
		bool requiresOutputCleanup = false;

		// Do mutations:
		for (uint i = 0; i < mutations; i++) {
			MutationTypes mt = getRandomMutationType(); // Properly weighted for appropriate types. See mutation.h, via forwarder.h.
			uint targetID = (uint)std::uniform_int_distribution<int>(0, (chromaCount - 1))(*getRNG());
			
			auto setNav = m_chromosomes.begin();
			std::next(setNav, targetID);
			targetID = setNav->first;
			auto& target = m_chromosomes[targetID];

			bool targetIsAnOutput = (targetID >= m_lowestOutputNeuronID);

			switch (mt) {
			case MutationTypes::NeuronAddition:
				if (m_chromosomes.size() < NEURON_COUNT_MAX) {
					addRandomNeuron();
					break;
				}
				// Intentional cascade. Doubles chance of tree reduction if at max size.
			case MutationTypes::NeuronDeletion:
				if (m_chromosomes.size() > NEURON_COUNT_MIN && !targetIsAnOutput) {
					//if (deleteNeuron(targetID)) { pruneTree(); };
					auto ret = deleteNeuron(targetID);
					requiresPruning |= ret.m_requiresPruning;
					requiresOutputCleanup |= ret.m_requiresOutputCleanup;
				}
				break;
			case MutationTypes::NeuronIDDrift:
				// Bounds
				uint max, min;
				if (targetIsAnOutput) {
					auto iter = m_chromosomes.find(targetID);
					bool isLast = (targetID == m_chromosomes.rbegin()->first);
					max = isLast ? NEURON_COUNT_MAX * 8u : (++iter)->first;
					--iter;
					--iter;
					min = iter->first;
				}
				else {
					max = *(target.m_references.rbegin());	// Exclusive
					min = m_inputCount - 1;				// Also exclusive.
					for (auto& c : target.m_startingWeights) { min = std::max(min, c.first); }
				}

				// Generate new ID
				std::normal_distribution<float> dist((float)targetID, ((float)targetID) * 0.15f);
				uint newID = max;
				while (newID <= min || newID >= max || m_chromosomes.find(newID) != m_chromosomes.end()) {
					newID = (uint)std::round(dist(*getRNG()));
				}

				moveNeuron(targetID, newID);
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
				else if (!targetIsAnOutput && m_chromosomes.size() > NEURON_COUNT_MIN) {
					//if (deleteNeuron(targetID)) { pruneTree(); };
					auto ret = deleteNeuron(targetID);
					requiresPruning |= ret.m_requiresPruning;
					requiresOutputCleanup |= ret.m_requiresOutputCleanup;
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

		if (requiresOutputCleanup) { requiresPruning |= cleanupOutputs(); }
		if (requiresPruning) { pruneTree(); }
	}

	Genome Genome::operator+(Genome& other)
	{
		Genome child(mp_forwarder, m_inputCount, m_outputCount);
		
		bool allShipshape = false;
		while (!allShipshape) {
			allShipshape = true;
			child = Genome(mp_forwarder, m_inputCount, m_outputCount);

			// Here, m_procBool1 is 'used in child'.
			for (auto& c : m_chromosomes) { c.second.m_procBool1 = false; }
			for (auto& c : other.m_chromosomes) { c.second.m_procBool1 = false; }

			if (m_inputCount != other.m_inputCount) { WARN("Input counts do not match on paired genomes!"); }
			if (m_outputCount != other.m_outputCount) { WARN("Output counts do not match on paired genomes!"); }

			Genome child(mp_forwarder, m_inputCount, m_outputCount);

			std::vector<bool> outputToParentA(m_outputCount);
			std::vector<uint> outputNeuronID(m_outputCount);

			std::uniform_int_distribution boolDist(0, 1);
			for (uint i = 0; i < m_outputCount; i++) {
				// Here we're chosing which parent to take a given output neuron from.
				outputToParentA[i] = (bool)boolDist(*getRNG());

				// Get the chosen output neuron, and its ID.
				auto rIter = outputToParentA[i] ? m_chromosomes.rbegin() : other.m_chromosomes.rbegin();
				int steps = m_outputCount - i - 1;
				for (int j = 0; j < steps; j++) { rIter++; }
				outputNeuronID[i] = rIter->first;

				// Traverse up the tree from the chosen output neuron
				std::queue<uint> idsToChain;
				idsToChain.push(outputNeuronID[i]);
				Genome& target = outputToParentA[i] ? *this : other;
				while (idsToChain.size() > 0) {
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

			// Go through each parent and move relevant nodes across into child.
			// In child, procbool1 will now mean 'belonged to parent A, rather than parent B'.

			std::map<uint, uint> movedNeuronOverrides; // This is clumsy, but it works because it'll only ever be relevant for the second parent.
			bool inParentA = true; // Changes to false in second iteration.
			auto& p = *this; // Changes to other in second iteration.
			
			for (uint i = 0; i < 2 && allShipshape; i++) {
				for (auto iter = p.m_chromosomes.begin(); iter != p.m_chromosomes.end() && allShipshape; iter++) {
					if (iter->second.m_procBool1) {
						if (child.m_chromosomes.find(iter->first) == child.m_chromosomes.end()) {
							child.m_chromosomes[iter->first] = iter->second;

							// Because we've only flood-filled the trees from the outputs backward,
							// forward references may be broken. We'll need to rebuild them from scratch.
							auto& tc = child.m_chromosomes[iter->first];
							tc.m_procBool1 = (i == 0);
							tc.m_references.clear();
							for (auto& sw : tc.m_startingWeights) {
								if (movedNeuronOverrides.find(sw.first) != movedNeuronOverrides.end) { child.m_chromosomes[movedNeuronOverrides[sw.first]].m_references.insert(iter->first); }
								else if (sw.first >= m_inputCount) { child.m_chromosomes[sw.first].m_references.insert(iter->first); }
							}
						}
						else {
							// There's already a node in child of that id.
							// So long as neither are an output, we'll combine it.
							// If it is, things get messy.
							if (!child.m_chromosomes[iter->first].m_isAnOutput && !iter->second.m_isAnOutput) {
								// Neither are an output.
								auto& sc = p.m_chromosomes[iter->first];
								auto& tc = child.m_chromosomes[iter->first];

								if ((bool)boolDist(*getRNG())) { tc.m_startingBias = sc.m_startingBias; }
								for (auto& sw : sc.m_startingWeights) {
									uint swID = sw.first;
									if (movedNeuronOverrides.find(swID) != movedNeuronOverrides.end) { swID = movedNeuronOverrides[swID]; }

									if (tc.m_startingWeights.find(swID) == tc.m_startingWeights.end()) {
										// New connection
										if (tc.m_startingWeights.size() < NEURON_CONNECTION_COUNT_MAX) {
											tc.m_startingWeights[swID] = sw.second;
											if (swID >= m_inputCount) { child.m_chromosomes[swID].m_references.insert(iter->first); }
										}
									}
									else {
										// Existing connection.
										if ((bool)boolDist(*getRNG())) { tc.m_startingWeights[swID] = sw.second; }
									}
								}
							}
							else {
								// Ah, fuck. It's an output.
								INFO("Detected conflicting output node IDs in genome combination; Resolving...");

								auto& sc = p.m_chromosomes[iter->first];
								auto& tc = child.m_chromosomes[iter->first];

								// Try moving the incoming node's ID forward.
								uint earliestSlot = (uint)std::max(sc.m_startingWeights.rend()->first, m_inputCount);
								uint choice = 0;
								for (uint j = iter->first; j > earliestSlot && choice == 0; --j) {
									auto f = child.m_chromosomes.find(j);
									if (f == child.m_chromosomes.end()) { choice = j; }
								}

								if (choice == 0) {
									// Oh dear. Failure. This'll just trigger a total flush and retry.
									WARN("Detected conflicting output node IDs in genome combination; Failed to resolve.");
									allShipshape = false;
								}
								else {
									INFO("Detected conflicting output node IDs in genome combination; Successfully resolved.");
									// Hoorah! Add a few overrides, copy to the new slot!
									if (inParentA) { WARN("movedNeuronOverrides addition detected in first iteration. Something has gone badly wrong."); }
									movedNeuronOverrides[iter->first] = choice;
									for (uint j = 0; j < m_outputCount; j++) {
										if (outputNeuronID[j] == iter->first && inParentA == outputToParentA[j]) {
											outputNeuronID[j] = choice;
										}
									}

									child.m_chromosomes[choice] = iter->second;

									// Because we're only flood-filling the trees from the outputs backward,
									// forward references may be broken. We'll need to rebuild them from scratch.
									auto& tc = child.m_chromosomes[choice];
									tc.m_procBool1 = (i == 0);
									tc.m_references.clear();
									for (auto& sw : tc.m_startingWeights) {
										if (movedNeuronOverrides.find(sw.first) != movedNeuronOverrides.end) { child.m_chromosomes[movedNeuronOverrides[sw.first]].m_references.insert(choice); }
										else if (sw.first >= m_inputCount) { child.m_chromosomes[sw.first].m_references.insert(choice); }
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
			// Now, lets rmake sure the output nodes are lined up correctly.
			// Then, we'll try merging random adjacent nodes to tie the two separate networks into one.
			
			// First, we find any non-output neurons that have too high an ID, and try to move them backward.
			if (allShipshape) {
				auto rIter = child.m_chromosomes.rbegin();
				std::vector<uint> tooHighIDs;
				uint foundoutputs = 0;
				uint lowestFoundOutputID = rIter->first;

				for (; rIter != child.m_chromosomes.rend() && foundoutputs < m_outputCount; ++rIter) {
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
				else {
					bool foundABigEnoughGap = false;
					uint lowestAvailableID; // Inclusive
					uint availabilityGap; // Number of free IDs.

					while (rIter != child.m_chromosomes.rend() && !foundABigEnoughGap) {
						lowestAvailableID = rIter->first + 1; // Inclusive
						availabilityGap = lowestFoundOutputID - lowestAvailableID;

						if (availabilityGap >= tooHighIDs.size()) { foundABigEnoughGap = true; }
						else {
							tooHighIDs.push_back(rIter->first);
							++rIter;
						}
					}

					if (!foundABigEnoughGap) {
						WARN("Could not find a large enough gap to rationalise non-output neurons in genome child creation. Restarting child-creation process...");
						allShipshape = false;
					}
					else {
						int increment = (int)(availabilityGap / tooHighIDs.size());
						uint currentDest = lowestFoundOutputID;
						for (auto iter = tooHighIDs.begin(); iter != tooHighIDs.end(); ++iter) {
							currentDest -= increment;
							moveNeuron(*iter, currentDest);
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
					sortedOutputs[sortedOutputNeuronID[i]] = child.m_chromosomes[outputNeuronID[i]];
				}

				for (uint i = 0; i < m_outputCount; i++) {
					child.m_chromosomes[sortedOutputNeuronID[i]] = sortedOutputs[sortedOutputNeuronID[i]];
					auto& tc = child.m_chromosomes[sortedOutputNeuronID[i]];

					for (auto& sw : tc.m_startingWeights) {
						child.m_chromosomes[sw.first].m_references.erase(outputNeuronID[i]);
					}
				}

				// The following happens separately to avoid neurons referenced by multiple from having
				// new connections deleted by accident.
				for (uint i = 0; i < m_outputCount; i++) {
					auto& tc = child.m_chromosomes[sortedOutputNeuronID[i]];

					for (auto& sw : tc.m_startingWeights) {
						child.m_chromosomes[sw.first].m_references.insert(sortedOutputNeuronID[i]);
					}
				}
			}

			// Last hard bit: merge random adjacent non-output nodes.
			uint desiredNodeCount;
			if (allShipshape) {
				// Work out how many nodes we actually want.
				desiredNodeCount = (m_chromosomes.size() + other.m_chromosomes.size()) / 2u;
				desiredNodeCount = std::clamp(desiredNodeCount, NEURON_COUNT_MIN, NEURON_COUNT_MAX);
				std::normal_distribution dncDist((float)desiredNodeCount, (float)desiredNodeCount * 0.15f);
				do { desiredNodeCount = (uint)dncDist(*getRNG()); }
				while (
					desiredNodeCount > NEURON_COUNT_MIN &&
					desiredNodeCount < NEURON_COUNT_MAX &&
					desiredNodeCount < child.m_chromosomes.size()
					);

				std::uniform_int_distribution boolDist(0, 1);

				while (child.m_chromosomes.size() > desiredNodeCount) {
					int difference = (int)(child.m_chromosomes.size() - desiredNodeCount); // How many merges are needed.
					int increment = (int)(child.m_chromosomes.size() / difference); // Roughly how many neurons per merge.

					uint merges = 0;
					int i = 1;
					int nextIncrement = (int)(increment / 2);
					auto lastIter = child.m_chromosomes.begin();
					auto iter = child.m_chromosomes.begin();
					++iter;

					while (merges < difference && iter != child.m_chromosomes.end()) {
						if (i >= nextIncrement && !iter->second.m_isAnOutput) {
							if (lastIter->second.m_procBool1 != iter->second.m_procBool1) {
								// Time for another merge, not an output, and from different parents.

								// Starting Bias
								bool overrideFirst = boolDist(*getRNG());
								if (overrideFirst) {
									lastIter->second.m_procBool1 = iter->second.m_procBool1;
									lastIter->second.m_startingBias = iter->second.m_startingBias;
								}

								// Remove interlinks
								lastIter->second.m_references.erase(iter->first);
								iter->second.m_startingWeights.erase(lastIter->first);

								// Starting Weights.
								for (auto& sw : iter->second.m_startingWeights) {
									if (sw.first > m_inputCount) {
										child.m_chromosomes[sw.first].m_references.erase(iter->first);
									}

									auto esw = lastIter->second.m_startingWeights.find(sw.first);
									if (esw != lastIter->second.m_startingWeights.end()) {
										// Already exists, merge.
										esw->second = boolDist(*getRNG()) ? sw.second : esw->second;
									}
									else if (lastIter->second.m_startingWeights.size() <= NEURON_CONNECTION_COUNT_MAX) {
										// Add it.
										lastIter->second.m_startingWeights[sw.first] = sw.second;
										if (sw.first > m_inputCount) {
											child.m_chromosomes[sw.first].m_references.insert(lastIter->first);
										}
									}
								}

								// References
								for (auto& r : iter->second.m_references) {
									auto& t = child.m_chromosomes[r];
									auto esw = t.m_startingWeights.find(lastIter->first);
									if (esw != t.m_startingWeights.end()) {
										esw->second = boolDist(*getRNG()) ? t.m_startingWeights[iter->first] : esw->second;
									}
									else { t.m_startingWeights[lastIter->first] = t.m_startingWeights[iter->first]; }

									t.m_startingWeights.erase(iter->first);
								}

								iter = child.m_chromosomes.erase(iter);

								++merges;
								nextIncrement += increment;
							}
						}

						++i;
						++lastIter;
						++iter;
					}
				}
			}

			// Finally: prune tree and rationalise outputs. If necessary, add random neurons and repeat.
			if (allShipshape) {
				bool goRoundAgain;
				do {
					goRoundAgain = false;
					
					child.cleanupOutputs();
					child.pruneTree();

					if (child.m_chromosomes.size() < NEURON_COUNT_MIN) {
						goRoundAgain = true;
						int neuronsToAdd = desiredNodeCount - child.m_chromosomes.size();
						for (uint i = 0; i < neuronsToAdd; i++) { child.addRandomNeuron(false); }
					}
				} while (goRoundAgain);
			}

			// All done! And, if allShipShape, then we'll return a brand new child!
		}
		return child;
	}
}