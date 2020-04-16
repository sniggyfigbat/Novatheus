#pragma once
#include <array>
#include <random>
#include "utils\logger.h"

namespace Core {
	enum MutationTypes {
		NeuronAddition = 0,
		NeuronDeletion,
		NeuronIDDrift,
		NeuronBiasDrift,
		ConnectionAddition,
		ConnectionDeletion,
		ConnectionIDDrift,
		ConnectionWeightDrift
	};
#define MutationTypesCount 8

	class MutationTable {
	private:
		std::array<unsigned int, MutationTypesCount> m_weights;
		unsigned int m_totalWeight = 0u;

		MutationTypes* mp_mutationTable;
		MutationTypes m_defaultValue = ConnectionWeightDrift;

		std::default_random_engine* p_rng = nullptr;
		std::uniform_int_distribution<int> m_distribution;
	public:
		MutationTable() {
			m_weights = {1u, 1u, 2u, 3u, 2u, 2u, 1u, 5u};
			setup();
		};
		MutationTable(const std::array<unsigned int, MutationTypesCount> weights) : m_weights(weights) {
			setup();
		}
		~MutationTable() {
			delete[] mp_mutationTable;
		}

		void setup() {
			for (auto w : m_weights) { m_totalWeight += w; }
			m_distribution = std::uniform_int_distribution<int>(0, m_totalWeight - 1);

			mp_mutationTable = new MutationTypes[m_totalWeight];
			unsigned int largestWeight = 0;

			unsigned int index = 0;
			for (unsigned int t = 0; t < MutationTypesCount; t++) {
				for (unsigned int i = 0; i < m_weights[t]; i++) {
					mp_mutationTable[index] = (MutationTypes)t;
					index++;
				}

				if (m_weights[t] > m_weights[largestWeight]) { largestWeight = t; }
			}

			m_defaultValue = (MutationTypes)largestWeight;
		}

		MutationTypes operator [](int i) const {
			if (i >= 0 && i < m_totalWeight) { return mp_mutationTable[i]; }
			else {
				WARN("MutationTable bracket operator called with invalid index ({0}). Returning default Mutation Type ({1}).", i, m_defaultValue);
				return m_defaultValue;
			}
		}
		MutationTypes& operator [](int i) {
			if (i >= 0 && i < m_totalWeight) { return mp_mutationTable[i]; }
			else {
				WARN("MutationTable bracket operator called with invalid index ({0}). Returning index 0 instead.", i, m_defaultValue);
				return mp_mutationTable[0];
			}
		}

		unsigned int getTotalWeight() { return m_totalWeight; }
		const std::array<unsigned int, MutationTypesCount>& getWeights() { return m_weights; }

		void setRandomEngine(std::default_random_engine* rng) { p_rng = rng; }
		MutationTypes getRandomMutationType() {
			if (p_rng == nullptr) {
				WARN("getRandomMutationType() called without valid random engine!");
				return m_defaultValue;
			}

			return (mp_mutationTable[m_distribution(*p_rng)]);
		}
	};
}