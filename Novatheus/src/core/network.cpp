#include "pch.h"
#include "core/network.h"

namespace Core {
	Network::Network(Genome * source, Squishifier* squishifier) :
		HasForwarder(source->getForwarder()),
		p_source(source),
		m_inputCount(source->m_inputCount),
		m_neuronCount(source->m_chromosomes.size()),
		m_outputCount(source->m_outputCount),
		m_valueBufferSize(source->m_inputCount + source->m_chromosomes.size()),
		m_startLRE(source->m_startLRExponent),
		m_LRDelta(source->m_LRExponentDelta)
	{
		INFO("id{0}: Network generating from genome id{1}...", getID(), source->getID());
		
		mp_valueBuffer = new float[m_valueBufferSize];
		m_neurons.reserve(m_neuronCount);

		std::map<uint, uint> idsToIndices;
		uint i = 0;

		for (; i < m_inputCount; i++) { idsToIndices[i] = i; }

		for (auto iter = source->m_chromosomes.begin(); iter != source->m_chromosomes.end(); ++iter) {
			idsToIndices[iter->first] = i;
			
			m_neurons.emplace_back(this, iter->second, idsToIndices);

			i++;
		}

		mp_squishifier = (squishifier != nullptr) ? squishifier : new FastSigmoid();

		m_LRDeltaPerBatch = m_LRDelta / (float)STANDARD_TRAINING_BATCH_COUNT;

		INFO("id{0}: Network generatiion complete.", getID());
	}

	Network::~Network()
	{
		delete[] mp_valueBuffer;
		delete mp_squishifier;
	}

	std::vector<float> Network::runNetwork(std::vector<float>& inputs, bool prepForBackprop)
	{
		if (inputs.size() != m_inputCount) {
			WARN("id{0}: Input vector of incorrect size {1} fed to network expecting size {2}", getID(), inputs.size(), m_inputCount);
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

		copy(&mp_valueBuffer[m_valueBufferSize - m_outputCount], &mp_valueBuffer[m_valueBufferSize], back_inserter(returnVals));
		//returnVals.assign((mp_valueBuffer + (m_valueBufferSize - m_outputCount)), (mp_valueBuffer + (m_valueBufferSize - 1)));
		return returnVals;
	}

	std::tuple<float, float, float>  Network::trainFromBatch(Batch& batch)
	{
		std::lock_guard<std::mutex> lock(*(batch.mp_inUse));

		// CA == 'Correct Answer'
		float batchAverageCost = 0.0f;
		float batchCAAverageCost = 0.0f;
		uint CASamples = 0u;

		float learningRate = m_startLRE + (m_trainedBatches * m_LRDeltaPerBatch);
		learningRate = std::pow(2.0f, learningRate);

		for (auto& s : batch.m_samples) {
			// Per sample
			for (uint i = 0; i < s.m_inputs.size(); i++) { mp_valueBuffer[i] = s.m_inputs[i]; }

			// Run forwards;
			for (uint i = 0; i < m_neuronCount; i++) {
				mp_valueBuffer[m_inputCount + i] = m_neurons[i].calculate(mp_squishifier, true);
			}

			float cost = 0.0f;

			float highestOutputVal = 0.0f;
			uint highestOutputIndex = 0u;
			uint correctOutputIndex = 0u;

			std::vector<float> outputs;
			outputs.reserve(m_outputCount);

			auto rIter = m_neurons.rbegin();
			auto rIterO = s.m_outputs.rbegin();
			for (uint i = 0; i < m_outputCount; i++) {
				outputs.insert(outputs.begin(), mp_valueBuffer[m_valueBufferSize - (i + 1)]);
				bool isCorrectOutput = (*rIterO > 0.5f);
				
				float diff = mp_valueBuffer[m_valueBufferSize - (i + 1)] - (*rIterO);
				float partialCost = diff * diff;

				// Cost of the true output is multiplied by 5.
				if (isCorrectOutput) {
					rIter->setDelCdelA(10.0f * diff);
					partialCost *= 5.0f;
				}
				else { rIter->setDelCdelA(2.0f * diff); }
				
				rIter->runBackprop();

				cost += partialCost;

				if (mp_valueBuffer[m_valueBufferSize - (i + 1)] > highestOutputVal) {
					highestOutputVal = mp_valueBuffer[m_valueBufferSize - (i + 1)];
					highestOutputIndex = m_outputCount - (i + 1);
				}

				if (isCorrectOutput) {
					correctOutputIndex = m_outputCount - (i + 1);
					batchCAAverageCost += partialCost;
				}

				++rIter;
				++rIterO;
			}

			batchAverageCost += cost;

			if (highestOutputIndex == correctOutputIndex) { CASamples++; }

			for (; rIter != m_neurons.rend(); ++rIter) {
				rIter->runBackprop();
			}
		}

		for (auto iter = m_neurons.begin(); iter != m_neurons.end(); ++iter) {
			iter->endBatch(learningRate, MINIBATCH_COUNT);
		}

		batchAverageCost /= (float)MINIBATCH_COUNT;

		batchCAAverageCost /= (float)MINIBATCH_COUNT;
		float caPercentage = (100.0f * (float)CASamples) / (float)MINIBATCH_COUNT;

		m_costBuffer.push_back(batchAverageCost);
		if (m_costBuffer.size() > 100) { m_costBuffer.pop_front(); }

		m_CACostBuffer.push_back(batchCAAverageCost);
		if (m_CACostBuffer.size() > 100) { m_CACostBuffer.pop_front(); }

		m_accuracyBuffer.push_back(caPercentage);
		if (m_accuracyBuffer.size() > 100) { m_accuracyBuffer.pop_front(); }

		m_trainedBatches++;

		return std::make_tuple(batchAverageCost, batchCAAverageCost, caPercentage);
	}

	std::tuple<float, float, float> Network::testFromBatch(Batch& batch)
	{
		std::lock_guard<std::mutex> lock(*(batch.mp_inUse));

		// CA == 'Correct Answer'
		float batchAverageCost = 0.0f;
		float batchCAAverageCost = 0.0f;
		uint CASamples = 0u;

		for (auto& s : batch.m_samples) {
			// Per sample
			for (uint i = 0; i < s.m_inputs.size(); i++) { mp_valueBuffer[i] = s.m_inputs[i]; }

			// Run forwards;
			for (uint i = 0; i < m_neuronCount; i++) {
				mp_valueBuffer[m_inputCount + i] = m_neurons[i].calculate(mp_squishifier, false);
			}

			float cost = 0.0f;

			float highestOutputVal = 0.0f;
			uint highestOutputIndex = 0u;
			uint correctOutputIndex = 0u;

			auto rIter = m_neurons.rbegin();
			auto rIterO = s.m_outputs.rbegin();
			for (uint i = 0; i < m_outputCount; i++) {
				bool isCorrectOutput = (*rIterO > 0.5f);

				float diff = mp_valueBuffer[m_valueBufferSize - (i + 1)] - (*rIterO);
				float partialCost = diff * diff;

				// Cost of the true output is multiplied by 5.
				if (isCorrectOutput) { partialCost *= 5.0f; }

				cost += partialCost;

				if (mp_valueBuffer[m_valueBufferSize - (i + 1)] > highestOutputVal) {
					highestOutputVal = mp_valueBuffer[m_valueBufferSize - (i + 1)];
					highestOutputIndex = m_outputCount - (i + 1);
				}

				if (isCorrectOutput) {
					correctOutputIndex = m_outputCount - (i + 1);
					batchCAAverageCost += partialCost;
				}

				++rIter;
				++rIterO;
			}

			batchAverageCost += cost;

			if (highestOutputIndex == correctOutputIndex) { CASamples++; }
		}

		batchAverageCost /= (float)MINIBATCH_COUNT;
		batchCAAverageCost /= (float)MINIBATCH_COUNT;
		float caPercentage = (100.0f * (float)CASamples) / (float)MINIBATCH_COUNT;
		
		return std::make_tuple(batchAverageCost, batchCAAverageCost, caPercentage);
	}

	Metrics Network::trainFromDataset(Dataset* dataset, std::array<bool, CROSSVAL_COUNT> crossvalidationSections, uint batches, uint batchOffset, bool detailedOutput)
	{
		uint batchIndex = 0, section = 0, batch = 0;

		// Offset management. Not sure it'll ever be used, but whatevs.
		uint cvsSize = dataset->m_data[0].m_batches.size();

		uint cvsOffset = batchOffset / cvsSize;
		uint trCvsCount = 0;
		for (auto c : crossvalidationSections) { if (!c) { ++trCvsCount; } }
		cvsOffset = cvsOffset % trCvsCount;
		for (; section < CROSSVAL_COUNT && cvsOffset > 0; ++section) { if (!crossvalidationSections[section]) { cvsOffset--; } }
		batch = batchOffset % cvsSize;

		// Section and Batch are now at the right offsets.
		if (detailedOutput) {
			float slr = m_startLRE + (m_trainedBatches * m_LRDeltaPerBatch);
			slr = std::pow(2.0f, slr);
			INFO("id{0}: Training network for {1} batches (starting offset Section {2}, Batch {3}), with learning rate {4}...", getID(), batches, section, batch, slr);
		}

		// Do actual training.
		while (batchIndex < batches) {
			for (; section < CROSSVAL_COUNT && batchIndex < batches; section++) {
				if (!crossvalidationSections[section]) {
					// Is a training section.
					for (; batch < cvsSize && batchIndex < batches; batch++) {
						auto output = trainFromBatch(dataset->m_data[section].m_batches[batch]);
						batchIndex++;

						if (detailedOutput) {
							INFO("id{0}: Finished training on Section {1}, Batch {2} (count {3}). Cost/CACost/Accuracy: {4}/{5}/{6}%.", getID(), section, batch, batchIndex, std::get<0>(output), std::get<1>(output), std::get<2>(output));
						}
					}
					batch = 0;

#ifdef DEBUG
					{
						float slr = m_startLRE + (m_trainedBatches * m_LRDeltaPerBatch);
						slr = std::pow(2.0f, slr);

						float trainingBufferAverageCost = 0.0f;
						float trainingBufferAverageCACost = 0.0f;
						float trainingBufferAccuracy = 0.0f;

						for (auto& c : m_costBuffer) { trainingBufferAverageCost += c; }
						for (auto& c : m_CACostBuffer) { trainingBufferAverageCACost += c; }
						for (auto& a : m_accuracyBuffer) { trainingBufferAccuracy += a; }

						trainingBufferAverageCost /= m_costBuffer.size();
						trainingBufferAverageCACost /= m_CACostBuffer.size();
						trainingBufferAccuracy /= m_accuracyBuffer.size();

						INFO("id{0}: Completed Section {1}. Approximate training Cost/CACost/Accuracy: {2}/{3}/{4}%. Learning rate is now {5}. Continuing...",
							getID(),
							section,
							trainingBufferAverageCost,
							trainingBufferAverageCACost,
							trainingBufferAccuracy,
							slr);
					}
#endif // DEBUG
				}
			}
			section = 0;
		}
		
		// Testing and metrics.
		if (detailedOutput) { INFO("id{0}: Running testing...", getID()); }
		float trainingBufferAverageCost = 0.0f;
		float trainingBufferAverageCACost = 0.0f;
		float trainingBufferAccuracy = 0.0f;
		float testingBufferAverageCost = 0.0f;
		float testingBufferAverageCACost = 0.0f;
		float testingBufferAccuracy = 0.0f;

		for (auto& c : m_costBuffer) { trainingBufferAverageCost += c; }
		for (auto& c : m_CACostBuffer) { trainingBufferAverageCACost += c; }
		for (auto& a : m_accuracyBuffer) { trainingBufferAccuracy += a; }

		trainingBufferAverageCost /= m_costBuffer.size();
		trainingBufferAverageCACost /= m_CACostBuffer.size();
		trainingBufferAccuracy /= m_accuracyBuffer.size();

		uint testedBatches = 0;
		for (uint s = 0; s < CROSSVAL_COUNT; ++s ) {
			if (crossvalidationSections[s]) {
				// Is a testing section.
				for (auto& b : dataset->m_data[s].m_batches) {
					auto output = testFromBatch(b);

					testingBufferAverageCost	+= std::get<0>(output);
					testingBufferAverageCACost	+= std::get<1>(output);
					testingBufferAccuracy		+= std::get<2>(output);
					testedBatches++;
				}
			}
		}

		testingBufferAverageCost	/= (float)testedBatches;
		testingBufferAverageCACost	/= (float)testedBatches;
		testingBufferAccuracy		/= (float)testedBatches;

		if (detailedOutput) {
			INFO("id{0}: Completed training for {1} batches. Approximate final training Cost/CACost/Accuracy: {2}/{3}/{4}%. Final testing Cost/CACost/Accuracy: {5}/{6}/{7}% (tested over {8} samples).",
				getID(),
				batchIndex,
				trainingBufferAverageCost,
				trainingBufferAverageCACost,
				trainingBufferAccuracy,
				testingBufferAverageCost,
				testingBufferAverageCACost,
				testingBufferAccuracy,
				testedBatches * MINIBATCH_COUNT);
		}

		return Metrics(trainingBufferAverageCost, trainingBufferAverageCACost, trainingBufferAccuracy, testingBufferAverageCost, testingBufferAverageCACost, testingBufferAccuracy);
	}
}