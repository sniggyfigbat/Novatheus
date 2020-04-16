#pragma once

namespace Core {
	class Chromosome {
	public:
		struct Weight {
		public:
			uint m_id; // Id of referred-to neuron.
			float m_value; // Starting weight of referred-to neuron.
			Weight(uint id, float value) : m_id(id), m_value(value) {};
		};
		
		uint m_id;
		std::vector<Weight> m_startingWeights;
		float m_startingBias;
	};
	
	class Genome : public Utils::HasForwarder {
	private:
		std::map<uint, Chromosome> m_chromosomes;
		std::set<uint> m_usedChromosomeIDs;
	public:
		Genome(Utils::Forwarder * forwarder) : Utils::HasForwarder(forwarder) {
		}
		~Genome() {}

		void mutate(bool supermutate = false);
	};
}
