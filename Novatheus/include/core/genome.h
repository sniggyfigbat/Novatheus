#pragma once

namespace Core {
	class Chromosome {
	public:
		//uint m_id;
		std::map<uint, float> m_startingWeights;
		float m_startingBias;

		std::set<uint> m_references; // Where this gets referenced from.

		bool m_prunable = false;

		Chromosome(float startingBias = 0.0f) : m_startingBias(startingBias) {}
	};
	
	class Genome : public Utils::HasForwarder {
	private:
		uint m_inputCount;	// How many IDs are reserved at the front for input values.
		uint m_outputCount; // How many of the rearmost neurons are read as the output.

		std::map<uint, Chromosome> m_chromosomes;

		bool deleteNeuron(uint ID);					// Returns whether doing do has left any hanging neurons, requiring a potential pruning.
		void pruneTree();							// Cycles from back to front, pruning any neuron not in some way linked to the outputs.
		uint addRandomNeuron();						// Adds a random neuron to the genome. Obviously. Returns its ID.
		void addRandomConnectionToNeuron(uint ID);	// Adds a random connection to a previous ID to the specified neuron.
	public:
		Genome(Utils::Forwarder * forwarder, uint inputCount, uint outputCount) :
			Utils::HasForwarder(forwarder),
			m_inputCount(inputCount),
			m_outputCount(outputCount)
		{
			if (outputCount > NEURON_COUNT_MIN) { WARN("Output count exceeds NEURON_COUNT_MIN in genome constructor."); }
			// TODO: Finish constructors.
		}
		~Genome() {}

		void mutate(bool supermutate = false);
		Genome operator+(const Genome & s) const; // TODO: Implement.
	};
}
