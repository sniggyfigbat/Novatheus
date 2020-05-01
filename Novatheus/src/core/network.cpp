#include "pch.h"
#include "core/network.h"

namespace Core {
	Network::Network(Genome * source, Squishifier* squishifier) :
		p_source(source),
		m_inputCount(source->m_inputCount),
		m_neuronCount(source->m_chromosomes.size()),
		m_outputCount(source->m_outputCount),
		m_valueBufferSize(source->m_inputCount + source->m_chromosomes.size())
	{
		mp_valueBuffer = new float[m_valueBufferSize];
		m_neurons.reserve(m_neuronCount);

		std::map<uint, uint> idsToIndices;
		uint i = 0;

		for (; i < m_inputCount; i++) { idsToIndices[i] = i; }

		for (auto iter = source->m_chromosomes.begin(); iter != source->m_chromosomes.end(); ++iter) {
			idsToIndices[iter->first] = i;
			
			m_neurons.push_back(Neuron(this, iter->second, idsToIndices));

			i++;
		}

		mp_squishifier = (squishifier != nullptr) ? squishifier : new FastSigmoid();
	}

	Network::~Network()
	{
		delete[] mp_valueBuffer;
		delete mp_squishifier;
	}

	std::vector<float> Network::runNetwork(std::vector<float>& inputs, bool prepForBackprop)
	{
		if (inputs.size() != m_inputCount) {
			WARN("Input vector of incorrect size {0} fed to network expecting size {1}", inputs.size(), m_inputCount);
			while (inputs.size() < m_inputCount) { inputs.push_back(0.0f); }
		}
		
		// Input values, suitably squished.
		/*for (uint i = 0; i < m_inputCount; i++) {
			mp_valueBuffer[i] = mp_squishifier->squish(inputs[i]);
		}*/

		for (uint i = 0; i < m_neuronCount; i++) {
			mp_valueBuffer[m_inputCount + i] = m_neurons[i].calculate(mp_squishifier, prepForBackprop);
		}

		// Build a return a vector from outputs.
		std::vector<float> returnVals;
		returnVals.reserve(m_outputCount);

		copy(&mp_valueBuffer[m_valueBufferSize - m_outputCount], &mp_valueBuffer[m_valueBufferSize], back_inserter(returnVals));		//returnVals.assign((mp_valueBuffer + (m_valueBufferSize - m_outputCount)), (mp_valueBuffer + (m_valueBufferSize - 1)));
		return returnVals;
	}

	void Network::trainFromMiniBatch(const std::vector<Sample*>& samples, float learningRate)
	{
		uint sampleCount = samples.size();
		float batchCostBeforeTraining = 0.0f;
		
		for (auto s : samples) {
			// Per sample

			// Run forwards;
			for (uint i = 0; i < m_neuronCount; i++) {
				mp_valueBuffer[m_inputCount + i] = m_neurons[i].calculate(mp_squishifier, true);
			}

			float cost = 0.0f;

			auto rIter = m_neurons.rbegin();
			auto rIterO = s->m_outputs.rbegin();
			for (uint i = 0; i < m_outputCount; i++) {
				float nc = mp_valueBuffer[m_valueBufferSize - (i + 1)] - (*rIterO);

				rIter->setDelCdelA(2.0f * nc);
				rIter->runBackprop(sampleCount);

				nc *= nc;
				cost += nc;

				++rIter;
				++rIterO;
			}

			m_batchAverageCost += cost / (float)sampleCount;

			for (; rIter != m_neurons.rend(); ++rIter) {
				rIter->runBackprop(sampleCount);
			}
		}

		for (auto iter = m_neurons.begin(); iter != m_neurons.end(); ++iter) {
			iter->endBatch(learningRate);
		}
		m_costBuffer.push(m_batchAverageCost);
		if (m_costBuffer.size() > 100) { m_costBuffer.pop(); }
		m_batchAverageCost = 0.0f;
	}
}