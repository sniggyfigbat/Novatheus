#pragma once

namespace Core {
	class Chromosome {
	public:
		//uint m_id;
		std::map<uint, float> m_startingWeights;
		float m_startingBias;

		std::set<uint> m_references; // Where this gets referenced from.

		bool m_procBool1 = false; // Used for both pruning and genome combination. Basically a local variable, meaning dependant on method.
		bool m_isAnOutput = false; // Whether or not this is an output neuron.

		Chromosome(float startingBias = 0.0f) : m_startingBias(startingBias) {}
	};
	
	class Genome : public Utils::HasForwarder {
		friend class Network;
	private:
		uint m_inputCount;	// How many IDs are reserved at the front for input values.
		uint m_outputCount; // How many of the rearmost neurons are read as the output.

		std::map<uint, Chromosome> m_chromosomes;
		uint m_lowestOutputNeuronID;				// TODO

		struct DeleteNeuronReturnStruct {
		public:
			bool m_requiresPruning;
			bool m_requiresOutputCleanup;
			DeleteNeuronReturnStruct(bool requiresPruning = false, bool requiresOutputCleanup = false) :
				m_requiresPruning(requiresPruning), m_requiresOutputCleanup(requiresOutputCleanup) {}
		};

		Genome::DeleteNeuronReturnStruct deleteNeuron(uint ID);					// Returns whether doing do has left any hanging neurons, requiring a potential pruning.
		void pruneTree();							// Cycles from back to front, pruning any neuron not in some way linked to the outputs.
		bool cleanupOutputs();						// Ensures that none of the outputs reference one another. Returns whether the tree needs pruning.
		uint addRandomNeuron(bool allowOutput = false);						// Adds a random neuron to the genome. Obviously. Returns its ID.
		void addRandomConnectionToNeuron(uint ID, bool allowReferencedOutputs = false);	// Adds a random connection to a previous ID to the specified neuron.
		void moveNeuron(uint sourceID, uint destID, bool warnInvalidMove = true);	// Moves a neuron. Does NOT rationalise outputs or prevent invalid moves.
	public:
		Genome(Utils::Forwarder * forwarder, uint inputCount, uint outputCount) :
			Utils::HasForwarder(forwarder),
			m_inputCount(inputCount),
			m_outputCount(outputCount)
		{
			if (outputCount > NEURON_COUNT_MIN) { WARN("Output count exceeds NEURON_COUNT_MIN in genome constructor."); }

			float interval = ((float)(NEURON_COUNT_MAX - NEURON_COUNT_MIN)) * 0.5f;
			uint desiredNeuronCount = std::clamp((uint)std::normal_distribution(interval + (float)NEURON_COUNT_MIN, 0.15f * interval)(*getRNG()), NEURON_COUNT_MIN, NEURON_COUNT_MAX);
			
			uint i = 0;
			for (; i < desiredNeuronCount / 2; i++) { addRandomNeuron(false); }
			for (; i < desiredNeuronCount; i++) { addRandomNeuron(true); }
		}
		// TODO: Add load-from-file constructor.
		~Genome() {}

		void mutate(bool supermutate = false);
		Genome operator+(Genome & s);
	};
}
