#pragma once
#include "core\metrics.h"
#include "core\neuron.h"
#include "core\squishifier.h"
#include "core\dataset.h"
#include "core/genome.h"

namespace Core {
	class Genome;
	
	class Network : public Utils::HasForwarder {
		friend class Neuron;
	protected:
		Genome* p_source;

		uint m_inputCount;
		uint m_outputCount;

		uint m_valueBufferSize;

		uint m_neuronCount;
		std::vector<Neuron> m_neurons;

		Squishifier* mp_squishifier = nullptr;

		std::list<float> m_costBuffer; // Used for tracking a rolling buffer of costs over the last n minibatches.
		std::list<float> m_CACostBuffer; // Used for tracking a rolling buffer of correct-answer costs over the last n minibatches.
		std::list<float> m_accuracyBuffer; // Used for tracking a rolling buffer of accuracy over the last n minibatches, in the form of percentage of samples answered correctly.

		float m_startLRE, m_LRDelta, m_LRDeltaPerBatch;
		uint m_trainedBatches = 0;
	public:
		Network(Genome * source, Squishifier* squishifier = nullptr);
		~Network();

		float* mp_valueBuffer = nullptr; // C-array of values, used to store neuron outputs when feeding forward.

		uint getInputCount() const { return m_inputCount; }
		uint getOutputCount() const { return m_outputCount; }

		std::vector<float> runNetwork(std::vector<float>& inputs, bool prepForBackprop = false);

		std::tuple<float, float, float> trainFromBatch(Batch& batch); // Returns average cost and total correct answers.
		std::tuple<float, float, float> testFromBatch(Batch& batch); // Returns average cost and total correct answers.
		
		Metrics trainFromDataset(Dataset* dataset, std::array<bool, CROSSVAL_COUNT> crossvalidationSections, uint batches, uint batchOffset = 0u, bool detailedOutput = false);
		void setLearningRate(float startExponent, float deltaExponent) {
			m_startLRE = startExponent;
			m_LRDelta = deltaExponent;
			m_LRDeltaPerBatch = m_LRDelta / (float)STANDARD_TRAINING_BATCH_COUNT;
		}
	};
}