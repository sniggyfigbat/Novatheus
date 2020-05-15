#pragma once
#include "core\metrics.h"

namespace Core {
	class Chromosome {
	public:
		//uint m_id;
		std::map<uint, float> m_startingWeights;
		float m_startingBias;

		std::set<uint> m_references; // Where this gets referenced from.

		bool m_procBool1 = false; // Used for both pruning and genome combination. Basically a local variable, meaning dependant on method.
		bool m_isAnOutput = false; // Whether or not this is an output neuron.

		void rationaliseWeightings();	// Uses Xavier initialisation. Can only be applied to fresh neurons;

		Chromosome(float startingBias = 0.0f, bool isAnOutput = false) :
			m_startingBias(startingBias),
			m_isAnOutput(isAnOutput)
		{}
	};
	
	class Genome : public Utils::HasForwarder {
		friend class Network;
	private:
		uint m_inputCount;	// How many IDs are reserved at the front for input values.
		uint m_outputCount; // How many of the rearmost neurons are read as the output.

		uint m_populationID;
		uint m_generation = 0u;

		bool m_tested = false;
		Metrics m_metrics;
		uint m_rank = 100u;			// 0u is best of gen.

		std::map<uint, Chromosome> m_chromosomes;
		uint m_lowestOutputNeuronID = 0;

		float m_startLRExponent = -4.0f, m_LRExponentDelta = -6.0f; // Learning rate is 2^a, where a starts as m_startLRExponent, and is reduced by m_LRExponentDelta every STANDARD_TRAINING_BATCH_COUNT batches (1260 as of writing).

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
		uint addRandomNeuron(bool allowOutput = false, bool rationalize = true);	// Adds a random neuron to the genome. Obviously. Returns its ID.
		uint addRandomConnectionToNeuron(uint ID, bool allowReferencedOutputs = false);	// Adds a random connection to a previous ID to the specified neuron.
		void moveNeuron(uint sourceID, uint destID, bool warnInvalidMove = true);	// Moves a neuron. Does NOT rationalise outputs or prevent invalid moves.
		
		Genome(Utils::Forwarder* forwarder, uint populationID, uint inputCount, uint outputCount, uint generation); // Empty constructor, used for creating children.
	public:
		Genome(Utils::Forwarder* forwarder, uint populationID, uint inputCount, uint outputCount, bool detailedOutput = false);
		Genome(Utils::Forwarder* forwarder, std::ifstream& source, bool detailedOutput = false);
		~Genome() {}

		void mutate(bool supermutate = false);
		Genome * operator+(Genome * s);

		uint getGeneration() { return m_generation; }
		void setGeneration(uint gen) { m_generation = gen; }
		void incrementGeneration() { ++m_generation; }

		uint getPopulationID() { return m_populationID; }

		bool isTested() { return m_tested; }

		inline float getAverageAccuracy() { return m_metrics.m_testingBufferAccuracy; }
		void setMetrics(const Metrics & metrics) {
			m_tested = true;
			m_metrics = metrics;
		}
		Metrics getMetrics() { return m_metrics; }

		uint getRank() { return m_rank; }
		void setRank(uint rank) { m_rank = rank; }

		void writeToFile(std::ofstream & file);
	};
}
