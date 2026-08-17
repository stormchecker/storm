#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/AutoParser.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"

TEST(StronglyConnectedComponentDecomposition, SmallSystemFromMatrix) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(6, 6);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 0, 0.3));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 5, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 2, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 1, 0.4));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 2, 0.3));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 3, 0.3));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 4, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 4, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(5, 1, 1.0));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::StronglyConnectedComponentDecomposition<double> sccDecomposition;
    storm::storage::StronglyConnectedComponentDecompositionOptions options;

    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(matrix, options));
    ASSERT_EQ(4ul, sccDecomposition.size());

    options.dropNaiveSccs();
    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(matrix, options));
    ASSERT_EQ(3ul, sccDecomposition.size());

    options.onlyBottomSccs();
    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(matrix, options));
    ASSERT_EQ(1ul, sccDecomposition.size());
}

TEST(StronglyConnectedComponentDecomposition, FullSystem1) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/tiny1.tra", STORM_TEST_RESOURCES_DIR "/lab/tiny1.lab", "", "");

    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> markovAutomaton = abstractModel->as<storm::models::sparse::MarkovAutomaton<double>>();

    storm::storage::StronglyConnectedComponentDecomposition<double> sccDecomposition;
    storm::storage::StronglyConnectedComponentDecompositionOptions options;

    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(markovAutomaton->getTransitionMatrix(), options));
    ASSERT_EQ(5ul, sccDecomposition.size());

    options.dropNaiveSccs();
    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(markovAutomaton->getTransitionMatrix(), options));
    ASSERT_EQ(2ul, sccDecomposition.size());

    options.onlyBottomSccs();
    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(markovAutomaton->getTransitionMatrix(), options));
    ASSERT_EQ(2ul, sccDecomposition.size());

    markovAutomaton = nullptr;
}

TEST(StronglyConnectedComponentDecomposition, FullSystem2) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/tiny2.tra", STORM_TEST_RESOURCES_DIR "/lab/tiny2.lab", "", "");

    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> markovAutomaton = abstractModel->as<storm::models::sparse::MarkovAutomaton<double>>();

    storm::storage::StronglyConnectedComponentDecomposition<double> sccDecomposition;
    storm::storage::StronglyConnectedComponentDecompositionOptions options;

    options.dropNaiveSccs();
    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(markovAutomaton->getTransitionMatrix(), options));
    ASSERT_EQ(2ul, sccDecomposition.size());

    // Now, because there is no ordering we have to check the contents of the MECs in a symmetrical way.
    storm::storage::StateBlock const& scc1 = sccDecomposition[0];
    storm::storage::StateBlock const& scc2 = sccDecomposition[1];

    storm::storage::StateBlock correctScc1 = {1, 3, 8, 9, 10};
    storm::storage::StateBlock correctScc2 = {4, 5, 6, 7};
    ASSERT_TRUE(scc1 == storm::storage::StateBlock(correctScc1.begin(), correctScc1.end()) ||
                scc1 == storm::storage::StateBlock(correctScc2.begin(), correctScc2.end()));
    ASSERT_TRUE(scc2 == storm::storage::StateBlock(correctScc1.begin(), correctScc1.end()) ||
                scc2 == storm::storage::StateBlock(correctScc2.begin(), correctScc2.end()));

    options.onlyBottomSccs();
    ASSERT_NO_THROW(sccDecomposition = storm::storage::StronglyConnectedComponentDecomposition<double>(markovAutomaton->getTransitionMatrix(), options));
    ASSERT_EQ(1ul, sccDecomposition.size());

    markovAutomaton = nullptr;
}
