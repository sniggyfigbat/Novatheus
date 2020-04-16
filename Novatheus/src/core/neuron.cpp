#include "pch.h"
#include "core/neuron.h"
#include "core/network.h"

namespace Core {
	Neuron::Neuron(float * valueBuffer, uint priorNeuronCount) :
		p_valueBuffer(valueBuffer),
		m_priorNeuronCount(priorNeuronCount)
	{
		mp_weights = new float[priorNeuronCount];
	}

	Neuron::~Neuron()
	{
		delete[] mp_weights;
	}

	float Neuron::calculate(Squishifier * squishifier)
	{
		float output = m_bias;
		for (uint i = 0; i < m_priorNeuronCount; i++) {
			if (mp_weights[i] != 0.0f) {
				output += mp_weights[i] * p_valueBuffer[i];
			}
		}

		output = squishifier->squish(output);

		return output;
	}
}