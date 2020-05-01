#pragma once
#include "core\neuron.h"
#include "core\squishifier.h"
#include "core\genome.h"
#include "core\sample.h"

namespace Core {
	class Network {
		friend class Neuron;
	protected:
		Genome* p_source;

		uint m_inputCount;
		uint m_outputCount;

		uint m_valueBufferSize;

		uint m_neuronCount;
		std::vector<Neuron> m_neurons;

		Squishifier* mp_squishifier = nullptr;

		float m_batchAverageCost = 0.0f;
		std::queue<float> m_costBuffer; // Used for tracking a rolling buffer of costs over the last n minibatches.
	public:
		Network(Genome * source, Squishifier* squishifier = nullptr);
		~Network();

		float* mp_valueBuffer = nullptr; // C-array of values, used to store neuron outputs when feeding forward.

		uint getInputCount() const { return m_inputCount; }
		uint getOutputCount() const { return m_outputCount; }

		std::vector<float> runNetwork(std::vector<float>& inputs, bool prepForBackprop = false);

		void trainFromMiniBatch(const std::vector<Sample*>& samples, float learningRate);
	};
}