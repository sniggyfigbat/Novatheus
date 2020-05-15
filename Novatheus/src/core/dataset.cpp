#include "pch.h"
#include "core/dataset.h"

namespace Core {
	bool Dataset::readIDXData(std::string dataFilePath, int dataMagicNumber, std::string labelFilePath, int labelMagicNumber)
	{
		if (m_alreadyInitialised) {
			WARN("Attempted to read IDX data ({0}) into already-initialised dataset. Dataset concatenation not yet supported.", dataFilePath);
			return false;
		}

		// Files:

		std::ifstream dataFile, labelFile;

		bool failure = false;
		try {
			dataFile.open("./data/" + dataFilePath, std::ios::binary);
		}
		catch (std::ios_base::failure& e) {
			WARN("Data file {0} threw exception: {1}", "./data/" + dataFilePath, e.code().message());
			failure = true;
		}

		try {
			labelFile.open("./data/" + labelFilePath, std::ios::binary);
		}
		catch (std::ios_base::failure& e) {
			WARN("Label file {0} threw exception: {1}", "./data/" + labelFilePath, e.code().message());
			failure = true;
		}

		if (!dataFile.is_open()) { WARN("Data file {0} failed to open", "Novatheus/data/" + dataFilePath); failure = true; }
		if (!labelFile.is_open()) { WARN("Label file {0} failed to open", "Novatheus/data/" + labelFilePath); failure = true; }
		if (failure) { return false; }
		INFO("Files opened...");

		// Magic numbers:

		int inpDataMN, inpLabelMN;
		readInt(dataFile, inpDataMN);
		readInt(labelFile, inpLabelMN);
		if (inpDataMN != dataMagicNumber) { WARN("Magic Number read from data file does not equal expected result. {0} != {1}", inpDataMN, dataMagicNumber); failure = true; }
		if (inpLabelMN != labelMagicNumber) { WARN("Magic Number read from label file does not equal expected result. {0} != {1}", inpLabelMN, labelMagicNumber); failure = true; }
		if (failure) { return false; }
		INFO("Magic numbers match...");

		// Metadata:

		int imageCount = 0, imageRows = 0, imageColumns = 0;
		readInt(dataFile, imageCount);
		readInt(dataFile, imageRows);
		readInt(dataFile, imageColumns);

		int labelCount = 0;
		readInt(labelFile, labelCount);

		INFO("Data file contains {0} images, each of which is {1}x{2}px. Label file contains {3} labels.", imageCount, imageRows, imageColumns, labelCount);

		if (imageCount != labelCount) {
			WARN("Image/Label count mismatch! Invalid data files!");
			return false;
		}

		uint imageContentsCount = imageRows * imageColumns; // How many floats in an image.
		uint minibatchCount = imageCount / MINIBATCH_COUNT;
		uint crossvalSectionContentsCount = minibatchCount / CROSSVAL_COUNT; // How many minibatches in a crossval section.
		uint leftovers = imageCount - (CROSSVAL_COUNT * crossvalSectionContentsCount * MINIBATCH_COUNT);

		m_data.reserve(CROSSVAL_COUNT);
		for (uint i = 0; i < CROSSVAL_COUNT; i++) { m_data.push_back(Section(crossvalSectionContentsCount)); }

		INFO("Data will be partitioned into {0} sections of {1} minibatches each. {2} samples will be left out and unused.", CROSSVAL_COUNT, crossvalSectionContentsCount, leftovers);
		INFO("Beginning data retooling...");

		bool filesFailed = false;
		uint successfulImages = 0;

		uint csi = 1;
		for (auto& cs : m_data) {
			//int bi = 0;
			for (uint i = 0; i < crossvalSectionContentsCount && !filesFailed; i++) {
				cs.m_batches.emplace_back();
				auto& b = cs.m_batches.back();

				for (auto& s : b.m_samples) {
					s.m_inputs.reserve(imageContentsCount);
					
					unsigned char label = 0;
					labelFile.read((char*)&label, 1);

					for (uint j = 0; j < OUTPUT_COUNT; j++) {
						s.m_outputs[j] = (label == j) ? 0.9f : 0.1f;
					}

					bool totallyEmpty = true;

					for (uint r = 0; r < imageRows && !filesFailed; r++) {
						for (uint c = 0; c < imageColumns; c++) {
							unsigned char pixel = 0;
							dataFile.read((char*)&pixel, sizeof(pixel));

							if (pixel == 0) { s.m_inputs.push_back(0.0f); }
							else {
								totallyEmpty = false;
								
								// Convert to float,
								// then divide by 255 to convert to 0 to 1,
								// then multiply by 0.8 and add 0.1 to convert to 0.1 to 0.9
								// so as to avoid stressing the system.

								float pixelData = (((float)pixel) * (0.8f / 255.0f)) + 0.1f; // And now it is 0.1 to 0.9.
								s.m_inputs.push_back(pixelData);
							}
						}
					}

					if (totallyEmpty) { WARN("Image detected to be entirely empty!"); filesFailed = true; }
					else { successfulImages++; }

					if (labelFile.eof()) { WARN("End of label file reached unexpectedly."); filesFailed = true; }
					if (labelFile.fail()) { WARN("Label file byte retrieval detected failure."); filesFailed = true; }
					if (dataFile.eof()) { WARN("End of data file reached unexpectedly."); filesFailed = true; }
					if (dataFile.fail()) { WARN("Data file byte retrieval detected failure."); filesFailed = true; }
				}

				//INFO("Completed batch {0}...", bi);
				//bi++;
			}

			INFO("Completed cross-validation section {0}...", csi);
			csi++;
		}

		m_alreadyInitialised = true;
		INFO("Data retooling complete. Dataset loaded.");

		return true;
	}
}