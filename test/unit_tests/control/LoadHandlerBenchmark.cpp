/*
 * Xournal++
 *
 * Unit test for benchmarking LoadHandler
 * Run with `$ test/test-units --gtest_filter=ControlLoadHandler*benchmark* --gtest_also_run_disabled_tests`
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include <config-test.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"

#include "filesystem.h"

static void benchFile(unsigned int N, const fs::path& filepath) {
    auto start = std::chrono::high_resolution_clock::now();

    for (unsigned int i = 0; i < N; ++i) {
        LoadHandler handler;
        handler.loadDocument(filepath);
    }
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Loaded file " << filepath << " " << N << " times in " << duration.count() << " milliseconds\n";
}


TEST(ControlLoadHandler, DISABLED_benchmarkStrokes) { benchFile(1, GET_TESTFILE("performance/many-strokes.xopp")); }

TEST(ControlLoadHandler, DISABLED_benchmarkSmallFile) { benchFile(10000, GET_TESTFILE("performance/small.xopp")); }

TEST(ControlLoadHandler, DISABLED_benchmarkAnalysisNotes) {
    benchFile(10, GET_TESTFILE("performance/analysis-notes.xopp"));
}

TEST(ControlLoadHandler, DISABLED_benchmarkMechanicsNotes) {
    benchFile(10, GET_TESTFILE("performance/mechanics-notes.xopp"));
}
