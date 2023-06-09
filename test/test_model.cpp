#include "cross_entropy_loss.hpp"
#include "exceptions/model.hpp"
#include "fixtures.hpp"
#include "image_loader.hpp"
#include "linear.hpp"
#include "model.hpp"
#include <Eigen/Dense>
#include <algorithm>
#include <bits/std_abs.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <initializer_list>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

using namespace model;
using json = nlohmann::json;

namespace test_model {
#pragma region Fixtures
std::vector<linear::Linear> getLayers() {
  std::vector<linear::Linear> layers = {linear::Linear(4, 3),
                                        linear::Linear(3, 2, "ReLU")};
  for (linear::Linear &layer : layers) {
    layer.setWeight(Eigen::MatrixXd::Ones(layer.outChannels, layer.inChannels));
    layer.setBias(Eigen::VectorXd::Ones(layer.outChannels));
  }
  return layers;
}

loss::CrossEntropyLoss getLoss() { return loss::CrossEntropyLoss("sum"); }

Model::KeywordArgs getKwargs() {
  Model::KeywordArgs kwargs;
  kwargs.setTrainMetricsFromMetricTypes({"loss"});
  kwargs.setValidationMetricsFromMetricTypes({"loss"});
  kwargs.classes = {"0", "1"};
  return kwargs;
}

Model getModel() { return Model(getLayers(), getLoss(), getKwargs()); }

std::pair<Eigen::MatrixXd, std::vector<int>> getData() {
  return std::make_pair(Eigen::MatrixXd{{4, -3, 2, 4},
                                        {6, -3, 6, 1},
                                        {5, 9, 8, 3},
                                        {8, -10, 8, -7},
                                        {0, 3, 7, 5},
                                        {7, -6, 8, 8},
                                        {1, -10, -7, 5},
                                        {-6, 5, 4, -9},
                                        {4, -3, 8, 6},
                                        {5, -9, 2, 1}},
                        std::vector<int>{0, 1, 1, 1, 1, 0, 1, 1, 1, 0});
}

struct MockDatasetBatcher : public loader::DatasetBatcher {
  Eigen::MatrixXd X;
  std::vector<int> y;
  int batchSize;

  MockDatasetBatcher(const Eigen::MatrixXd &X, const std::vector<int> &y,
                     int batchSize)
      : X(X), y(y), batchSize(batchSize), loader::DatasetBatcher("", {}, {}, {},
                                                                 batchSize){};

  int size() const override {
    return (this->X.rows() + this->batchSize - 1) / this->batchSize;
  }

  loader::minibatch operator[](int i) const override {
    int start = i * this->batchSize,
        end =
            std::min(this->y.size(), (unsigned long)(i + 1) * this->batchSize);
    Eigen::MatrixXd X(end - start, this->X.cols());
    for (int j = start; j < end; ++j) {
      X.row(j - start) = this->X.row(j);
    }
    std::vector<int> y(this->y.begin() + start, this->y.begin() + end);
    return std::make_pair(X, y);
  };
};

struct MockLoader : public loader::ImageLoader {
  Eigen::MatrixXd trainX, valX;
  std::vector<int> trainY, valY, labels;

  MockLoader(float split) {
    auto [X, y] = getData();
    int trainSize = X.rows() * split;
    this->trainX = Eigen::MatrixXd(trainSize, X.cols());
    for (int i = 0; i < trainSize; ++i) {
      this->trainX.row(i) = X.row(i);
    }
    this->valX = Eigen::MatrixXd(X.rows() - trainSize, X.cols());
    for (int i = trainSize; i < X.rows(); ++i) {
      this->valX.row(i - trainSize) = X.row(i);
    }

    this->trainY = std::vector<int>(y.begin(), y.begin() + trainSize);
    this->valY = std::vector<int>(y.begin() + trainSize, y.end());

    std::unordered_set<int> classes(y.begin(), y.end());
    this->labels = std::vector<int>(classes.begin(), classes.end());
    this->classes = std::vector<std::string>(this->labels.size());
    for (int i = 0; i < this->labels.size(); ++i) {
      this->classes[i] = std::to_string(this->labels[i]);
    }
  };

  std::shared_ptr<loader::DatasetBatcher>
  getBatcher(std::string type, int batchSize,
             const loader::DatasetBatcher::KeywordArgs &kwargs =
                 loader::DatasetBatcher::KeywordArgs()) const override {
    if (type != "train" && type != "test") {
      throw "Invalid type";
    }
    return std::make_shared<MockDatasetBatcher>(MockDatasetBatcher(
        type == "train" ? this->trainX : this->valX,
        type == "train" ? this->trainY : this->valY, batchSize));
  }
};

class ModelJsonFile : public test_filesystem::BaseFileSystemFixture {
protected:
  std::filesystem::path expected;

  void SetUp() override {
    test_filesystem::BaseFileSystemFixture::SetUp();
    this->expected = this->root / "true_model.json";
    std::ofstream file(this->expected);
    file << "{\"class\": \"Model\", \"layers\": [{\"class\": \"Linear\", "
            "\"weight\": [[1.0, 1.0, 1.0, 1.0], [1.0, 1.0, 1.0, 1.0], [1.0, "
            "1.0, 1.0, 1.0]], \"bias\": [1.0, 1.0, 1.0], "
            "\"activation_function\": \"NoActivation\"}, {\"class\": "
            "\"Linear\", \"weight\": [[1.0, 1.0, 1.0], [1.0, 1.0, 1.0]], "
            "\"bias\": [1.0, 1.0], \"activation_function\": \"ReLU\"}], "
            "\"loss\": {\"class\": \"CrossEntropyLoss\", \"reduction\": "
            "\"sum\"}, \"total_epochs\": 0, \"train_metrics\": {\"loss\": []}, "
            "\"validation_metrics\": {\"loss\": []}, \"classes\": [\"0\", "
            "\"1\"]}";
    file.close();
  }
};
#pragma endregion Fixtures

#pragma region Tests
#pragma region Init
TEST(Model, TestInitWithInvalidTotalEpochs) {
  std::vector<linear::Linear> layers = getLayers();
  loss::CrossEntropyLoss loss = getLoss();
  Model::KeywordArgs kwargs = getKwargs();
  kwargs.totalEpochs = -1;

  EXPECT_THROW(Model(layers, loss, kwargs),
               exceptions::model::InvalidTotalEpochException);
}
#pragma endregion Init

#pragma region Properties
#pragma region Classes
TEST(Model, TestClasses) {
  Model model = getModel();
  std::vector<std::string> classes = getKwargs().classes;
  EXPECT_EQ(classes, model.getClasses())
      << "Classes were not set during construction.";

  classes = {"a", "b", "c"};
  model.setClasses(classes);
  EXPECT_EQ(classes, model.getClasses()) << "Classes were not updated.";
}
#pragma endregion Classes

#pragma region Evaluation mode
TEST(Model, TestEvaluationMode) {
  Model model = getModel();
  auto checkAll = [&](bool mode) {
    ASSERT_EQ(model.getEval(), mode)
        << "Expected model's evaluation mode to be set to "
        << (mode ? "true" : "false");
    for (const auto &layer : model.getLayers()) {
      ASSERT_EQ(layer.getEval(), mode)
          << "Expected all layers' evaluation mode to be set to "
          << (mode ? "true" : "false");
    }
  };

  checkAll(false);
  model.setEval(false);
  checkAll(false);
  model.setEval(true);
  checkAll(true);
}
#pragma endregion Evaluation mode

#pragma region Layers
TEST(Model, TestLayers) {
  std::vector<std::vector<linear::Linear>> layers{
      {linear::Linear(1, 2), linear::Linear(2, 3), linear::Linear(3, 5)},
      {linear::Linear(1, 2)}};
  Model model = getModel();
  for (int i = 0; i < layers.size(); ++i) {
    model.setLayers(layers[i]);
    for (int j = 0; j < layers[i].size(); ++j) {
      ASSERT_EQ(layers[i][j], model.getLayers()[j])
          << "Layers did not match on test " << i << " layer " << j;
    }
  }
}

TEST(Model, TestLayersWithEmptyVector) {
  std::vector<linear::Linear> layers;
  Model model = getModel();
  EXPECT_THROW(model.setLayers(layers),
               exceptions::model::EmptyLayersVectorException);
}
#pragma endregion Layers

#pragma region Total epochs
TEST(Model, TestTotalEpochs) {
  Model model = getModel();
  ASSERT_EQ(0, model.getTotalEpochs()) << "Total epochs should default to 0.";

  model.setTotalEpochs(10);
  ASSERT_EQ(10, model.getTotalEpochs())
      << "Total epochs should have been changed to 10.";
}

TEST(Model, TestTotalEpochsWithInvalidValue) {
  Model model = getModel();
  EXPECT_THROW(model.setTotalEpochs(-1),
               exceptions::model::InvalidTotalEpochException)
      << "A negative value should throw the exception.";
}
#pragma endregion Total epochs

#pragma region Loss
TEST(Model, TestLoss) {
  Model model = getModel();
  loss::CrossEntropyLoss loss("mean");

  ASSERT_NE(model.getLoss(), loss)
      << "Loss should have different reduction methods.";
  model.setLoss(loss);
  ASSERT_EQ(model.getLoss(), loss)
      << "Loss should have the same reduction methods.";
}
#pragma endregion Loss
#pragma endregion Properties

#pragma region Load
TEST(Model, TestLoad) {
  std::vector<linear::Linear> layers = getLayers();
  loss::CrossEntropyLoss loss = getLoss();
  std::vector<std::string> classes = {"0", "1"};
  int totalEpochs = 10;
  std::unordered_map<std::string, metricHistoryValue> trainMetrics{
      {"loss", {(float)1, (float)2, (float)3}},
      {"recall",
       {(std::vector<float>){1, 2, 3}, (std::vector<float>){4, 5, 6},
        (std::vector<float>){7, 8, 9}}}},
      validationMetrics{
          {"loss", {(float)3, (float)2, (float)1}},
          {"recall",
           {(std::vector<float>){9, 8, 7}, (std::vector<float>){6, 5, 4},
            (std::vector<float>){3, 2, 1}}}};

  json attributes;
  attributes["class"] = "Model";
  attributes["layers"] = json::array();
  for (const linear::Linear &layer : layers) {
    attributes["layers"].push_back(layer.toJson());
  }
  attributes["loss"] = loss.toJson();
  attributes["total_epochs"] = totalEpochs;
  attributes["classes"] = classes;
  attributes["train_metrics"] = json::object();
  attributes["train_metrics"]["loss"] = json::array({1.0, 2.0, 3.0});
  attributes["train_metrics"]["recall"] =
      json::array({{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}});
  attributes["validation_metrics"]["loss"] = json::array({3.0, 2.0, 1.0});
  attributes["validation_metrics"]["recall"] =
      json::array({{9.0, 8.0, 7.0}, {6.0, 5.0, 4.0}, {3.0, 2.0, 1.0}});

  Model model = Model::fromJson(attributes);
  EXPECT_EQ(layers, model.getLayers());
  EXPECT_EQ(loss, model.getLoss());
  EXPECT_EQ(totalEpochs, model.getTotalEpochs());
  EXPECT_EQ(trainMetrics, model.getTrainMetrics());
  EXPECT_EQ(validationMetrics, model.getValidationMetrics());
  EXPECT_EQ(classes, model.getClasses());
}

TEST_F(ModelJsonFile, TestLoad) {
  Model expected = getModel(), model = Model::load(this->expected);
  EXPECT_EQ(expected.getLayers(), model.getLayers());
  EXPECT_EQ(expected.getLoss(), model.getLoss());
  EXPECT_EQ(expected.getTotalEpochs(), model.getTotalEpochs());
  EXPECT_EQ(expected.getTrainMetrics(), model.getTrainMetrics());
  EXPECT_EQ(expected.getValidationMetrics(), model.getValidationMetrics());
  EXPECT_EQ(expected.getClasses(), model.getClasses());
}

TEST_F(ModelJsonFile, TestLoadInvalidFormat) {
  Model model = getModel();
  std::vector<std::string> extensions{".test", ".txt", ".pkl"};
  for (const std::string &extension : extensions) {
    std::filesystem::path path = this->root / ("model." + extension);
    EXPECT_THROW(model.load(path), exceptions::model::InvalidExtensionException)
        << "Exception did not throw for " << extension;
  }
}
#pragma endregion Load

#pragma region Save
TEST(Model, TestToJson) {
  Model model = getModel();
  json expected{{"class", "Model"},
                {"layers",
                 {{{"class", "Linear"},
                   {"weight", {{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}}},
                   {"bias", {1, 1, 1}},
                   {"activation_function", "NoActivation"}},
                  {{"class", "Linear"},
                   {"weight", {{1, 1, 1}, {1, 1, 1}}},
                   {"bias", {1, 1}},
                   {"activation_function", "ReLU"}}}},
                {"loss", {{"class", "CrossEntropyLoss"}, {"reduction", "sum"}}},
                {"total_epochs", 0},
                {"train_metrics", {{"loss", json::array()}}},
                {"validation_metrics", {{"loss", json::array()}}},
                {"classes", {"0", "1"}}};
  ASSERT_EQ(expected, model.toJson());
}

TEST_F(ModelJsonFile, TestSave) {
  Model model = getModel();
  std::filesystem::path savePath = this->root / "model.json";
  model.save(savePath);

  json expected, result;
  std::ifstream(this->expected) >> expected;
  std::ifstream(savePath) >> result;
  ASSERT_EQ(expected, result);
}

TEST_F(ModelJsonFile, TestSaveInvalidFormat) {
  Model model = getModel();
  std::vector<std::string> extensions{".test", ".txt", ".pkl"};
  for (const std::string &extension : extensions) {
    std::filesystem::path savePath = this->root / ("model." + extension);
    EXPECT_THROW(model.save(savePath),
                 exceptions::model::InvalidExtensionException)
        << "Exception did not throw for " << extension;
  }
}
#pragma endregion Save

#pragma region Forward pass
TEST(Model, TestForward) {
  Model model = getModel();
  Eigen::MatrixXd x = getData().first,
                  expected{{25., 25.}, {34., 34.}, {79., 79.}, {1., 1.},
                           {49., 49.}, {55., 55.}, {0., 0.},   {0., 0.},
                           {49., 49.}, {1., 1.}},
                  result = model.forward(x);

  ASSERT_TRUE(expected.isApprox(result)) << "Expected:\n"
                                         << expected << "\nGot:\n"
                                         << result;
}

TEST(Model, TestPredict) {
  Model model = getModel();
  Eigen::MatrixXd x{{0, 0, 0, 0}};
  std::vector<std::string> expected{"0"};
  ASSERT_EQ(expected, model.predict(x));
}

TEST(Model, TestPredictWithNodeClasses) {
  Model model(getLayers(), getLoss());
  Eigen::MatrixXd x{{0, 0, 0, 0}};
  EXPECT_THROW(model.predict(x), exceptions::model::MissingClassesException);
}
#pragma endregion Forward pass

#pragma region Train
TEST(Model, TestTrainNoValidation) {
  int epochs = 1;
  Model model = getModel();
  MockLoader loader(1);
  model.train(loader, 1e-4, 1, epochs);
  ASSERT_EQ(epochs, model.getTotalEpochs());

  // Check loss history
  std::vector<float> lossHistory{0.6935848934440013};
  ASSERT_EQ(lossHistory.size(), model.getTrainMetrics()["loss"].size())
      << "Train loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(std::abs(lossHistory[i] -
                         std::get<float>(model.getTrainMetrics()["loss"][i])) <
                Eigen::NumTraits<float>::dummy_precision())
        << "Train loss at index " << i
        << " does not match. Expected: " << lossHistory[i]
        << ", got: " << std::get<float>(model.getTrainMetrics()["loss"][i]);
  }
  lossHistory = {};
  ASSERT_EQ(lossHistory.size(), model.getValidationMetrics()["loss"].size())
      << "Validation loss history size does not match.";

  // Check parameters
  std::vector<Eigen::MatrixXd> weights{
      Eigen::MatrixXd{
          {0.999998755067, 1.000002096106, 1.000000522309, 0.999998187911},
          {0.999998755067, 1.000002096106, 1.000000522309, 0.999998187911},
          {0.999998755067, 1.000002096106, 1.000000522309, 0.999998187911}},
      Eigen::MatrixXd{{0.998065222731, 0.998065222731, 0.998065222731},
                      {1.001934777269, 1.001934777269, 1.001934777269}}};
  std::vector<Eigen::VectorXd> biases{
      Eigen::VectorXd{{0.9999999267982, 0.9999999267982, 0.9999999267982}},
      Eigen::VectorXd{{0.999912134981, 1.000087865019}}};
  for (int i = 0; i < weights.size(); ++i) {
    ASSERT_TRUE(weights[i].isApprox(model.getLayers()[i].getWeight()))
        << "Weights of layer " << i << " does not match.";
    ASSERT_TRUE(biases[i].isApprox(model.getLayers()[i].getBias()))
        << "Bias of layer " << i << " does not match.";
  }
}

TEST(Model, TestTrainWithValidation) {
  int epochs = 1;
  Model model = getModel();
  MockLoader loader(0.7);
  model.train(loader, 1e-4, 1, epochs);
  ASSERT_EQ(epochs, model.getTotalEpochs());

  // Check loss history
  std::vector<float> lossHistory{0.7016281735019937};
  ASSERT_EQ(lossHistory.size(), model.getTrainMetrics()["loss"].size())
      << "Train loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(std::abs(lossHistory[i] -
                         std::get<float>(model.getTrainMetrics()["loss"][i])) <
                Eigen::NumTraits<float>::dummy_precision())
        << "Train loss at index " << i
        << " does not match. Expected: " << lossHistory[i]
        << ", got: " << std::get<float>(model.getTrainMetrics()["loss"][i]);
  }
  lossHistory = {0.6748015101206345};
  ASSERT_EQ(lossHistory.size(), model.getValidationMetrics()["loss"].size())
      << "Validation loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(
        std::abs(lossHistory[i] -
                 std::get<float>(model.getValidationMetrics()["loss"][i])) <
        Eigen::NumTraits<float>::dummy_precision())
        << "Validation loss at index " << i
        << " does not match. Expected: " << lossHistory[i] << ", got: "
        << std::get<float>(model.getValidationMetrics()["loss"][i]);
  }

  // Check parameters
  std::vector<Eigen::MatrixXd> weights{
      Eigen::MatrixXd{
          {0.999999277295, 1.000000688537, 1.00000001873, 0.999997713475},
          {0.999999277295, 1.000000688537, 1.00000001873, 0.999997713475},
          {0.999999277295, 1.000000688537, 1.00000001873, 0.999997713475}},
      Eigen::MatrixXd{{0.998819881654, 0.998819881654, 0.998819881654},
                      {1.001180118346, 1.001180118346, 1.001180118346}}};
  std::vector<Eigen::VectorXd> biases{
      Eigen::VectorXd{{1.000000008979, 1.000000008979, 1.000000008979}},
      Eigen::VectorXd{{0.999909294313, 1.000090705687}}};
  for (int i = 0; i < weights.size(); ++i) {
    ASSERT_TRUE(weights[i].isApprox(model.getLayers()[i].getWeight()))
        << "Weights of layer " << i << " does not match.";
    ASSERT_TRUE(biases[i].isApprox(model.getLayers()[i].getBias()))
        << "Bias of layer " << i << " does not match.";
  }
}

TEST(Model, TestTrainWithValidationMultipleEpoch) {
  int epochs = 3;
  Model model = getModel();
  MockLoader loader(0.7);
  model.train(loader, 1e-4, 1, epochs);
  ASSERT_EQ(epochs, model.getTotalEpochs());

  // Check loss history
  std::vector<float> lossHistory{0.7016281735019942, 0.6905228054354886,
                                 0.6832302979740018};
  ASSERT_EQ(lossHistory.size(), model.getTrainMetrics()["loss"].size())
      << "Train loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(std::abs(lossHistory[i] -
                         std::get<float>(model.getTrainMetrics()["loss"][i])) <
                Eigen::NumTraits<float>::dummy_precision())
        << "Train loss at index " << i
        << " does not match. Expected: " << lossHistory[i]
        << ", got: " << std::get<float>(model.getTrainMetrics()["loss"][i]);
  }
  lossHistory = {0.6748015101206345, 0.6608838491713995, 0.6502008469335846};
  ASSERT_EQ(lossHistory.size(), model.getValidationMetrics()["loss"].size())
      << "Validation loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(
        std::abs(lossHistory[i] -
                 std::get<float>(model.getValidationMetrics()["loss"][i])) <
        Eigen::NumTraits<float>::dummy_precision())
        << "Validation loss at index " << i
        << " does not match. Expected: " << lossHistory[i] << ", got: "
        << std::get<float>(model.getValidationMetrics()["loss"][i]);
  }

  // Check parameters
  std::vector<Eigen::MatrixXd> weights{
      Eigen::MatrixXd{
          {0.999999463368, 1.000004564799, 1.000004417718, 0.999989087074},
          {0.999999463368, 1.000004564799, 1.000004417718, 0.999989087074},
          {0.999999463368, 1.000004564799, 1.000004417718, 0.999989087074}},
      Eigen::MatrixXd{{0.997116223774, 0.997116223774, 0.997116223774},
                      {1.002883776226, 1.002883776226, 1.002883776226}}};
  std::vector<Eigen::VectorXd> biases{
      Eigen::VectorXd{{1.000000421266, 1.000000421266, 1.000000421266}},
      Eigen::VectorXd{{0.999763917669, 1.000236082331}}};
  for (int i = 0; i < weights.size(); ++i) {
    ASSERT_TRUE(weights[i].isApprox(model.getLayers()[i].getWeight()))
        << "Weights of layer " << i << " does not match.";
    ASSERT_TRUE(biases[i].isApprox(model.getLayers()[i].getBias()))
        << "Bias of layer " << i << " does not match.";
  }
}

TEST(Model, TestTrainWithValidationLargerBatchSize) {
  int epochs = 1;
  Model model = getModel();
  MockLoader loader(0.7);
  model.train(loader, 1e-4, 3, epochs);
  ASSERT_EQ(epochs, model.getTotalEpochs());

  // Check loss history
  std::vector<float> lossHistory{1.62205669796401};
  ASSERT_EQ(lossHistory.size(), model.getTrainMetrics()["loss"].size())
      << "Train loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(std::abs(lossHistory[i] -
                         std::get<float>(model.getTrainMetrics()["loss"][i])) <
                Eigen::NumTraits<float>::dummy_precision())
        << "Train loss at index " << i
        << " does not match. Expected: " << lossHistory[i]
        << ", got: " << std::get<float>(model.getTrainMetrics()["loss"][i]);
  }
  lossHistory = {2.02241995571785};
  ASSERT_EQ(lossHistory.size(), model.getValidationMetrics()["loss"].size())
      << "Validation loss history size does not match.";
  for (int i = 0; i < lossHistory.size(); ++i) {
    ASSERT_TRUE(
        std::abs(lossHistory[i] -
                 std::get<float>(model.getValidationMetrics()["loss"][i])) <
        Eigen::NumTraits<float>::dummy_precision())
        << "Validation loss at index " << i
        << " does not match. Expected: " << lossHistory[i] << ", got: "
        << std::get<float>(model.getValidationMetrics()["loss"][i]);
  }

  // Check parameters
  std::vector<Eigen::MatrixXd> weights{
      Eigen::MatrixXd{
          {1.000000065579, 0.999999892849, 1.000000853661, 0.999998408936},
          {1.000000065579, 0.999999892849, 1.000000853661, 0.999998408936},
          {1.000000065579, 0.999999892849, 1.000000853661, 0.999998408936}},
      Eigen::MatrixXd{{0.998776001136, 0.998776001136, 0.998776001136},
                      {1.001223998864, 1.001223998864, 1.001223998864}}};
  std::vector<Eigen::VectorXd> biases{
      Eigen::VectorXd{{1.000000123572, 1.000000123572, 1.000000123572}},
      Eigen::VectorXd{{0.999907388883, 1.000092611117}}};
  for (int i = 0; i < weights.size(); ++i) {
    ASSERT_TRUE(weights[i].isApprox(model.getLayers()[i].getWeight()))
        << "Weights of layer " << i << " does not match.";
    ASSERT_TRUE(biases[i].isApprox(model.getLayers()[i].getBias()))
        << "Bias of layer " << i << " does not match.";
  }
}
#pragma endregion Train

#pragma region Test
TEST(Model, TestTestWithNoClasses) {
  Model model(getLayers(), getLoss());
  MockLoader loader(1);
  EXPECT_THROW(model.test(loader("train", 1)),
               exceptions::model::MissingClassesException);
}
#pragma endregion Test

#pragma region Builtins
TEST(Model, TestCall) {
  Model model = getModel();
  Eigen::MatrixXd x = getData().first,
                  expected{{25., 25.}, {34., 34.}, {79., 79.}, {1., 1.},
                           {49., 49.}, {55., 55.}, {0., 0.},   {0., 0.},
                           {49., 49.}, {1., 1.}},
                  result = model(x);

  ASSERT_TRUE(expected.isApprox(result)) << "Expected:\n"
                                         << expected << "\nGot:\n"
                                         << result;
}

TEST(Model, TestEqual) {
  Model model = getModel(), other = getModel();
  ASSERT_EQ(model, other) << "Models should be the same.";

  std::vector<linear::Linear> layers = getLayers();
  layers[0].setBias(Eigen::VectorXd::Zero(layers[0].outChannels));
  other = Model(layers, getLoss(), getKwargs());
  ASSERT_NE(model, other) << "Layers are different.";

  layers = getLayers();
  layers.emplace_back(3, 2, "ReLU");
  other = Model(layers, getLoss(), getKwargs());
  ASSERT_NE(model, other) << "Number of layers are different.";

  loss::CrossEntropyLoss loss("mean");
  other = Model(getLayers(), loss, getKwargs());
  ASSERT_NE(model, other) << "Loss is different.";
}
#pragma endregion Builtins
#pragma endregion Tests
} // namespace test_model