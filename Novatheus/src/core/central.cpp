#include "pch.h"
#include "core\central.h"
#include "core/network.h"

namespace Core {
	void CentralController::generateRandomNetwork(bool detailedOutput)
	{
		if (mp_network != nullptr) {
			WARN("Network already exists!");
			return;
		}

		uint popID = std::uniform_int_distribution(1000u, 9999u)(*mp_rng); // Four-digit unique population id.

		mp_genome = new Genome(mp_forwarder, popID, 28u * 28u, OUTPUT_COUNT, detailedOutput);

		if (detailedOutput) { INFO("Generating network from genome..."); }
		mp_network = new Network(mp_genome, new FastSigmoid());
		if (detailedOutput) { INFO("Generated network from genome."); }

		return;
	}

	void CentralController::generateRandomPopulation()
	{
		if (!mvp_generation.empty()) {
			WARN("Generation already exists!");
			return;
		}

		uint popID = std::uniform_int_distribution(1000u, 9999u)(*mp_rng); // Four-digit unique population id.
		
		std::vector<std::future<Genome*>> generatorFutures;

		INFO("Starting generation of population (popID{0}) across {1} threads...", popID, GEN_WIDTH);

		for (uint i = 0; i < GEN_WIDTH; i++) {
			generatorFutures.emplace_back(std::async(
				std::launch::async,
				[](Utils::Forwarder* forwarder, uint popID) {
					return new Genome(forwarder, popID, 28u * 28u, OUTPUT_COUNT, true);
				},
				mp_forwarder,
				popID
				));
		}

		INFO("All threads launched...");

		for (uint i = 0; i < GEN_WIDTH; i++) {
			mvp_generation.push_back(generatorFutures[i].get());
		}

		INFO("Completed generation of population {0}.", popID);
	}

	void CentralController::savePopulation()
	{
		INFO("Saving genome (popID{0}, gen{1})...", mvp_generation[0]->getPopulationID(), mvp_generation[0]->getGeneration());

		std::string filepath = "/genomes/" + std::to_string(mvp_generation[0]->getPopulationID());
		std::string filename = std::to_string(mvp_generation[0]->getGeneration()) + ".population";
		std::string t;

		t = std::filesystem::current_path().string() + filepath;
		std::filesystem::path fs(t);
		if (!std::filesystem::exists(fs)) {
			INFO("Folder does not exist. Generating: '{0}'", t);
			std::filesystem::create_directories(fs);
		}

		uint bestRank = 100u;
		uint bestIndex = 0u;

		t = "." + filepath + "/" + filename;
		INFO("Opening file: '{0}'", t);
		std::ofstream outputFile(t, std::ios::out | std::ios::trunc | std::ios::binary);
		if (outputFile.is_open()) {
			INFO("File created/opened. Writing...");

			uint genomeCount = mvp_generation.size();
			outputFile.write(reinterpret_cast<const char*>(&genomeCount), sizeof(genomeCount));

			for (uint i = 0; i < mvp_generation.size(); i++) {
				INFO("Writing id{0}...", mvp_generation[i]->getID());
				mvp_generation[i]->writeToFile(outputFile);

				if (mvp_generation[i]->getRank() < bestRank) {
					bestRank = mvp_generation[i]->getRank();
					bestIndex = i;
				}
			}
			INFO("Writing to file complete. Successfully saved population (popid{0}).", mvp_generation[0]->getPopulationID());
		}
		else { WARN("Operation failed: Output file was not detected as open, suggesting error."); }

		INFO("Saving best of generation {0} (id{1}) in additional single-genome file...",
			mvp_generation[bestIndex]->getGeneration(),
			mvp_generation[bestIndex]->getID());

		filename = std::to_string(mvp_generation[bestIndex]->getGeneration()) + ".genome";
		t = "." + filepath + "/" + filename;
		INFO("Opening file: '{0}'", t);
		std::ofstream outputFile2(t, std::ios::out | std::ios::trunc | std::ios::binary);
		if (outputFile2.is_open()) {
			INFO("File created/opened. Writing...");
			mvp_generation[bestIndex]->writeToFile(outputFile2);
			INFO("Writing to file complete. Successfully saved genome (id{0}).", mvp_generation[bestIndex]->getID());
		}
		else { WARN("Operation failed: Output file was not detected as open, suggesting error."); }

		return;
	}

	void CentralController::loadPopulation(uint popID, uint generation)
	{
		std::string filepath;
		filepath = "./genomes/" + std::to_string(popID) + "/" + std::to_string(generation) + ".population";

		INFO("Loading population from file: '{0}'", filepath);
		std::ifstream source(filepath, std::ios::in | std::ios::binary);
		if (source.is_open()) {
			uint genomeCount;
			source.read(reinterpret_cast<char*>(&genomeCount), sizeof(genomeCount));
			INFO("File opened successfully. Contains {0} genomes.", genomeCount);
			for (uint i = 0; i < genomeCount; i++) {
				INFO("Reading genome...");
				mvp_generation.push_back(new Genome(mp_forwarder, source, true));
				INFO("Successfully read genome.");
			}
			INFO("Successfully read all {0} genomes.", genomeCount);
		}
		else { WARN("Operation failed: Output file was not detected as open, suggesting error."); }
		return;
	}

	void CentralController::stepPopulation()
	{
		INFO("Stepping population (popID{0}) from generation {1} to {2}...",
			mvp_generation[0]->getPopulationID(),
			mvp_generation[0]->getGeneration(),
			mvp_generation[0]->getGeneration() + 1
		);

		// ASSUME HAS ALREADY BEEN SORTED.

		// Sort the previous generation.
		//std::sort(mvp_generation.begin(), mvp_generation.end(), [](Genome* a, Genome* b) {
		//	return a->getAverage() < b->getRank();
		//});

		if (mvp_generation.size() != GEN_WIDTH) {
			WARN("Previous generation has width of {0}, rather than expected {1}! This will almost certainly cause major issues.", mvp_generation.size(), GEN_WIDTH);
		}

		uint popID = mvp_generation[0]->getPopulationID();

		// Deletion tracking setup
		std::vector<bool> keep;
		keep.reserve(GEN_WIDTH);
		for (uint i = 0; i < GEN_WIDTH; i++) { keep.push_back(false); }

		auto lastGen = mvp_generation;

		mvp_generation.clear();
		mvp_generation.reserve(GEN_WIDTH);

		// GEN_WIDTH had better be a multiple of 16.
		uint oneSixteenth = (uint)((float)GEN_WIDTH / 16.0f);
		uint threeSixteenths = 3u * oneSixteenth;
		uint fourSixteenths = 4u * oneSixteenth;

		uint index = 0;

		// Top three get copied straight into next gen.
		// Except, that's a waste of processing time, so they actually just get transerred over and their gen-counter incremented.
		for (uint i = 0; i < threeSixteenths; i++) {
			mvp_generation.push_back(lastGen[i]);
			keep[i] = true;
			index++;
		}

		// One totally new genome.
		for (uint i = 0; i < oneSixteenth; i++) {
			mvp_generation.push_back(new Genome(mp_forwarder, popID, 28u * 28u, OUTPUT_COUNT, true));
			index++;
		}

		if (m_rouletteWheel.empty()) {
			// Generate the roulette wheel.
			// Top of the rankings will have 16 tickets, the bottom will have 1, linear interp between.
			float divisor = (float)GEN_WIDTH / 16.0f;

			uint totalTickets = 0u;
			std::vector<uint> ticketCounts;
			ticketCounts.reserve(GEN_WIDTH);

			for (uint i = 0; i < GEN_WIDTH; i++) {
				uint tickets = (uint)((float)(GEN_WIDTH - i) / divisor);
				totalTickets += tickets;
				ticketCounts.push_back(tickets);
			}

			m_rouletteWheel.reserve(totalTickets);
			for (uint i = 0; i < GEN_WIDTH; i++) {
				for (uint j = 0; j < ticketCounts[i]; j++) {
					m_rouletteWheel.push_back(i);
				}
			}

			INFO("No roulette wheel detected. Constructed one containing a total of {0} tickets. Continuing...", totalTickets);
		}

		std::uniform_int_distribution rouletteDist(0u, (uint)m_rouletteWheel.size() - 1u);

		// Four roulette children, unmutated
		for (uint i = 0; i < fourSixteenths; i++) {
			uint parentB, parentA = m_rouletteWheel[rouletteDist(*mp_rng)];
			do { parentB = m_rouletteWheel[rouletteDist(*mp_rng)]; } while (parentA == parentB);

			Genome* child = *lastGen[parentA] + lastGen[parentB];

			mvp_generation.push_back(child);
			index++;
		}

		// Four roulette children, mutated
		for (uint i = 0; i < fourSixteenths; i++) {
			uint parentB, parentA = m_rouletteWheel[rouletteDist(*mp_rng)];
			do { parentB = m_rouletteWheel[rouletteDist(*mp_rng)]; } while (parentA == parentB);

			Genome* child = *lastGen[parentA] + lastGen[parentB];
			child->mutate();

			mvp_generation.push_back(child);
			index++;
		}

		// Three mid-tiers, mutated.
		for (uint i = 0; i < threeSixteenths; i++) {
			mvp_generation.push_back(lastGen[threeSixteenths + i]);
			keep[threeSixteenths + i] = true;
			mvp_generation[index]->mutate();
			index++;
		}

		// One mid-tier, super-mutated.
		for (uint i = 0; mvp_generation.size() < GEN_WIDTH; i++) {
			mvp_generation.push_back(lastGen[threeSixteenths + threeSixteenths + i]);
			keep[threeSixteenths + threeSixteenths + i] = true;
			mvp_generation[index]->mutate(true);
			index++;
		}

		// Delete everything we don't need to keep.
		for (uint i = 0; i < GEN_WIDTH; i++) {
			if (!keep[i]) { delete lastGen[i]; }
		}

		for (uint i = 0; i < GEN_WIDTH; i++) { mvp_generation[i]->incrementGeneration(); }

		INFO("Successfully stepped population (popID{0}) to generation {1}.",
			mvp_generation[0]->getPopulationID(),
			mvp_generation[0]->getGeneration()
		);
	}

	void CentralController::runPopulation(uint genLimit)
	{
		INFO("Training population...");
		bool indefinite = (genLimit == 0u);

		while (genLimit > 0 || indefinite) {
			if (genLimit > 0) { genLimit--; }

			for (uint i = 0; i < GEN_WIDTH; i++) { m_popRunStates[i] = RunState::Awaiting; }

			const uint simulTest = 2u; // How many pops to test simultaneously. Note that each pop will run 10 threads.
			std::vector<std::future<bool>> ongoingTests; // Returns true for success.
			for (uint i = 0; i < simulTest; i++) {
				ongoingTests.emplace_back(std::async(std::launch::async, [this]() {
					bool keepRunning = true;
					while (keepRunning) {
						uint candidate = 0u;
						keepRunning = false;
						// Work out which one to test.
						{
							std::lock_guard<std::mutex> lock(m_popRunStatesMutex);

							for (uint i = 0; i < GEN_WIDTH && !keepRunning; i++) {
								if (m_popRunStates[i] == CentralController::RunState::Awaiting) {
									if (mvp_generation[i]->isTested()) {
										m_popRunStates[i] = CentralController::RunState::Completed;
										INFO("Detected viable previous results for genome id{0} - accuracy {1}%. Skipping...", mvp_generation[i]->getID(), mvp_generation[i]->getAverageAccuracy());
									}
									else {
										m_popRunStates[i] = CentralController::RunState::Running;
										keepRunning = true;
										candidate = i;
									}
								}
							}
						}

						// Test the candidate.
						if (keepRunning) {
							INFO("Starting crossvalidated training and testing for genome id{0}...", mvp_generation[candidate]->getID());
							trainTestAndCrossval<FastSigmoid>(mvp_generation[candidate], STANDARD_TRAINING_BATCH_COUNT);
							INFO("Completed crossvalidated training and testing for genome id{0}.", mvp_generation[candidate]->getID());
							{
								std::lock_guard<std::mutex> lock(m_popRunStatesMutex);
								m_popRunStates[candidate] = CentralController::RunState::Completed;
							}
						}
					}
					return true;
				}));
			}

			for (auto& f : ongoingTests) { if (!f.get()) { ERRORM("Asynchronous testing lambda returned failure!"); }; }

			// Sort the generation by accuracy.
			std::sort(mvp_generation.begin(), mvp_generation.end(), [](Genome* a, Genome* b) {
				return a->getAverageAccuracy() > b->getAverageAccuracy();
			});
			for (uint i = 0; i < GEN_WIDTH; i++) { mvp_generation[i]->setRank(i); }

			// Output:
			uint gen = mvp_generation[0]->getGeneration(), popID = mvp_generation[0]->getPopulationID();
			{
				//	top is belonging to the genome that had the best accuracy, best is simply the best in category.
				float tr_ac_top,	tr_ac_mean,		tr_ac_best,		tr_ac_uq,	tr_ac_median,	tr_ac_lq,	tr_ac_worst;
				float tr_acac_top,	tr_acac_mean,	tr_acac_best,	tr_acac_uq,	tr_acac_median,	tr_acac_lq,	tr_acac_worst;
				float tr_aa_top,	tr_aa_mean,		tr_aa_best,		tr_aa_uq,	tr_aa_median,	tr_aa_lq,	tr_aa_worst;
				float te_ac_top,	te_ac_mean,		te_ac_best,		te_ac_uq,	te_ac_median,	te_ac_lq,	te_ac_worst;
				float te_acac_top,	te_acac_mean,	te_acac_best,	te_acac_uq,	te_acac_median,	te_acac_lq,	te_acac_worst;
				float te_aa_top,	te_aa_mean,		te_aa_best,		te_aa_uq,	te_aa_median,	te_aa_lq,	te_aa_worst;
				std::vector<float> tr_ac, tr_acac, tr_aa, te_ac, te_acac, te_aa;
				tr_ac.reserve(GEN_WIDTH);
				tr_acac.reserve(GEN_WIDTH);
				tr_aa.reserve(GEN_WIDTH);
				te_ac.reserve(GEN_WIDTH);
				te_acac.reserve(GEN_WIDTH);
				te_aa.reserve(GEN_WIDTH);

				// Top performer metrics.
				auto topMetrics = mvp_generation[0]->getMetrics();
				tr_ac_top	= topMetrics.m_trainingBufferAverageCost;
				tr_acac_top	= topMetrics.m_trainingBufferAverageCACost;
				tr_aa_top	= topMetrics.m_trainingBufferAccuracy;
				te_ac_top	= topMetrics.m_testingBufferAverageCost;
				te_acac_top	= topMetrics.m_testingBufferAverageCACost;
				te_aa_top	= topMetrics.m_testingBufferAccuracy;

				// Harvest all data.
				for (uint i = 0; i < GEN_WIDTH; i++) {
					auto metrics = mvp_generation[i]->getMetrics();
					tr_ac.push_back(metrics.m_trainingBufferAverageCost);
					tr_acac.push_back(metrics.m_trainingBufferAverageCACost);
					tr_aa.push_back(metrics.m_trainingBufferAccuracy);
					te_ac.push_back(metrics.m_testingBufferAverageCost);
					te_acac.push_back(metrics.m_testingBufferAverageCACost);
					te_aa.push_back(metrics.m_testingBufferAccuracy);
				}

				// Process means.
				tr_ac_mean = 0.0f;
				for (auto x : tr_ac) { tr_ac_mean += x; }
				tr_ac_mean /= tr_ac.size();
				tr_acac_mean = 0.0f;
				for (auto x : tr_acac) { tr_acac_mean += x; }
				tr_acac_mean /= tr_acac.size();
				tr_aa_mean = 0.0f;
				for (auto x : tr_aa) { tr_aa_mean += x; }
				tr_aa_mean /= tr_aa.size();
				te_ac_mean = 0.0f;
				for (auto x : te_ac) { te_ac_mean += x; }
				te_ac_mean /= te_ac.size();
				te_acac_mean = 0.0f;
				for (auto x : te_acac) { te_acac_mean += x; }
				te_acac_mean /= te_acac.size();
				te_aa_mean = 0.0f;
				for (auto x : te_aa) { te_aa_mean += x; }
				te_aa_mean /= te_aa.size();

				// Sorting for median-related stuff.
				std::sort(tr_ac.begin(), tr_ac.end());
				std::sort(tr_acac.begin(), tr_acac.end());
				std::sort(tr_aa.begin(), tr_aa.end(), std::greater<float>());
				std::sort(te_ac.begin(), te_ac.end());
				std::sort(te_acac.begin(), te_acac.end());
				std::sort(te_aa.begin(), te_aa.end(), std::greater<float>());

				// Bests and worsts.
				tr_ac_best = tr_ac.front();
				tr_ac_worst = tr_ac.back();
				tr_acac_best = tr_acac.front();
				tr_acac_worst = tr_acac.back();
				tr_aa_best = tr_aa.front();
				tr_aa_worst = tr_aa.back();
				te_ac_best = te_ac.front();
				te_ac_worst = te_ac.back();
				te_acac_best = te_acac.front();
				te_acac_worst = te_acac.back();
				te_aa_best = te_aa.front();
				te_aa_worst = te_aa.back();

				// Medians and quartiles.
				uint medIndex = (GEN_WIDTH / 2u) - 1u,
					uqIndex = (uint)((float)(1u + GEN_WIDTH) * 0.25f) - 1u,
					lqIndex = (uint)((float)(1u + GEN_WIDTH) * 0.75f) - 1u;

				tr_ac_median = (tr_ac[medIndex] + tr_ac[medIndex + 1]) * 0.5f;
				tr_ac_uq = tr_ac[uqIndex];
				tr_ac_lq = tr_ac[lqIndex];
				tr_acac_median = (tr_acac[medIndex] + tr_acac[medIndex + 1]) * 0.5f;
				tr_acac_uq = tr_acac[uqIndex];
				tr_acac_lq = tr_acac[lqIndex];
				tr_aa_median = (tr_aa[medIndex] + tr_aa[medIndex + 1]) * 0.5f;
				tr_aa_uq = tr_aa[uqIndex];
				tr_aa_lq = tr_aa[lqIndex];
				te_ac_median = (te_ac[medIndex] + te_ac[medIndex + 1]) * 0.5f;
				te_ac_uq = te_ac[uqIndex];
				te_ac_lq = te_ac[lqIndex];
				te_acac_median = (te_acac[medIndex] + te_acac[medIndex + 1]) * 0.5f;
				te_acac_uq = te_acac[uqIndex];
				te_acac_lq = te_acac[lqIndex];
				te_aa_median = (te_aa[medIndex] + te_aa[medIndex + 1]) * 0.5f;
				te_aa_uq = te_aa[uqIndex];
				te_aa_lq = te_aa[lqIndex];

				// Excellent! Now we need to write it to file.
				// Start by checking if the folder exists,
				// then check if file exists,
				// create and insert headers if not
				// then open and append the next row of data.

				std::string filepath = "/genomes/" + std::to_string(popID);
				std::string filename = "data.txt";
				std::string t;

				t = std::filesystem::current_path().string() + filepath;
				std::filesystem::path fs(t);
				if (!std::filesystem::exists(fs)) {
					INFO("Folder does not exist. Generating: '{0}'", t);
					std::filesystem::create_directories(fs);
				}

					t = "." + filepath + "/" + filename;
				{
					std::ifstream preexisting(t);
					if (!preexisting.is_open()) {
						INFO("File does not exist. Generating: '{0}'", t);
						preexisting.close();

						std::ofstream outputFile(t, std::ios::out | std::ios::trunc);
						if (outputFile.is_open()) {
							outputFile << "gen\t\t\t\t";
							outputFile << "tr_ac_top\t\ttr_ac_mean\t\ttr_ac_best\t\ttr_ac_uq\t\ttr_ac_median\ttr_ac_lq\t\ttr_ac_worst\t\t";
							outputFile << "tr_acac_top\t\ttr_acac_mean\ttr_acac_best\ttr_acac_uq\t\ttr_acac_median\ttr_acac_lq\t\ttr_acac_worst\t";
							outputFile << "tr_aa_top\t\ttr_aa_mean\t\ttr_aa_best\t\ttr_aa_uq\t\ttr_aa_median\ttr_aa_lq\t\ttr_aa_worst\t\t";
							outputFile << "te_ac_top\t\tte_ac_mean\t\tte_ac_best\t\tte_ac_uq\t\tte_ac_median\tte_ac_lq\t\tte_ac_worst\t\t";
							outputFile << "te_acac_top\t\tte_acac_mean\tte_acac_best\tte_acac_uq\t\tte_acac_median\tte_acac_lq\t\tte_acac_worst\t";
							outputFile << "te_aa_top\t\tte_aa_mean\t\tte_aa_best\t\tte_aa_uq\t\tte_aa_median\tte_aa_lq\t\tte_aa_worst\t\t";
							outputFile << "\r\n";
							outputFile.close();
						}
						else { ERRORM("Failed to generate file '{0}'", t); }
					}
				}

				std::ofstream outputFile(t, std::ios::out | std::ios::app);
				if (outputFile.is_open()) {
					outputFile << std::to_string(gen) << "\t\t\t\t" << std::fixed << std::setprecision(12);
					outputFile << std::to_string(tr_ac_top) << "\t\t" << std::to_string(tr_ac_mean) << "\t\t" << std::to_string(tr_ac_best) << "\t\t" << std::to_string(tr_ac_uq) << "\t\t" << std::to_string(tr_ac_median) << "\t\t" << std::to_string(tr_ac_lq) << "\t\t" << std::to_string(tr_ac_worst) << "\t\t";
					outputFile << std::to_string(tr_acac_top) << "\t\t" << std::to_string(tr_acac_mean) << "\t\t" << std::to_string(tr_acac_best) << "\t\t" << std::to_string(tr_acac_uq) << "\t\t" << std::to_string(tr_acac_median) << "\t\t" << std::to_string(tr_acac_lq) << "\t\t" << std::to_string(tr_acac_worst) << "\t\t";
					outputFile << std::to_string(tr_aa_top) << "\t\t" << std::to_string(tr_aa_mean) << "\t\t" << std::to_string(tr_aa_best) << "\t\t" << std::to_string(tr_aa_uq) << "\t\t" << std::to_string(tr_aa_median) << "\t\t" << std::to_string(tr_aa_lq) << "\t\t" << std::to_string(tr_aa_worst) << "\t\t";
					outputFile << std::to_string(te_ac_top) << "\t\t" << std::to_string(te_ac_mean) << "\t\t" << std::to_string(te_ac_best) << "\t\t" << std::to_string(te_ac_uq) << "\t\t" << std::to_string(te_ac_median) << "\t\t" << std::to_string(te_ac_lq) << "\t\t" << std::to_string(te_ac_worst) << "\t\t";
					outputFile << std::to_string(te_acac_top) << "\t\t" << std::to_string(te_acac_mean) << "\t\t" << std::to_string(te_acac_best) << "\t\t" << std::to_string(te_acac_uq) << "\t\t" << std::to_string(te_acac_median) << "\t\t" << std::to_string(te_acac_lq) << "\t\t" << std::to_string(te_acac_worst) << "\t\t";
					outputFile << std::to_string(te_aa_top) << "\t\t" << std::to_string(te_aa_mean) << "\t\t" << std::to_string(te_aa_best) << "\t\t" << std::to_string(te_aa_uq) << "\t\t" << std::to_string(te_aa_median) << "\t\t" << std::to_string(te_aa_lq) << "\t\t" << std::to_string(te_aa_worst) << "\t\t";
					outputFile << "\r\n";

					outputFile.close();
					INFO("Saved data to file '{0}'.", t);
				}
				else { ERRORM("Failed to open file '{0}'. Cannot output data!", t); }
			}

			savePopulation();
			stepPopulation();
			INFO("Generation complete. Continuing...");
		}

		INFO("Finished training population.");
	}

	void CentralController::executeCommand(std::pair<std::string, std::vector<std::string>> commandPair)
	{
		std::string& command = m_commandQueue.front().first;
		auto& params = m_commandQueue.front().second;
		
		if (command == "quit" ||
			command == "q" ||
			command == "end" ||
			command == "stop" ||
			command == "close"
			) {
			INFO("Are you sure you with to quit? (Y/N)");

			std::string answer;
			getline(std::cin, answer);

			if (answer == "y" || answer == "Y") { m_orderedToQuit = true; return; }
			return;
		}
		else if (command == "load_dataset" ||
			command == "ld") {

			std::string dataFileName, labelFileName;

			if (params.size() >= 4) {
				std::string dataFileName = params[0],
					labelFileName = params[2];
				int dmn = std::stoi(params[1]),
					lmn = std::stoi(params[3]);
				
				INFO("Loading data file '{0}' ({1}) with label file '{2}' ({3}).", dataFileName, dmn, labelFileName, lmn);
				mp_dataset->readIDXData(dataFileName, dmn, labelFileName, lmn);
			}
			else {
				INFO("Cannot execute 'load_dataset'. Parameters required: dataFileName, dataFileMagicNumber, labelFileName, labelFileMagicNumber. Alternatively, use 'load_default_dataset'.");
			}
			return;
		}
		else if (command == "load_default_dataset" ||
			command == "load_dataset_default" ||
			command == "ldd") {

			mp_dataset->readIDXData("MNIST/train-images.idx3-ubyte", 2051, "MNIST/train-labels.idx1-ubyte", 2049);

			return;
		}
		else if (command == "gen_random_network" ||
			command == "grn") {
			generateRandomNetwork(true);
			return;
		}
		else if (command == "gen_random_population" ||
			command == "gen_random_pop" ||
			command == "grp") {
			generateRandomPopulation();
			return;
		}
		else if (command == "train_network" ||
			command == "tn") {
			if (mp_network == nullptr) {
				WARN("No network available to train! Use 'gen_random_network' ('grn').");
				return;
			}
			if (!mp_dataset->getAlreadyInitialised()) {
				WARN("No dataset available for training purposes! Use 'load_dataset' ('ld') or 'load_default_dataset' ('ldd').");
				return;
			}

			uint batchCount = STANDARD_TRAINING_BATCH_COUNT;
			uint startingOffset = 0u;
			if (params.size() > 0) { batchCount = std::stoi(params[0]); }
			if (params.size() > 1) { startingOffset = std::stoi(params[1]); }

			auto results = mp_network->trainFromDataset(mp_dataset,
				std::array<bool, CROSSVAL_COUNT> { true, true, true },
				batchCount,
				startingOffset,
				true);

			return;
		}
		else if (command == "train_population" ||
			command == "tp") {
			if (mvp_generation.empty()) {
				WARN("No generation available to save! Use 'gen_random_population' ('grp'), followed by 'train_population' ('tp').");
				return;
			}
			if (!mp_dataset->getAlreadyInitialised()) {
				WARN("No dataset available for training purposes! Use 'load_dataset' ('ld') or 'load_default_dataset' ('ldd').");
				return;
			}

			uint maxGenerations = 0u;
			if (params.size() > 0) { maxGenerations = std::stoi(params[0]); }

			runPopulation(maxGenerations);

			return;
		}
		else if (command == "crossval_train_network" ||
			command == "ctn") {
			if (mp_genome == nullptr) {
				WARN("No genome available to train! Use 'gen_random_network' ('grn').");
				return;
			}
			if (!mp_dataset->getAlreadyInitialised()) {
				WARN("No dataset available for training purposes! Use 'load_dataset' ('ld') or 'load_default_dataset' ('ldd').");
				return;
			}

			uint batchCount = STANDARD_TRAINING_BATCH_COUNT;
			if (params.size() > 0) { batchCount = std::stoi(params[0]); }

			INFO("Starting cross-validated training of genome for {0} batches.", batchCount);
			trainTestAndCrossval<FastSigmoid>(mp_genome, batchCount);
			INFO("Cross-validated training complete.");

			return;
		}
		else if (command == "save_network" ||
			command == "sn" ||
			command == "save_genome" || 
			command  == "sg") {
			if (mp_genome == nullptr) {
				WARN("No genome available to save! Use 'gen_random_network' ('grn'), followed by 'train_network' ('tn').");
				return;
			}

			std::string filepath = "/genomes/" + std::to_string(mp_genome->getPopulationID());
			std::string filename = std::to_string(mp_genome->getGeneration()) + ".genome";
			std::string t;

			INFO("Saving genome (popID{0}, gen{1})...", mp_genome->getPopulationID(), mp_genome->getGeneration());

			t = std::filesystem::current_path().string() + filepath;
			std::filesystem::path fs(t);
			if (!std::filesystem::exists(fs)) {
				INFO("Folder does not exist. Generating: '{0}'", t);
				std::filesystem::create_directories(fs);
			}

			t = "." + filepath + "/" + filename;
			INFO("Opening file: '{0}'", t);
			std::ofstream outputFile(t, std::ios::out | std::ios::trunc | std::ios::binary);
			if (outputFile.is_open()) {
				INFO("File created/opened. Writing...");
				mp_genome->writeToFile(outputFile);
				INFO("Writing to file complete. Successfully saved genome (id{0}).", mp_genome->getID());
			}
			else { WARN("Operation failed: Output file was not detected as open, suggesting error."); }
			return;
		}
		else if (command == "load_network" ||
			command == "ln" ||
			command == "load_genome" ||
			command == "lg") {
			if (mp_genome != nullptr || mp_network != nullptr) {
				WARN("Solo genome slot already taken. Deletion functionality not yet implemented.");
				return;
			}

			if (params.size() < 1) {
				WARN("No population ID specified. Cannot load genome.");
			}

			std::string filepath;

			if (params.size() < 2) {
				INFO("No generation parameter specified. Generation 0 assumed...");
				filepath = "./genomes/" + params[0] + "/0.genome";
			}
			else { filepath = "./genomes/" + params[0] + "/" + params[1] + ".genome"; }

			INFO("Loading genome from file: '{0}'", filepath);
			std::ifstream source(filepath, std::ios::in | std::ios::binary);
			if (source.is_open()) {
				INFO("File opened successfully. Loading...");
				mp_genome = new Genome(mp_forwarder, source, true);
				INFO("File loaded. Generating network from genome...");
				mp_network = new Network(mp_genome, new FastSigmoid());
				INFO("Generated network from genome.");
			}
			else { WARN("Operation failed: Output file was not detected as open, suggesting error."); }
			return;
		}
		else if (command == "save_population" ||
			command == "sp" ||
			command == "save_pop") {
			if (mvp_generation.empty()) {
				WARN("No generation available to save! Use 'gen_random_population' ('grp'), followed by 'train_population' ('tp').");
			}
			else { savePopulation(); }
			return;
		}
		else if (command == "load_population" ||
			command == "lp" ||
			command == "load_pop") {
			if (!mvp_generation.empty()) {
				WARN("A population is already loaded. Deletion functionality not yet implemented.");
				return;
			}

			if (params.size() < 2) {
				WARN("Inadequate parameter count, cannot load population. Use should be in the form 'load_population popID generation', eg. 'lp 4649 3'.");
				return;
			}

			loadPopulation(std::stoul(params[0]), std::stoul(params[1]));
			return;
		}
		else if (command == "step_population" ||
			command == "step_p") {
			if (mvp_generation.empty()) {
				WARN("No generation available to save! Use 'gen_random_population' ('grp'), followed by 'train_population' ('tp').");
				return;
			}
			stepPopulation();
			return;
		}
		else if (command == "about") {
			INFO("Project Novatheus was built by Sniggyfigbat as part of a Master's-level coursework.");
			INFO("Github: https://github.com/sniggyfigbat/Novatheus");
			return;
		}
		else if (command == "help") {
			INFO("Command list:");
			INFO("");
			INFO("  - 'quit' ('q'):\t\t\t\tExit the application.");
			INFO("  - 'load_dataset' ('ld') :\t\t\tstring dataFilePath, uint dataFileMagicNumber, string labelFilePath, uint labelFileMagicNumber :\tLoads a dataset from a pair of idx files. Path relative to 'Novatheus/data/'.");
			INFO("  - 'load_default_dataset' ('ldd') :\t\tLoads the MNIST dataset.");
			INFO("  - 'gen_random_network' ('grn') :\t\tGenerates a single genome, creates a network from it, and stores both in their respective slots.");
			INFO("  - 'train_network' ('tn') :\t\t\tuint batches = 420u, uint batchStartingOffset = 0u :\tTrains the network stored in the single slot for the given number of batches, starting at the offset given.");
			INFO("  - 'crossval_train_network' ('ctn') :\tuint batches = 420u :\tGenerates 10 networks from the solo-slot genome, then trains each from a cross-validates selection of batches, using multiple cores.");
			INFO("  - 'save_network' ('sn') :\t\t\tSaves the network stored in the single slot to file, in the appropriate subfolder of 'Novatheus/genomes/'.");
			INFO("  - 'load_network' ('ln') :\t\t\tuint populationID, uint generation=0 :\tLoads to the single slot the network found in the corresponding file, 'Novatheus/genomes/$populationID$/$generation$.genome'.");
			INFO("  - 'gen_random_population' ('grp') :\tGenerates a population of genomes, and stores them in the population slot.");
			INFO("  - 'train_population' ('tp') :\t\tuint maxGenerations=infinite :\tTrains the population of genomes for the given number of generations, using over 20 threads. Takes many hours.");
			INFO("  - 'save_population' ('sp') :\t\tSaves the population to file, in the appropriate subfolder of 'Novatheus/genomes/'.");
			INFO("  - 'load_population' ('lp') :\t\tuint populationID, uint generation :\tLoads to the population slot the genomes found in the corresponding file, 'Novatheus/genomes/$populationID$/$generation$.population'.");
			INFO("  - 'step_population' ('step_p') :\t\tRuns the generation-incrementation code on the population slot.");
			INFO("");
			CRITICAL("IMPORTANT! When training, populations are saved AFTER testing but BEFORE the next generation is generated. As such, always run 'step_p' after loading a population, before further training.");
			INFO("");
			CRITICAL("IMPORTANT! seperate commands with ' -> ' to queue them, eg. 'ldd -> grp -> tp 5 -> q'.");
			INFO("");
			return;
		}

		INFO("'{0}' is not a recognised command.", command);
		return;
	}

	template <class SquishifierType>
	Core::Metrics CentralController::trainTestAndCrossval(Genome* genome, uint batches)
	{
		std::vector<Network *> networks;
		networks.reserve(CROSSVAL_COUNT);
		for (uint n = 0; n < CROSSVAL_COUNT; n++) {
			networks.push_back(new Network(genome, new SquishifierType()));
		}
		
		std::vector<uint> testSections;
		uint testSectionCount = (uint)(((float)CROSSVAL_COUNT) * 0.3f);
		testSections.reserve(testSectionCount);
		for (uint t = 0; t < testSectionCount; t++) { testSections.push_back(t); }

		std::vector<std::future<Metrics>> results;
		results.reserve(CROSSVAL_COUNT);

		uint offset = 0u;
		for (uint t = 0; t < CROSSVAL_COUNT; t++) {
			std::array<bool, CROSSVAL_COUNT> sections{};
			for (auto test : testSections) { sections[test] = true; }

			results.emplace_back(std::async(std::launch::async, &Network::trainFromDataset, networks[t], mp_dataset, sections, batches, offset, false));
			offset += (uint)mp_dataset->m_data[t].m_batches.size() * MINIBATCH_COUNT;

			for (auto& test : testSections) {
				test = (test + 1) % CROSSVAL_COUNT;
			}
		}

		Metrics total = results[0].get();
		for (uint r = 1; r < CROSSVAL_COUNT; r++) { total = total + results[r].get(); }
		total = total / CROSSVAL_COUNT;

		INFO("id{0}: Completed full training and crossvalidation, over {1} batches. Approximate average final training Cost/CACost/Accuracy: {2}/{3}/{4}%. Average final testing training Cost/CACost/Accuracy: {5}/{6}/{7}%.",
			genome->getID(),
			batches,
			total.m_trainingBufferAverageCost,
			total.m_trainingBufferAverageCACost,
			total.m_trainingBufferAccuracy,
			total.m_testingBufferAverageCost,
			total.m_testingBufferAverageCACost,
			total.m_testingBufferAccuracy);

		for (uint n = 0; n < CROSSVAL_COUNT; n++) {
			delete networks[n];
		}

		genome->setMetrics(total);

		return total;
	}

	CentralController::CentralController()
	{
		INFO("Central Controller initialising...");

		//mp_rng = new std::default_random_engine();
		mp_rng = new std::default_random_engine((uint)std::chrono::system_clock::now().time_since_epoch().count());
		//mp_rng = new std::default_random_engine(12345u);

		mp_assetManager = new Utils::AssetManager();
		mp_forwarder = new Utils::Forwarder(mp_rng, mp_assetManager);
		mp_dataset = new Dataset();

		INFO("Central Controller initialised.");
	}

	CentralController::~CentralController()
	{
		INFO("Central Controller terminating...");
		delete mp_network;
		delete mp_genome;

		for (auto pointer : mvp_generation) { delete pointer; }

		delete mp_dataset;
		delete mp_forwarder;
		delete mp_assetManager;
		delete mp_rng;

		INFO("Central Controller terminated.");
	}

	bool CentralController::runLoop()
	{
		INFO("Awaiting instruction:");
		std::string inputStr;
		std::getline(std::cin, inputStr);
		std::stringstream input(inputStr);

		bool commanded = false;
		while (!input.eof()) {
			std::string item;
			input >> item;
			for (auto c = item.begin(); c != item.end(); ++c) { (*c) = std::tolower(*c); }

			if (item == "->") { commanded = false; }
			else if (!commanded) {
				m_commandQueue.push(std::make_pair(item, std::vector<std::string>()));
				commanded = true;
			}
			else { m_commandQueue.back().second.push_back(item); }
		}

		while (!m_commandQueue.empty() && !m_orderedToQuit) {
			executeCommand(m_commandQueue.front());
			m_commandQueue.pop();
		}
		
		if (m_orderedToQuit) { return false; }
		return true;
	}
}