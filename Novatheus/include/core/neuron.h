#pragma once
#include "utils\utils.h"
#include "core\squishifier.h"

namespace Core {
	class Neuron {
	protected:
		float* p_valueBuffer;
		float* mp_weights;
		uint m_priorNeuronCount;
		float m_bias;
	public:
		Neuron(float * valueBuffer, uint weightCount);
		~Neuron();

		float calculate(Squishifier * squishifier);

		float getBias() const { return m_bias; }
		Neuron& setBias(float newBias) { m_bias = newBias; return *this; }
	};
}